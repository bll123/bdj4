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
#include <assert.h>
#include <wchar.h>

#if _hdr_netinet_in
# include <netinet/in.h>
#endif
#if _hdr_winsock2
# include <winsock2.h>
#endif

#include "ati.h"
#include "atibdj4.h"
#include "audiofile.h"
#include "fileop.h"
#include "istring.h"
#include "log.h"
#include "mdebug.h"
#include "slist.h"
#include "tagdef.h"

typedef struct atisaved {
  bool                  hasdata;
  int                   filetype;
  int                   tagtype;
} atisaved_t;

void
atibdj4LogRIFFVersion (void)
{
  logMsg (LOG_DBG, LOG_INFO, "internal riff version %s", "1.0.0");
}

void
atibdj4ParseRIFFTags (atidata_t *atidata, slist_t *tagdata,
    const char *ffn, int tagtype, int *rewrite)
{
  return;
}

int
atibdj4WriteRIFFTags (atidata_t *atidata, const char *ffn,
    slist_t *updatelist, slist_t *dellist, nlist_t *datalist,
    int tagtype, int filetype)
{
  return -1;
}

atisaved_t *
atibdj4SaveRIFFTags (atidata_t *atidata,
    const char *ffn, int tagtype, int filetype)
{
  return NULL;
}

void
atibdj4FreeSavedRIFFTags (atisaved_t *atisaved, int tagtype, int filetype)
{
  return;
}

int
atibdj4RestoreRIFFTags (atidata_t *atidata,
    atisaved_t *atisaved, const char *ffn, int tagtype, int filetype)
{
  return -1;
}

void
atibdj4CleanRIFFTags (atidata_t *atidata,
    const char *ffn, int tagtype, int filetype)
{
  return;
}
