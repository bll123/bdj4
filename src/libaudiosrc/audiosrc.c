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

#include "audiosrc.h"
#include "bdj4.h"
#include "bdjstring.h"
#include "fileop.h"
#include "mdebug.h"
#include "pathbld.h"
#include "sysvars.h"

#define AUDIOSRC_FILE       "file://"
#define AUDIOSRC_YOUTUBE    "https://www.youtube.com/"
#define AUDIOSRC_YOUTUBE_S  "https://youtu.be/"
#define AUDIOSRC_YOUTUBE_M  "https://m.youtube.com/"
enum {
  AUDIOSRC_FILE_LEN = strlen (AUDIOSRC_FILE),
  AUDIOSRC_YOUTUBE_LEN = strlen (AUDIOSRC_YOUTUBE),
  AUDIOSRC_YOUTUBE_S_LEN = strlen (AUDIOSRC_YOUTUBE_S),
  AUDIOSRC_YOUTUBE_M_LEN = strlen (AUDIOSRC_YOUTUBE_M),
};

typedef struct asiter {
  int           type;
  asiterdata_t  *asidata;
} asiter_t;


int
audiosrcGetType (const char *nm)
{
  int     type = AUDIOSRC_TYPE_FILE;

  /* no point in trying to cache this, as the name comparison */
  /* is longer than the comparison done to determine the file type */

  if (fileopIsAbsolutePath (nm)) {
    type = AUDIOSRC_TYPE_FILE;
  } else if (strncmp (nm, AUDIOSRC_FILE, AUDIOSRC_FILE_LEN) == 0) {
    type = AUDIOSRC_TYPE_FILE;
  } else if (strncmp (nm, AUDIOSRC_YOUTUBE, AUDIOSRC_YOUTUBE_LEN) == 0) {
    type = AUDIOSRC_TYPE_YOUTUBE;
  } else if (strncmp (nm, AUDIOSRC_YOUTUBE_S, AUDIOSRC_YOUTUBE_S_LEN) == 0) {
    type = AUDIOSRC_TYPE_YOUTUBE;
  } else if (strncmp (nm, AUDIOSRC_YOUTUBE_M, AUDIOSRC_YOUTUBE_M_LEN) == 0) {
    type = AUDIOSRC_TYPE_YOUTUBE;
  }

  return type;
}

bool
audiosrcExists (const char *nm)
{
  int     type = AUDIOSRC_TYPE_FILE;
  bool    exists = false;


  if (nm == NULL) {
    return false;
  }

  type = audiosrcGetType (nm);

  if (type == AUDIOSRC_TYPE_FILE) {
    exists = audiosrcfileExists (nm);
  }

  return exists;
}

bool
audiosrcOriginalExists (const char *nm)
{
  int     type = AUDIOSRC_TYPE_FILE;
  bool    exists = false;


  if (nm == NULL) {
    return false;
  }

  type = audiosrcGetType (nm);

  if (type == AUDIOSRC_TYPE_FILE) {
    exists = audiosrcfileOriginalExists (nm);
  }

  return exists;
}

bool
audiosrcRemove (const char *nm)
{
  int     type = AUDIOSRC_TYPE_FILE;
  bool    rc = false;

  type = audiosrcGetType (nm);

  if (type == AUDIOSRC_TYPE_FILE) {
    rc = audiosrcfileRemove (nm);
  }

  return rc;
}

bool
audiosrcPrep (const char *sfname, char *tempnm, size_t sz)
{
  int     type = AUDIOSRC_TYPE_FILE;
  bool    rc;

  type = audiosrcGetType (sfname);

  if (type == AUDIOSRC_TYPE_FILE) {
    rc = audiosrcfilePrep (sfname, tempnm, sz);
  } else {
    /* in most cases, just return the same URI */
    strlcpy (tempnm, sfname, sz);
    rc = true;
  }

  return rc;
}

void
audiosrcPrepClean (const char *sfname, const char *tempnm)
{
  int   type = AUDIOSRC_TYPE_FILE;

  type = audiosrcGetType (sfname);

  if (type == AUDIOSRC_TYPE_FILE) {
    audiosrcfilePrepClean (tempnm);
  }
}

void
audiosrcFullPath (const char *sfname, char *fullpath, size_t sz)
{
  int     type = AUDIOSRC_TYPE_FILE;

  type = audiosrcGetType (sfname);

  *fullpath = '\0';
  if (type == AUDIOSRC_TYPE_FILE) {
    audiosrcfileFullPath (sfname, fullpath, sz);
  } else {
    strlcpy (fullpath, sfname, sz);
  }
}

asiter_t *
audiosrcStartIterator (const char *uri)
{
  asiter_t  *asiter = NULL;
  int       type;

  if (uri == NULL) {
    return NULL;
  }

  asiter = mdmalloc (sizeof (asiter_t));
  type = audiosrcGetType (uri);
  asiter->type = type;
  asiter->asidata = NULL;

  if (type == AUDIOSRC_TYPE_FILE) {
    asiter->asidata = audiosrcfileStartIterator (uri);
  }

  if (asiter->asidata == NULL) {
    mdfree (asiter);
    asiter = NULL;
  }

  return asiter;
}

void
audiosrcCleanIterator (asiter_t *asiter)
{
  if (asiter == NULL) {
    return;
  }

  if (asiter->type == AUDIOSRC_TYPE_FILE) {
    audiosrcfileCleanIterator (asiter->asidata);
    asiter->asidata = NULL;
  }
  mdfree (asiter);
}

const char *
audiosrcIterator (asiter_t *asiter)
{
  const char    *rval;

  if (asiter == NULL) {
    return NULL;
  }

  if (asiter->type == AUDIOSRC_TYPE_FILE) {
    rval = audiosrcfileIterator (asiter->asidata);
  }

  return NULL;
}
