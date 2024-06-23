/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <stdarg.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjvarsdf.h"
#include "callback.h"
#include "configui.h"
#include "dance.h"
#include "ilist.h"
#include "log.h"
#include "mdebug.h"
#include "ui.h"

/* table editing */
static bool   confuiTableMoveUp (void *udata);
static bool   confuiTableMoveDown (void *udata);
static void   confuiTableMove (confuigui_t *gui, int dir);
static bool   confuiTableRemove (void *udata);

void
confuiMakeItemTable (confuigui_t *gui, uiwcont_t *boxp, confuiident_t id,
    int flags)
{
  uiwcont_t     *vbox = NULL;
  uiwcont_t     *bvbox = NULL;
  uiwcont_t     *scwindow = NULL;
  uiwcont_t     *uiwidgetp = NULL;
  uivirtlist_t  *uivl = NULL;
  const char    *tag;

  logProcBegin ();

  switch (id) {
    case CONFUI_ID_DANCE: { tag = "conf-dance"; break; }
    case CONFUI_ID_LEVELS: { tag = "conf-level"; break; }
    case CONFUI_ID_RATINGS: { tag = "conf-rating"; break; }
    case CONFUI_ID_STATUS: { tag = "conf-status"; break; }
    default: { tag = "conf"; break; }
  }

  gui->tables [id].flags = flags;

  if (id == CONFUI_ID_DANCE ||
      id == CONFUI_ID_LEVELS ||
      id == CONFUI_ID_RATINGS ||
      id == CONFUI_ID_STATUS) {
    int   heading = VL_SHOW_HEADING;

    if (id == CONFUI_ID_DANCE) {
      heading = VL_NO_HEADING;
    }
    vbox = uiCreateVertBox ();
    uiWidgetSetAllMargins (vbox, 1);
    uiWidgetAlignHorizStart (vbox);
    uiBoxPackStartExpand (boxp, vbox);
    uivl = uiCreateVirtList (tag, vbox, 5, heading, 100);
    gui->tables [id].uivl = uivl;
    uiwcontFree (vbox);
  } else {
    scwindow = uiCreateScrolledWindow (300);
    uiWidgetExpandVert (scwindow);
    uiBoxPackStartExpand (boxp, scwindow);

    gui->tables [id].uitree = uiCreateTreeView ();

    uiWidgetSetMarginStart (gui->tables [id].uitree, 8);
    uiTreeViewEnableHeaders (gui->tables [id].uitree);
    uiWindowPackInWindow (scwindow, gui->tables [id].uitree);

    uiwcontFree (scwindow);
  }

  bvbox = uiCreateVertBox ();
  uiWidgetSetAllMargins (bvbox, 4);
  uiWidgetSetMarginTop (bvbox, 32);
  uiWidgetAlignVertStart (bvbox);
  uiBoxPackStart (boxp, bvbox);

  if ((flags & CONFUI_TABLE_NO_UP_DOWN) != CONFUI_TABLE_NO_UP_DOWN) {
    gui->tables [id].callbacks [CONFUI_TABLE_CB_UP] = callbackInit (
        confuiTableMoveUp, gui, NULL);
    uiwidgetp = uiCreateButton (
        gui->tables [id].callbacks [CONFUI_TABLE_CB_UP],
        /* CONTEXT: configuration: table edit: button: move selection up */
        _("Move Up"), "button_up");
    uiBoxPackStart (bvbox, uiwidgetp);
    gui->tables [id].buttons [CONFUI_BUTTON_TABLE_UP] = uiwidgetp;

    gui->tables [id].callbacks [CONFUI_TABLE_CB_DOWN] = callbackInit (
        confuiTableMoveDown, gui, NULL);
    uiwidgetp = uiCreateButton (
        gui->tables [id].callbacks [CONFUI_TABLE_CB_DOWN],
        /* CONTEXT: configuration: table edit: button: move selection down */
        _("Move Down"), "button_down");
    uiBoxPackStart (bvbox, uiwidgetp);
    gui->tables [id].buttons [CONFUI_BUTTON_TABLE_DOWN] = uiwidgetp;
  }

  gui->tables [id].callbacks [CONFUI_TABLE_CB_REMOVE] = callbackInit (
      confuiTableRemove, gui, NULL);
  uiwidgetp = uiCreateButton (
      gui->tables [id].callbacks [CONFUI_TABLE_CB_REMOVE],
      /* CONTEXT: configuration: table edit: button: delete selection */
      _("Delete"), "button_remove");
  uiBoxPackStart (bvbox, uiwidgetp);
  gui->tables [id].buttons [CONFUI_BUTTON_TABLE_DELETE] = uiwidgetp;

  gui->tables [id].callbacks [CONFUI_TABLE_CB_ADD] = callbackInit (
      confuiTableAdd, gui, NULL);
  uiwidgetp = uiCreateButton (
      gui->tables [id].callbacks [CONFUI_TABLE_CB_ADD],
      /* CONTEXT: configuration: table edit: button: add new selection */
      _("Add New"), "button_add");
  uiBoxPackStart (bvbox, uiwidgetp);
  gui->tables [id].buttons [CONFUI_BUTTON_TABLE_ADD] = uiwidgetp;

  uiwcontFree (bvbox);

  logProcEnd ("");
}

void
confuiTableFree (confuigui_t *gui, confuiident_t id)
{
  for (int i = 0; i < CONFUI_TABLE_CB_MAX; ++i) {
    callbackFree (gui->tables [id].callbacks [i]);
    gui->tables [id].callbacks [i] = NULL;
  }
  for (int i = 0; i < CONFUI_BUTTON_TABLE_MAX; ++i) {
    uiwcontFree (gui->tables [id].buttons [i]);
    gui->tables [id].buttons [i] = NULL;
  }
  uiwcontFree (gui->tables [id].uitree);
  gui->tables [id].uitree = NULL;
  uivlFree (gui->tables [id].uivl);
  gui->tables [id].uivl = NULL;
}

void
confuiTableSave (confuigui_t *gui, confuiident_t id)
{
  savefunc_t    savefunc;
  char          tbuff [40];

  logProcBegin ();
  if (gui->tables [id].changed == false) {
    logProcEnd ("not-changed");
    return;
  }
  if (gui->tables [id].savefunc == NULL) {
    logProcEnd ("no-savefunc");
    return;
  }

  if (gui->tables [id].listcreatefunc != NULL) {
    callback_t    *cb;

    snprintf (tbuff, sizeof (tbuff), "cu-table-save-%d", id);
    gui->tables [id].savelist = ilistAlloc (tbuff, LIST_ORDERED);
    gui->tables [id].saveidx = 0;
    cb = callbackInit (gui->tables [id].listcreatefunc, gui, NULL);
    uiTreeViewForeach (gui->tables [id].uitree, cb);
    callbackFree (cb);
  }

  savefunc = gui->tables [id].savefunc;
  if (savefunc != NULL) {
    savefunc (gui);
  }
  logProcEnd ("");
}

bool
confuiTableChanged (void *udata, int32_t col)
{
  confuigui_t   *gui = udata;

  gui->tables [gui->tablecurr].changed = true;
  return UICB_CONT;
}

bool
confuiSwitchTable (void *udata, int32_t pagenum)
{
  confuigui_t       *gui = udata;
  uiwcont_t         *uitree = NULL;
  confuiident_t     newid;

  logProcBegin ();
  if ((newid = (confuiident_t) uinbutilIDGet (gui->nbtabid, pagenum)) < 0) {
    logProcEnd ("bad-pagenum");
    return UICB_STOP;
  }

  if (gui->tablecurr == newid) {
    logProcEnd ("same-id");
    return UICB_CONT;
  }

  confuiSetStatusMsg (gui, "");

  gui->tablecurr = (confuiident_t) uinbutilIDGet (
      gui->nbtabid, pagenum);

  if (gui->tablecurr == CONFUI_ID_MOBILE_MQ) {
    confuiUpdateMobmqQrcode (gui);
  }
  if (gui->tablecurr == CONFUI_ID_REM_CONTROL) {
    confuiUpdateRemctrlQrcode (gui);
  }
  if (gui->tablecurr == CONFUI_ID_ORGANIZATION) {
    confuiUpdateOrgExamples (gui, bdjoptGetStr (OPT_G_ORGPATH));
  }
  if (gui->tablecurr == CONFUI_ID_DISP_SEL_LIST) {
    /* be sure to create the target first */
    confuiCreateTagSelectedDisp (gui);
    confuiCreateTagListingDisp (gui);
  }

  if (gui->tablecurr >= CONFUI_ID_TABLE_MAX) {
    logProcEnd ("non-table");
    return UICB_CONT;
  }

  uitree = gui->tables [gui->tablecurr].uitree;
  if (uitree != NULL) {
    uiTreeViewSelectDefault (uitree);
  }

  logProcEnd ("");
  return UICB_CONT;
}

bool
confuiTableAdd (void *udata)
{
  confuigui_t       *gui = udata;
  uiwcont_t         *uitree = NULL;
  uivirtlist_t      *uivl = NULL;
  int               count = 0;
  int               flags;
  bool              found = false;

  logProcBegin ();

  if (gui->tablecurr >= CONFUI_ID_TABLE_MAX) {
    logProcEnd ("non-table");
    return UICB_STOP;
  }

  uitree = gui->tables [gui->tablecurr].uitree;
  uivl = gui->tables [gui->tablecurr].uivl;
  if (uitree == NULL && uivl == NULL) {
    logProcEnd ("no-tree");
    return UICB_STOP;
  }

  flags = gui->tables [gui->tablecurr].flags;
  if (uitree != NULL) {
    count = uiTreeViewSelectGetCount (uitree);
  }
  if (uivl != NULL) {
    count = uivlSelectionCount (uivl);
  }
  if (count == 1) {
    found = true;
  }

  if (found) {
    int     idx;

    if (uitree != NULL) {
      idx = uiTreeViewSelectGetIndex (uitree);
    }
    if (uivl != NULL) {
      idx = uivlGetCurrSelection (uivl);
    }
    if (idx == 0 &&
        (flags & CONFUI_TABLE_KEEP_FIRST) == CONFUI_TABLE_KEEP_FIRST) {
      if (uitree != NULL) {
        if (! uiTreeViewSelectNext (uitree)) {
          found = false;
        }
      }
      if (uivl != NULL) {
        int   nidx;

        nidx = uivlMoveSelection (uivl, VL_DIR_NEXT);
        if (idx == nidx) {
          found = false;
        }
      }
    }
  }

  if (uitree != NULL) {
    if (! found) {
      uiTreeViewValueAppend (uitree);
    } else {
      uiTreeViewValueInsertBefore (uitree);
    }
  }

  switch (gui->tablecurr) {
    case CONFUI_ID_DANCE:
    case CONFUI_ID_LEVELS:
    case CONFUI_ID_RATINGS:
    case CONFUI_ID_STATUS: {
      addfunc_t   addfunc;

      addfunc = gui->tables [gui->tablecurr].addfunc;
      if (addfunc != NULL) {
        addfunc (gui);
      }
      break;
    }

    case CONFUI_ID_GENRES: {
      /* CONTEXT: configuration: genre name that is set when adding a new genre */
      confuiGenreSet (uitree, true, _("New Genre"), 0);
      break;
    }

    default: {
      break;
    }
  }

  if (uitree != NULL) {
    uiTreeViewSelectCurrent (uitree);
  }

  if (uitree != NULL) {
    gui->tables [gui->tablecurr].currcount += 1;
  }
  gui->tables [gui->tablecurr].changed = true;
  logProcEnd ("");
  return UICB_CONT;
}

/* internal routines */

/* table editing */

static bool
confuiTableMoveUp (void *udata)
{
  logProcBegin ();
  confuiTableMove (udata, CONFUI_MOVE_PREV);
  logProcEnd ("");
  return UICB_CONT;
}

static bool
confuiTableMoveDown (void *udata)
{
  logProcBegin ();
  confuiTableMove (udata, CONFUI_MOVE_NEXT);
  logProcEnd ("");
  return UICB_CONT;
}

static void
confuiTableMove (confuigui_t *gui, int dir)
{
  uiwcont_t         *uitree = NULL;
  uivirtlist_t      *uivl = NULL;
  int               count = 0;
  ilistidx_t        idx = -1;
  int               flags;

  logProcBegin ();
  flags = gui->tables [gui->tablecurr].flags;

  uitree = gui->tables [gui->tablecurr].uitree;
  uivl = gui->tables [gui->tablecurr].uivl;
  if (uitree == NULL && uivl == NULL) {
    logProcEnd ("no-tree");
    return;
  }

  if (uitree != NULL) {
    count = uiTreeViewSelectGetCount (uitree);
  }
  if (uivl != NULL) {
    count = uivlSelectionCount (uivl);
  }
  if (count != 1) {
    logProcEnd ("no-selection");
    return;
  }

  if (uitree != NULL) {
    idx = uiTreeViewSelectGetIndex (uitree);
  }
  if (uivl != NULL) {
    idx = uivlGetCurrSelection (uivl);
  }
  if (idx < 0) {
    return;
  }

  count = gui->tables [gui->tablecurr].currcount;

  if (idx == 0 && dir == CONFUI_MOVE_PREV) {
    logProcEnd ("move-prev-first");
    return;
  }
  if (idx == count - 1 && dir == CONFUI_MOVE_NEXT) {
    logProcEnd ("move-next-last");
    return;
  }
  if (idx == 1 &&
      dir == CONFUI_MOVE_PREV &&
      (flags & CONFUI_TABLE_KEEP_FIRST) == CONFUI_TABLE_KEEP_FIRST) {
    logProcEnd ("move-prev-keep-first");
    return;
  }
  if (idx == count - 1 &&
      dir == CONFUI_MOVE_PREV &&
      (flags & CONFUI_TABLE_KEEP_LAST) == CONFUI_TABLE_KEEP_LAST) {
    logProcEnd ("move-prev-keep-last");
    return;
  }
  if (idx == count - 2 &&
      dir == CONFUI_MOVE_NEXT &&
      (flags & CONFUI_TABLE_KEEP_LAST) == CONFUI_TABLE_KEEP_LAST) {
    logProcEnd ("move-next-keep-last");
    return;
  }
  if (idx == 0 &&
      dir == CONFUI_MOVE_NEXT &&
      (flags & CONFUI_TABLE_KEEP_FIRST) == CONFUI_TABLE_KEEP_FIRST) {
    logProcEnd ("move-next-keep-first");
    return;
  }

  if (uitree != NULL) {
    if (dir == CONFUI_MOVE_PREV) {
      uiTreeViewMoveBefore (uitree);
    } else {
      uiTreeViewMoveAfter (uitree);
    }
  }
  if (uivl != NULL) {
    movefunc_t    movefunc;

    movefunc = gui->tables [gui->tablecurr].movefunc;
    if (movefunc != NULL) {
      movefunc (gui, idx, dir);
    }
  }
  gui->tables [gui->tablecurr].changed = true;
  logProcEnd ("");
}

static bool
confuiTableRemove (void *udata)
{
  confuigui_t       *gui = udata;
  uiwcont_t         *uitree;
  uivirtlist_t      *uivl;
  int               idx = -1;
  int               count = 0;
  int               flags;

  logProcBegin ();

  flags = gui->tables [gui->tablecurr].flags;

  uitree = gui->tables [gui->tablecurr].uitree;
  uivl = gui->tables [gui->tablecurr].uivl;
  if (uitree == NULL && uivl == NULL) {
    return UICB_STOP;
  }

  if (uitree != NULL) {
    count = uiTreeViewSelectGetCount (uitree);
  }
  if (uivl != NULL) {
    count = uivlSelectionCount (uivl);
  }
  if (count != 1) {
    logProcEnd ("no-selection");
    return UICB_STOP;
  }

  if (uitree != NULL) {
    idx = uiTreeViewSelectGetIndex (uitree);
  }
  if (uivl != NULL) {
    idx = uivlGetCurrSelection (uivl);
  }
  if (idx < 0) {
    return UICB_STOP;
  }

  count = gui->tables [gui->tablecurr].currcount;

  if (idx == 0 &&
      (flags & CONFUI_TABLE_KEEP_FIRST) == CONFUI_TABLE_KEEP_FIRST) {
    logProcEnd ("keep-first");
    return UICB_CONT;
  }
  if (idx == count - 1 &&
      (flags & CONFUI_TABLE_KEEP_LAST) == CONFUI_TABLE_KEEP_LAST) {
    logProcEnd ("keep-last");
    return UICB_CONT;
  }

  if (uivl != NULL) {
    removefunc_t  removefunc;

    removefunc = gui->tables [gui->tablecurr].removefunc;
    if (removefunc != NULL) {
      removefunc (gui, idx);
    }

    count = gui->tables [gui->tablecurr].currcount;
    if (idx >= count) {
      uivlSetSelection (uivl, count - 1);
    }
  }
  if (uitree != NULL) {
    uiTreeViewValueRemove (uitree);
  }

  gui->tables [gui->tablecurr].changed = true;
  if (uitree != NULL) {
    gui->tables [gui->tablecurr].currcount -= 1;
  }

  logProcEnd ("");
  return UICB_CONT;
}

