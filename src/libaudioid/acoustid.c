/*
 * Copyright 2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <math.h>

#include "audioid.h"
#include "bdj4.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "fileop.h"
#include "ilist.h"
#include "log.h"
#include "mdebug.h"
#include "osprocess.h"
#include "slist.h"
#include "song.h"
#include "songutil.h"
#include "sysvars.h"
#include "tagdef.h"
#include "tmutil.h"
#include "vsencdec.h"
#include "webclient.h"

#define ACOUSTID_DEBUG 1

enum {
  ACOUSTID_BUFF_SZ = 16384,
};

typedef struct audioidacoustid {
  webclient_t   *webclient;
  const char    *webresponse;
  size_t        webresplen;
  mstime_t      globalreqtimer;
  int           respcount;
  char          akey [100];
} audioidacoustid_t;

/*
 * acoustid:
 *  for each result (score)
 *    for each recording (recording-id)
 *      for each release
 *        : (album-artist, album-title, date, disctotal)
 *        medium: (discnumber, tracktotal)
 *        track: (tracknumber, title, artist)
 */

static audioidxpath_t acoustidalbartistxp [] = {
  { AUDIOID_XPATH_DATA, AUDIOID_TYPE_JOINPHRASE, "/joinphrase", NULL, NULL },
  { AUDIOID_XPATH_DATA, TAG_ALBUMARTIST, "/name", NULL, NULL },
  { AUDIOID_XPATH_END,  AUDIOID_TYPE_TREE, "end-artist", NULL, NULL },
};

static audioidxpath_t acoustidartistxp [] = {
  { AUDIOID_XPATH_DATA, AUDIOID_TYPE_JOINPHRASE, "/joinphrase", NULL, NULL },
  { AUDIOID_XPATH_DATA, TAG_ARTIST, "/name", NULL, NULL },
  { AUDIOID_XPATH_END,  AUDIOID_TYPE_TREE, "end-artist", NULL, NULL },
};

/* relative to /response/recordings/recording/releases/release/mediums/medium/tracks/track */
static audioidxpath_t acoustidtrackxp [] = {
  { AUDIOID_XPATH_TREE, AUDIOID_TYPE_TREE, "/artists/artist", NULL, acoustidartistxp },
  { AUDIOID_XPATH_DATA, TAG_TRACKNUMBER, "/position", NULL, NULL },
  { AUDIOID_XPATH_DATA, TAG_TITLE, "/title", NULL, NULL },
  { AUDIOID_XPATH_END,  AUDIOID_TYPE_TREE, "end-track", NULL, NULL },
};

/* relative to /response/recordings/recording/releases/release/mediums/medium */
static audioidxpath_t acoustidmediumxp [] = {
  { AUDIOID_XPATH_DATA, TAG_DISCNUMBER, "/position", NULL, NULL },
  { AUDIOID_XPATH_DATA, TAG_TRACKTOTAL, "/track_count", NULL, NULL },
  { AUDIOID_XPATH_TREE, AUDIOID_TYPE_TREE, "/tracks/track", NULL, acoustidtrackxp },
  { AUDIOID_XPATH_END,  AUDIOID_TYPE_TREE, "end-medium", NULL, NULL },
};

/* relative to /response/recordings/recording/releases/release */
static audioidxpath_t acoustidreleasexp [] = {
  { AUDIOID_XPATH_TREE, AUDIOID_TYPE_TREE, "/artists/artist", NULL, acoustidalbartistxp },
  { AUDIOID_XPATH_DATA, TAG_ALBUM, "/title", NULL, NULL },
  { AUDIOID_XPATH_DATA, TAG_DATE, "/date/year", NULL, NULL },
  { AUDIOID_XPATH_DATA, AUDIOID_TYPE_MONTH, "/date/month", NULL, NULL },
  { AUDIOID_XPATH_DATA, TAG_DISCTOTAL, "/medium_count", NULL, NULL },
  { AUDIOID_XPATH_TREE, AUDIOID_TYPE_TREE, "/mediums/medium", NULL, acoustidmediumxp },
  { AUDIOID_XPATH_END,  AUDIOID_TYPE_TREE, "end-release", NULL, NULL },
};

/* relative to /response/recordings/recording */
static audioidxpath_t acoustidrecordingxp [] = {
  { AUDIOID_XPATH_DATA, TAG_RECORDING_ID, "/id", NULL, NULL },
  { AUDIOID_XPATH_TREE, AUDIOID_TYPE_RESPIDX, "/releases/release", NULL, acoustidreleasexp },
  { AUDIOID_XPATH_END,  AUDIOID_TYPE_TREE, "end-recording", NULL, NULL },
};

/* relative to /response/results/result */
static audioidxpath_t acoustidresultxp [] = {
  { AUDIOID_XPATH_DATA, TAG_AUDIOID_SCORE, "/score", NULL, NULL },
  { AUDIOID_XPATH_TREE, AUDIOID_TYPE_TREE, "/recordings/recording", NULL, acoustidrecordingxp },
  { AUDIOID_XPATH_END,  AUDIOID_TYPE_TREE, "end-result", NULL, NULL },
};

static audioidxpath_t acoustidxpaths [] = {
  { AUDIOID_XPATH_TREE, AUDIOID_TYPE_TREE, "/response/results/result", NULL, acoustidresultxp },
  { AUDIOID_XPATH_END,  AUDIOID_TYPE_TREE, "end-response", NULL, NULL },
};

static void acoustidWebResponseCallback (void *userdata, const char *resp, size_t len);
#if ACOUSTID_DEBUG
static void dumpData (audioidacoustid_t *acoustid);
#endif

audioidacoustid_t *
acoustidInit (void)
{
  audioidacoustid_t *acoustid;
  const char        *takey;

  acoustid = mdmalloc (sizeof (audioidacoustid_t));
  acoustid->webclient = webclientAlloc (acoustid, acoustidWebResponseCallback);
  acoustid->webresponse = NULL;
  acoustid->webresplen = 0;
  mstimeset (&acoustid->globalreqtimer, 0);
  acoustid->respcount = 0;
  takey = bdjoptGetStr (OPT_G_ACOUSTID_KEY);
  vsencdec (takey, acoustid->akey, sizeof (acoustid->akey));

  return acoustid;
}

void
acoustidFree (audioidacoustid_t *acoustid)
{
  if (acoustid == NULL) {
    return;
  }

  webclientClose (acoustid->webclient);
  acoustid->webclient = NULL;
  acoustid->webresponse = NULL;
  acoustid->webresplen = 0;
  mdfree (acoustid);
}

int
acoustidLookup (audioidacoustid_t *acoustid, const song_t *song,
    ilist_t *respdata)
{
  char          uri [MAXPATHLEN];
  char          *query;
  long          dur;
  double        ddur;
  char          *fpdata = NULL;
  size_t        retsz;
  const char    *fn;
  char          *ffn;
  const char    *targv [10];
  int           targc = 0;
  mstime_t      starttm;

  fn = songGetStr (song, TAG_FILE);
  ffn = songutilFullFileName (fn);
  if (! fileopFileExists (ffn)) {
    return 0;
  }

  mstimestart (&starttm);
  /* acoustid prefers at most three calls per second */
  while (! mstimeCheck (&acoustid->globalreqtimer)) {
    mssleep (10);
  }
  mstimeset (&acoustid->globalreqtimer, 334);
  logMsg (LOG_DBG, LOG_IMPORTANT, "acoustid: wait time: %" PRId64,
      (int64_t) mstimeend (&starttm));

  query = mdmalloc (ACOUSTID_BUFF_SZ);

  dur = songGetNum (song, TAG_DURATION);
  ddur = round ((double) dur / 1000.0);

  fpdata = mdmalloc (ACOUSTID_BUFF_SZ);

  targv [targc++] = sysvarsGetStr (SV_PATH_FPCALC);
  targv [targc++] = ffn;
  targv [targc++] = "-plain";
  targv [targc++] = NULL;
  mstimestart (&starttm);
  osProcessPipe (targv, OS_PROC_WAIT | OS_PROC_NOSTDERR,
      fpdata, ACOUSTID_BUFF_SZ, &retsz);
  logMsg (LOG_DBG, LOG_IMPORTANT, "acoustid: fpcalc: %" PRId64,
      (int64_t) mstimeend (&starttm));

  strlcpy (uri, sysvarsGetStr (SV_AUDIOID_ACOUSTID_URI), sizeof (uri));
  snprintf (query, ACOUSTID_BUFF_SZ,
      "client=%s"
      "&format=xml"
      "&meta=recordingids+recordings+releases+tracks+artists"
      "&duration=%.0f"
      "&fingerprint=%.*s",
      acoustid->akey,
      ddur,
      (int) retsz, fpdata
      );
  logMsg (LOG_DBG, LOG_AUDIO_ID, "audioid: acoustid: query: %s", query);


  mstimestart (&starttm);
  webclientPostCompressed (acoustid->webclient, uri, query);
  logMsg (LOG_DBG, LOG_IMPORTANT, "acoustid: web-query: %" PRId64,
      (int64_t) mstimeend (&starttm));

#if ACOUSTID_DEBUG
  dumpData (acoustid);
#endif

  if (acoustid->webresponse != NULL && acoustid->webresplen > 0) {
    mstimestart (&starttm);
    acoustid->respcount = audioidParseAll (acoustid->webresponse,
        acoustid->webresplen, acoustidxpaths, respdata);
    logMsg (LOG_DBG, LOG_IMPORTANT, "acoustid: parse: %" PRId64,
        (int64_t) mstimeend (&starttm));
  }

  dataFree (query);
  dataFree (fpdata);
  return acoustid->respcount;
}

static void
acoustidWebResponseCallback (void *userdata, const char *resp, size_t len)
{
  audioidacoustid_t   *acoustid = userdata;

  acoustid->webresponse = resp;
  acoustid->webresplen = len;
  return;
}

#if ACOUSTID_DEBUG

static void
dumpData (audioidacoustid_t *acoustid)
{
  FILE *ofh;

  if (acoustid->webresponse != NULL && acoustid->webresplen > 0) {
    ofh = fopen ("out-acoustid.xml", "w");
    if (ofh != NULL) {
      fwrite (acoustid->webresponse, 1, acoustid->webresplen, ofh);
      fprintf (ofh, "\n");
      fclose (ofh);
    }
  }
}

#endif
