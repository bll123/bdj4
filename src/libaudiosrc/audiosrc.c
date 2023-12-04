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

static int audiosrcGetType (const char *nm);

bool
audiosrcExists (const char *nm)
{
  bool  rc = false;
  int   type;


  if (nm == NULL) {
    return false;
  }

  type = audiosrcGetType (nm);

  if (type == AUDIOSRC_TYPE_FILE) {
    rc = audiosrcfileExists (nm);
  }

  return rc;
}

bool
audiosrcRemove (const char *nm)
{
  int     type;
  bool rc = false;

  type = audiosrcGetType (nm);

  if (type == AUDIOSRC_TYPE_FILE) {
    rc = audiosrcfileRemove (nm);
  }

  return rc;
}

char *
audiosrcPrep (const char *nm)
{
  int     type;
  char    *tempnm = NULL;

  type = audiosrcGetType (nm);

  if (type == AUDIOSRC_TYPE_FILE) {
    audiosrcfilePrep (nm);
  }

  return tempnm;
}

static int
audiosrcGetType (const char *nm)
{
  int     type = AUDIOSRC_TYPE_FILE;

  if (*nm == '/') {
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

