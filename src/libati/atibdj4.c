/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <libavformat/avformat.h>

#include "ati.h"
#include "atibdj4.h"
#include "audiofile.h"
#include "bdj4intl.h"
#include "fileop.h"
#include "log.h"
#include "mdebug.h"
#include "slist.h"
#include "tagdef.h"

static void atibdj4LogCallback (void *avcl, int level, const char *fmt, va_list vl);
static void atibdj4LogVersion (void);
static bool gversionlogged = false;

const char *
atiiDesc (void)
{
  /* CONTEXT: Name of the BDJ4 internal audio tagging interface */
  return _("BDJ4 Internal");
}

atidata_t *
atiiInit (const char *atipkg, int writetags,
    taglookup_t tagLookup, tagcheck_t tagCheck,
    tagname_t tagName, audiotaglookup_t audioTagLookup)
{
  atidata_t *atidata;

  atidata = mdmalloc (sizeof (atidata_t));
  atidata->writetags = writetags;
  atidata->tagLookup = tagLookup;
  atidata->tagCheck = tagCheck;
  atidata->tagName = tagName;
  atidata->audioTagLookup = audioTagLookup;
  atidata->data = NULL;

  /* turn off logging for ffmpeg */
  av_log_set_callback (atibdj4LogCallback);

  return atidata;
}

void
atiiFree (atidata_t *atidata)
{
  if (atidata != NULL) {
    mdfree (atidata);
  }
}

void
atiiSupportedTypes (int supported [])
{
  /* as of 2023-8-29 */
  supported [AFILE_TYPE_FLAC] = ATI_READ_WRITE;
  supported [AFILE_TYPE_MP3] = ATI_READ_WRITE;
  supported [AFILE_TYPE_VORBIS] = ATI_READ_WRITE;
  supported [AFILE_TYPE_OPUS] = ATI_READ_WRITE;
  supported [AFILE_TYPE_MP4] = ATI_READ_WRITE;
  supported [AFILE_TYPE_WMA] = ATI_NONE;
}

bool
atiiUseReader (void)
{
  return false;
}

char *
atiiReadTags (atidata_t *atidata, const char *ffn)
{
  return NULL;
}

void
atiiParseTags (atidata_t *atidata, slist_t *tagdata, const char *ffn,
    char *data, int filetype, int tagtype, int *rewrite)
{
  AVFormatContext   *ictx = NULL;
  char              pbuff [100];
  int32_t           duration;
  int               rc;
  bool              needduration = true;

  atibdj4LogVersion ();

  if (! fileopFileExists (ffn)) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "no file %s", ffn);
    return;
  }

  logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "parse tags %s", ffn);

  if (filetype == AFILE_TYPE_FLAC) {
    needduration = false;
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: flac");
    atibdj4ParseFlacTags (atidata, tagdata, ffn, tagtype, rewrite);
  }
  if (tagtype == TAG_TYPE_ID3) {
    /* mp3 duration is complicated, let the ffmpeg library do this */
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp3");
    atibdj4ParseMP3Tags (atidata, tagdata, ffn, tagtype, rewrite);
  }
  if (filetype == AFILE_TYPE_MP4) {
    needduration = false;
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp4");
    atibdj4ParseMP4Tags (atidata, tagdata, ffn, tagtype, rewrite);
  }
  if (filetype == AFILE_TYPE_VORBIS) {
    needduration = false;
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: ogg");
    atibdj4ParseOggTags (atidata, tagdata, ffn, tagtype, rewrite);
  }
  if (filetype == AFILE_TYPE_OPUS) {
    needduration = false;
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: opus");
    atibdj4ParseOpusTags (atidata, tagdata, ffn, tagtype, rewrite);
  }

  if (needduration) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "duration: use avformat");

    if ((rc = avformat_open_input (&ictx, ffn, NULL, NULL))) {
      logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "unable to open %d %s", rc, ffn);
      return;
    }

    if ((rc = avformat_find_stream_info (ictx, NULL)) < 0) {
      logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "unable to load stream info %d %s", rc, ffn);
      avformat_close_input (&ictx);
      return;
    }

    duration = ictx->duration;
    duration /= 1000;
    snprintf (pbuff, sizeof (pbuff), "%ld", (long) duration);
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "duration: %s", pbuff);
    slistSetStr (tagdata, atidata->tagName (TAG_DURATION), pbuff);

    avformat_close_input (&ictx);
  }

  return;
}

int
atiiWriteTags (atidata_t *atidata, const char *ffn,
    slist_t *updatelist, slist_t *dellist, nlist_t *datalist,
    int tagtype, int filetype)
{
  int         rc = -1;

  atibdj4LogVersion ();

  if (! fileopFileExists (ffn)) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "no file %s", ffn);
    return -1;
  }

  logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "write-tags: %s upd:%d del:%d", ffn, slistGetCount (updatelist), slistGetCount (dellist));

  if (filetype == AFILE_TYPE_FLAC) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: flac");
    rc = atibdj4WriteFlacTags (atidata, ffn, updatelist, dellist, datalist, tagtype, filetype);
  }
  if (tagtype == TAG_TYPE_ID3) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp3");
    rc = atibdj4WriteMP3Tags (atidata, ffn, updatelist, dellist, datalist, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_MP4) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp4");
    rc = atibdj4WriteMP4Tags (atidata, ffn, updatelist, dellist, datalist, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_VORBIS) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: ogg");
    rc = atibdj4WriteOggTags (atidata, ffn, updatelist, dellist, datalist, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_OPUS) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: opus");
    rc = atibdj4WriteOpusTags (atidata, ffn, updatelist, dellist, datalist, tagtype, filetype);
  }

  return rc;
}

atisaved_t *
atiiSaveTags (atidata_t *atidata, const char *ffn, int tagtype, int filetype)
{
  atisaved_t *atisaved = NULL;

  if (filetype == AFILE_TYPE_FLAC) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: flac");
    atisaved = atibdj4SaveFlacTags (atidata, ffn, tagtype, filetype);
  }
  if (tagtype == TAG_TYPE_ID3) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp3");
    atisaved = atibdj4SaveMP3Tags (atidata, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_MP4) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp4");
    atisaved = atibdj4SaveMP4Tags (atidata, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_VORBIS) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: ogg");
    atisaved = atibdj4SaveOggTags (atidata, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_OPUS) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: opus");
    atisaved = atibdj4SaveOpusTags (atidata, ffn, tagtype, filetype);
  }
  return atisaved;
}

void
atiiFreeSavedTags (atisaved_t *atisaved, int tagtype, int filetype)
{
  if (atisaved == NULL) {
    return;
  }

  if (filetype == AFILE_TYPE_FLAC) {
    atibdj4FreeSavedFlacTags (atisaved, tagtype, filetype);
  }
  if (tagtype == TAG_TYPE_ID3) {
    atibdj4FreeSavedMP3Tags (atisaved, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_MP4) {
    atibdj4FreeSavedMP4Tags (atisaved, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_VORBIS) {
    atibdj4FreeSavedOggTags (atisaved, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_OPUS) {
    atibdj4FreeSavedOpusTags (atisaved, tagtype, filetype);
  }
}

int
atiiRestoreTags (atidata_t *atidata, atisaved_t *atisaved,
    const char *ffn, int tagtype, int filetype)
{
  if (filetype == AFILE_TYPE_FLAC) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: flac");
    atibdj4RestoreFlacTags (atidata, atisaved, ffn, tagtype, filetype);
  }
  if (tagtype == TAG_TYPE_ID3) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp3");
    atibdj4RestoreMP3Tags (atidata, atisaved, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_MP4) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp4");
    atibdj4RestoreMP4Tags (atidata, atisaved, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_VORBIS) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: ogg");
    atibdj4RestoreOggTags (atidata, atisaved, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_OPUS) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: opus");
    atibdj4RestoreOpusTags (atidata, atisaved, ffn, tagtype, filetype);
  }
  return 0;
}

void
atiiCleanTags (atidata_t *atidata, const char *ffn, int tagtype, int filetype)
{
  if (filetype == AFILE_TYPE_FLAC) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: flac");
    atibdj4CleanFlacTags (atidata, ffn, tagtype, filetype);
  }
  if (tagtype == TAG_TYPE_ID3) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp3");
    atibdj4CleanMP3Tags (atidata, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_MP4) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: mp4");
    atibdj4CleanMP4Tags (atidata, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_VORBIS) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: ogg");
    atibdj4CleanOggTags (atidata, ffn, tagtype, filetype);
  }
  if (filetype == AFILE_TYPE_OPUS) {
    logMsg (LOG_DBG, LOG_DBUPDATE | LOG_AUDIO_TAG, "tag-type: opus");
    atibdj4CleanOpusTags (atidata, ffn, tagtype, filetype);
  }
  return;
}

/* internal routines */

/* for logging ffmpeg output */
static void
atibdj4LogCallback (void *avcl, int level, const char *fmt, va_list vl)
{
//  vfprintf (stderr, fmt, vl);
  return;
}

static void
atibdj4LogVersion (void)
{
  if (! gversionlogged) {
    atibdj4LogMP3Version ();
    atibdj4LogOggVersion ();
    atibdj4LogOpusVersion ();
    atibdj4LogFlacVersion ();
    atibdj4LogMP4Version ();
    gversionlogged = true;
  }
}
