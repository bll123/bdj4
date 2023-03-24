/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "callback.h"
#include "dance.h"
#include "filemanip.h"
#include "log.h"
#include "manageui.h"
#include "mdebug.h"
#include "pathbld.h"
#include "playlist.h"
#include "tagdef.h"
#include "ui.h"
#include "uiduallist.h"
#include "uiselectfile.h"

enum {
  MSEQ_MENU_CB_SEQ_LOAD,
  MSEQ_MENU_CB_SEQ_COPY,
  MSEQ_MENU_CB_SEQ_NEW,
  MSEQ_MENU_CB_SEQ_DELETE,
  MSEQ_CB_SEL_FILE,
  MSEQ_CB_MAX,
};

typedef struct manageseq {
  uiwcont_t       *windowp;
  nlist_t         *options;
  uimenu_t        *seqmenu;
  callback_t      *callbacks [MSEQ_CB_MAX];
  callback_t      *seqloadcb;
  callback_t      *seqnewcb;
  uiduallist_t    *seqduallist;
  uientry_t       *seqname;
  uiwcont_t       *statusMsg;
  char            *seqoldname;
  bool            seqbackupcreated : 1;
  bool            changed : 1;
  bool            inload : 1;
} manageseq_t;

static bool   manageSequenceLoad (void *udata);
static long   manageSequenceLoadCB (void *udata, const char *fn);
static bool   manageSequenceCopy (void *udata);
static bool   manageSequenceNew (void *udata);
static bool   manageSequenceDelete (void *udata);
static void   manageSetSequenceName (manageseq_t *manageseq, const char *nm);

manageseq_t *
manageSequenceAlloc (uiwcont_t *window, nlist_t *options, uiwcont_t *statusMsg)
{
  manageseq_t *manageseq;

  manageseq = mdmalloc (sizeof (manageseq_t));
  manageseq->seqduallist = NULL;
  manageseq->seqoldname = NULL;
  manageseq->seqbackupcreated = false;
  manageseq->seqmenu = uiMenuAlloc ();
  manageseq->seqname = uiEntryInit (20, 50);
  manageseq->statusMsg = statusMsg;
  manageseq->windowp = window;
  manageseq->options = options;
  manageseq->changed = false;
  manageseq->inload = false;
  manageseq->seqloadcb = NULL;
  manageseq->seqnewcb = NULL;
  for (int i = 0; i < MSEQ_CB_MAX; ++i) {
    manageseq->callbacks [i] = NULL;
  }

  manageseq->callbacks [MSEQ_CB_SEL_FILE] =
      callbackInitStr (manageSequenceLoadCB, manageseq);

  return manageseq;
}

void
manageSequenceFree (manageseq_t *manageseq)
{
  if (manageseq != NULL) {
    uiMenuFree (manageseq->seqmenu);
    uiduallistFree (manageseq->seqduallist);
    dataFree (manageseq->seqoldname);
    uiEntryFree (manageseq->seqname);
    for (int i = 0; i < MSEQ_CB_MAX; ++i) {
      callbackFree (manageseq->callbacks [i]);
    }
    mdfree (manageseq);
  }
}

void
manageSequenceSetLoadCallback (manageseq_t *manageseq, callback_t *uicb)
{
  if (manageseq == NULL) {
    return;
  }
  manageseq->seqloadcb = uicb;
}

void
manageSequenceSetNewCallback (manageseq_t *manageseq, callback_t *uicb)
{
  if (manageseq == NULL) {
    return;
  }
  manageseq->seqnewcb = uicb;
}

void
manageBuildUISequence (manageseq_t *manageseq, uiwcont_t *vboxp)
{
  uiwcont_t           *hbox;
  uiwcont_t           *uiwidgetp;
  dance_t             *dances;
  slist_t             *dancelist;

  logProcBegin (LOG_PROC, "manageBuildUISequence");

  /* edit sequences */
  uiWidgetSetAllMargins (vboxp, 2);

  hbox = uiCreateHorizBox ();
  uiBoxPackStart (vboxp, hbox);

  /* CONTEXT: sequence editor: label for sequence name */
  uiwidgetp = uiCreateColonLabel (_("Sequence"));
  uiBoxPackStart (hbox, uiwidgetp);
  uiwcontFree (uiwidgetp);

  uiEntryCreate (manageseq->seqname);
  uiEntrySetValidate (manageseq->seqname, manageValidateName,
      manageseq->statusMsg, UIENTRY_IMMEDIATE);
  uiWidgetSetClass (uiEntryGetWidgetContainer (manageseq->seqname), ACCENT_CLASS);
  /* CONTEXT: sequence editor: default name for a new sequence */
  manageSetSequenceName (manageseq, _("New Sequence"));
  uiBoxPackStart (hbox, uiEntryGetWidgetContainer (manageseq->seqname));

  manageseq->seqduallist = uiCreateDualList (vboxp,
      DUALLIST_FLAGS_MULTIPLE | DUALLIST_FLAGS_PERSISTENT,
      tagdefs [TAG_DANCE].displayname,
      /* CONTEXT: sequence editor: title for the sequence list  */
      _("Sequence"));

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  dancelist = danceGetDanceList (dances);
  uiduallistSet (manageseq->seqduallist, dancelist, DUALLIST_TREE_SOURCE);

  uiwcontFree (hbox);

  logProcEnd (LOG_PROC, "manageBuildUISequence", "");
}

uimenu_t *
manageSequenceMenu (manageseq_t *manageseq, uiwcont_t *uimenubar)
{
  uiwcont_t   *menu = NULL;
  uiwcont_t   *menuitem = NULL;

  logProcBegin (LOG_PROC, "manageSequenceMenu");
  if (! uiMenuInitialized (manageseq->seqmenu)) {
    menuitem = uiMenuAddMainItem (uimenubar,
        /* CONTEXT: sequence editor: menu selection: sequence: edit menu */
        manageseq->seqmenu, _("Edit"));
    menu = uiCreateSubMenu (menuitem);
    uiwcontFree (menuitem);

    manageseq->callbacks [MSEQ_MENU_CB_SEQ_LOAD] = callbackInit (
        manageSequenceLoad, manageseq, NULL);
    /* CONTEXT: sequence editor: menu selection: sequence: edit menu: load */
    menuitem = uiMenuCreateItem (menu, _("Load"),
        manageseq->callbacks [MSEQ_MENU_CB_SEQ_LOAD]);
    uiwcontFree (menuitem);

    manageseq->callbacks [MSEQ_MENU_CB_SEQ_NEW] = callbackInit (
        manageSequenceNew, manageseq, NULL);
    /* CONTEXT: sequence editor: menu selection: sequence: edit menu: start new sequence */
    menuitem = uiMenuCreateItem (menu, _("Start New Sequence"),
        manageseq->callbacks [MSEQ_MENU_CB_SEQ_NEW]);
    uiwcontFree (menuitem);

    manageseq->callbacks [MSEQ_MENU_CB_SEQ_COPY] = callbackInit (
        manageSequenceCopy, manageseq, NULL);
    /* CONTEXT: sequence editor: menu selection: sequence: edit menu: create copy */
    menuitem = uiMenuCreateItem (menu, _("Create Copy"),
        manageseq->callbacks [MSEQ_MENU_CB_SEQ_COPY]);
    uiwcontFree (menuitem);

    manageseq->callbacks [MSEQ_MENU_CB_SEQ_DELETE] = callbackInit (
        manageSequenceDelete, manageseq, NULL);
    /* CONTEXT: sequence editor: menu selection: sequence: edit menu: delete sequence */
    menuitem = uiMenuCreateItem (menu, _("Delete"),
        manageseq->callbacks [MSEQ_MENU_CB_SEQ_DELETE]);
    uiwcontFree (menuitem);

    uiMenuSetInitialized (manageseq->seqmenu);
    uiwcontFree (menu);
  }

  uiMenuDisplay (manageseq->seqmenu);

  logProcEnd (LOG_PROC, "manageSequenceMenu", "");
  return manageseq->seqmenu;
}

void
manageSequenceSave (manageseq_t *manageseq)
{
  sequence_t  *seq = NULL;
  slist_t     *slist;
  char        nnm [MAXPATHLEN];
  char        *name;
  bool        changed = false;

  logProcBegin (LOG_PROC, "manageSequenceSave");
  if (manageseq->seqoldname == NULL) {
    logProcEnd (LOG_PROC, "manageSequenceSave", "no-old-name");
    return;
  }

  slist = uiduallistGetList (manageseq->seqduallist);
  if (slistGetCount (slist) <= 0) {
    slistFree (slist);
    logProcEnd (LOG_PROC, "manageSequenceSave", "no-seq-data");
    return;
  }

  if (uiduallistIsChanged (manageseq->seqduallist)) {
    changed = true;
  }

  name = manageTrimName (uiEntryGetValue (manageseq->seqname));

  /* the sequence has been renamed */
  if (strcmp (manageseq->seqoldname, name) != 0) {
    playlistRename (manageseq->seqoldname, name);
    changed = true;
  }

  if (! changed) {
    mdfree (name);
    slistFree (slist);
    logProcEnd (LOG_PROC, "manageSequenceSave", "not-changed");
    return;
  }

  /* need the full name for backups */
  pathbldMakePath (nnm, sizeof (nnm),
      name, BDJ4_SEQUENCE_EXT, PATHBLD_MP_DREL_DATA);
  if (! manageseq->seqbackupcreated) {
    filemanipBackup (nnm, 1);
    manageseq->seqbackupcreated = true;
  }

  manageSetSequenceName (manageseq, name);
  seq = sequenceCreate (name);
  sequenceSave (seq, slist);
  sequenceFree (seq);

  playlistCheckAndCreate (name, PLTYPE_SEQUENCE);
  slistFree (slist);
  if (manageseq->seqloadcb != NULL) {
    callbackHandlerStr (manageseq->seqloadcb, name);
  }
  mdfree (name);
  logProcEnd (LOG_PROC, "manageSequenceSave", "");
}

/* the current sequence may be renamed or deleted. */
/* check for this and if the sequence has */
/* disappeared, reset */
void
manageSequenceLoadCheck (manageseq_t *manageseq)
{
  char    *name;

  logProcBegin (LOG_PROC, "manageSequenceLoadCheck");
  if (manageseq->seqoldname == NULL) {
    logProcEnd (LOG_PROC, "manageSequenceLoadCheck", "no-old-name");
    return;
  }

  name = manageTrimName (uiEntryGetValue (manageseq->seqname));

  if (! sequenceExists (name)) {
    /* make sure no save happens */
    dataFree (manageseq->seqoldname);
    manageseq->seqoldname = NULL;
    manageSequenceNew (manageseq);
  } else {
    if (manageseq->seqloadcb != NULL) {
      callbackHandlerStr (manageseq->seqloadcb, name);
    }
  }
  mdfree (name);
  logProcEnd (LOG_PROC, "manageSequenceLoadCheck", "");
}

void
manageSequenceLoadFile (manageseq_t *manageseq, const char *fn, int preloadflag)
{
  sequence_t  *seq = NULL;
  char        *dstr = NULL;
  nlist_t     *dancelist = NULL;
  slist_t     *tlist = NULL;
  nlistidx_t  iteridx;
  nlistidx_t  didx;

  if (manageseq->inload) {
    return;
  }
  logProcBegin (LOG_PROC, "manageSequenceLoadFile");

  logMsg (LOG_DBG, LOG_ACTIONS, "load sequence file");
  if (! sequenceExists (fn)) {
    logProcEnd (LOG_PROC, "manageSequenceLoadFile", "no-seq-file");
    return;
  }

  manageseq->inload = true;

  if (preloadflag == MANAGE_STD) {
    manageSequenceSave (manageseq);
  }

  seq = sequenceAlloc (fn);
  if (seq == NULL) {
    logProcEnd (LOG_PROC, "manageSequenceLoadFile", "null");
    manageseq->inload = false;
    return;
  }

  dancelist = sequenceGetDanceList (seq);
  tlist = slistAlloc ("temp-seq", LIST_UNORDERED, NULL);
  nlistStartIterator (dancelist, &iteridx);
  while ((didx = nlistIterateKey (dancelist, &iteridx)) >= 0) {
    dstr = nlistGetStr (dancelist, didx);
    slistSetNum (tlist, dstr, didx);
  }
  uiduallistSet (manageseq->seqduallist, tlist, DUALLIST_TREE_TARGET);
  uiduallistClearChanged (manageseq->seqduallist);
  slistFree (tlist);

  manageSetSequenceName (manageseq, fn);
  if (manageseq->seqloadcb != NULL && preloadflag == MANAGE_STD) {
    callbackHandlerStr (manageseq->seqloadcb, fn);
  }

  sequenceFree (seq);
  manageseq->seqbackupcreated = false;
  manageseq->inload = false;
  logProcEnd (LOG_PROC, "manageSequenceLoadFile", "");
}

/* internal routines */

static bool
manageSequenceLoad (void *udata)
{
  manageseq_t  *manageseq = udata;

  logProcBegin (LOG_PROC, "manageSequenceLoad");
  logMsg (LOG_DBG, LOG_ACTIONS, "= action: load sequence");
  manageSequenceSave (manageseq);
  selectFileDialog (SELFILE_SEQUENCE, manageseq->windowp, manageseq->options,
      manageseq->callbacks [MSEQ_CB_SEL_FILE]);
  logProcEnd (LOG_PROC, "manageSequenceLoad", "");
  return UICB_CONT;
}

static long
manageSequenceLoadCB (void *udata, const char *fn)
{
  manageseq_t *manageseq = udata;

  logProcBegin (LOG_PROC, "manageSequenceLoadCB");
  manageSequenceLoadFile (manageseq, fn, MANAGE_STD);
  logProcEnd (LOG_PROC, "manageSequenceLoadCB", "");
  return 0;
}

static bool
manageSequenceCopy (void *udata)
{
  manageseq_t *manageseq = udata;
  char        *oname;
  char        newname [200];

  logProcBegin (LOG_PROC, "manageSequenceCopy");
  logMsg (LOG_DBG, LOG_ACTIONS, "= action: copy sequence");
  manageSequenceSave (manageseq);

  oname = manageTrimName (uiEntryGetValue (manageseq->seqname));

  /* CONTEXT: sequence editor: the new name after 'create copy' (e.g. "Copy of DJ-2022-04") */
  snprintf (newname, sizeof (newname), _("Copy of %s"), oname);
  if (manageCreatePlaylistCopy (manageseq->statusMsg, oname, newname)) {
    manageSetSequenceName (manageseq, newname);
    manageseq->seqbackupcreated = false;
    uiduallistClearChanged (manageseq->seqduallist);
    if (manageseq->seqloadcb != NULL) {
      callbackHandlerStr (manageseq->seqloadcb, newname);
    }
  }
  mdfree (oname);
  logProcEnd (LOG_PROC, "manageSequenceCopy", "");
  return UICB_CONT;
}

static bool
manageSequenceNew (void *udata)
{
  manageseq_t *manageseq = udata;
  char        tbuff [200];
  slist_t     *tlist;

  logProcBegin (LOG_PROC, "manageSequenceNew");
  logMsg (LOG_DBG, LOG_ACTIONS, "= action: new sequence");
  manageSequenceSave (manageseq);

  /* CONTEXT: sequence editor: default name for a new sequence */
  snprintf (tbuff, sizeof (tbuff), _("New Sequence"));
  manageSetSequenceName (manageseq, tbuff);
  manageseq->seqbackupcreated = false;
  tlist = slistAlloc ("tmp-sequence", LIST_UNORDERED, NULL);
  uiduallistSet (manageseq->seqduallist, tlist, DUALLIST_TREE_TARGET);
  uiduallistClearChanged (manageseq->seqduallist);
  slistFree (tlist);
  if (manageseq->seqnewcb != NULL) {
    callbackHandler (manageseq->seqnewcb);
  }
  logProcEnd (LOG_PROC, "manageSequenceNew", "");
  return UICB_CONT;
}

static bool
manageSequenceDelete (void *udata)
{
  manageseq_t *manageseq = udata;
  char        *oname;

  logProcBegin (LOG_PROC, "manageSequenceDelete");
  logMsg (LOG_DBG, LOG_ACTIONS, "= action: delete sequence");
  oname = manageTrimName (uiEntryGetValue (manageseq->seqname));
  manageDeletePlaylist (manageseq->statusMsg, oname);
  uiduallistClearChanged (manageseq->seqduallist);
  manageSequenceNew (manageseq);
  mdfree (oname);
  logProcEnd (LOG_PROC, "manageSequenceDelete", "");
  return UICB_CONT;
}

static void
manageSetSequenceName (manageseq_t *manageseq, const char *name)
{

  logProcBegin (LOG_PROC, "manageSetSequenceName");
  dataFree (manageseq->seqoldname);
  manageseq->seqoldname = mdstrdup (name);
  uiEntrySetValue (manageseq->seqname, name);
  logProcEnd (LOG_PROC, "manageSetSequenceName", "");
}
