/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>
#include <memory.h>

#if _hdr_MacTypes
# if defined (BDJ4_MEM_DEBUG)
#  undef BDJ4_MEM_DEBUG
# endif
#endif

#include "mdebug.h"
#include "volsink.h"
#include "volume.h"

static int gvol [3] = { 30, 20, 10 };
static int gsink = 0;

void
voliDesc (char **ret, int max)
{
  int   c = 0;

  if (max < 2) {
    return;
  }

  ret [c++] = "Null Audio";
  ret [c++] = NULL;
}

void
voliDisconnect (void)
{
  return;
}

void
voliCleanup (void **udata)
{
  return;
}

int
voliProcess (volaction_t action, const char *sinkname,
    int *vol, volsinklist_t *sinklist, void **udata)
{
  if (action == VOL_HAVE_SINK_LIST) {
    return true;
  }

  if (action == VOL_GETSINKLIST) {
    sinklist->defname = mdstrdup ("no-volume");
    sinklist->sinklist = NULL;
    sinklist->count = 3;
    sinklist->sinklist = mdrealloc (sinklist->sinklist,
        sinklist->count * sizeof (volsinkitem_t));
    sinklist->sinklist [0].defaultFlag = 1;
    sinklist->sinklist [0].idxNumber = 0;
    sinklist->sinklist [0].name = mdstrdup ("no-volume");
    sinklist->sinklist [0].nmlen = strlen (sinklist->sinklist [0].name);
    sinklist->sinklist [0].description = mdstrdup ("No Volume");
    sinklist->sinklist [1].defaultFlag = 0;
    sinklist->sinklist [1].idxNumber = 1;
    sinklist->sinklist [1].name = mdstrdup ("silence");
    sinklist->sinklist [1].nmlen = strlen (sinklist->sinklist [1].name);
    sinklist->sinklist [1].description = mdstrdup ("Silence");
    sinklist->sinklist [2].defaultFlag = 0;
    sinklist->sinklist [2].idxNumber = 2;
    sinklist->sinklist [2].name = mdstrdup ("quiet");
    sinklist->sinklist [2].nmlen = strlen (sinklist->sinklist [2].name);
    sinklist->sinklist [2].description = mdstrdup ("Quiet");
    return 0;
  }

  if (action == VOL_SET_SYSTEM_DFLT) {
    if (strcmp (sinkname, "no-volume") == 0) {
      gsink = 0;
    }
    if (strcmp (sinkname, "silence") == 0) {
      gsink = 1;
    }
    if (strcmp (sinkname, "quiet") == 0) {
      gsink = 2;
    }
    return 0;
  }

  if (action == VOL_SET) {
    gvol [gsink] = *vol;
    if (gvol [gsink] < 0) { gvol [gsink] = 0; }
    if (gvol [gsink] > 100) { gvol [gsink] = 100; }
  }

  if (vol != NULL && (action == VOL_SET || action == VOL_GET)) {
    *vol = gvol [gsink];

    return *vol;
  }

  return 0;
}
