/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "bdjopt.h"
#include "log.h"
#include "mdebug.h"
#include "ui.h"
#include "callback.h"
#include "uiaudioid.h"

uiaudioid_t *
uiaudioidInit (nlist_t *options, dispsel_t *dispsel)
{
  uiaudioid_t  *uiaudioid;

  logProcBegin (LOG_PROC, "uiaudioidInit");

  uiaudioid = mdmalloc (sizeof (uiaudioid_t));

  uiaudioid->options = options;
  uiaudioid->dispsel = dispsel;
  uiaudioid->savecb = NULL;
  uiaudioid->statusMsg = NULL;

  uiaudioidUIInit (uiaudioid);

  logProcEnd (LOG_PROC, "uiaudioidInit", "");
  return uiaudioid;
}

void
uiaudioidFree (uiaudioid_t *uiaudioid)
{
  logProcBegin (LOG_PROC, "uiaudioidFree");

  if (uiaudioid != NULL) {
    uiaudioidUIFree (uiaudioid);
    mdfree (uiaudioid);
  }

  logProcEnd (LOG_PROC, "uiaudioidFree", "");
}

void
uiaudioidMainLoop (uiaudioid_t *uiaudioid)
{
  uiaudioidUIMainLoop (uiaudioid);
  return;
}

void
uiaudioidSetSaveCallback (uiaudioid_t *uiaudioid, callback_t *uicb)
{
  if (uiaudioid == NULL) {
    return;
  }

  uiaudioid->savecb = uicb;
}