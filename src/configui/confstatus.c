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
#include <stdarg.h>

#include <gtk/gtk.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "bdjvarsdf.h"
#include "configui.h"
#include "ilist.h"
#include "log.h"
#include "mdebug.h"
#include "status.h"
#include "tagdef.h"
#include "ui.h"

static bool confuiStatusListCreate (void *udata);
static void confuiStatusSave (confuigui_t *gui);

void
confuiBuildUIEditStatus (confuigui_t *gui)
{
  UIWidget      vbox;
  UIWidget      hbox;
  UIWidget      uiwidget;
  UIWidget      sg;

  logProcBegin (LOG_PROC, "confuiBuildUIEditStatus");
  uiCreateVertBox (&vbox);

  /* edit status */
  confuiMakeNotebookTab (&vbox, gui,
      /* CONTEXT: configuration: edit status table */
      _("Edit Status"), CONFUI_ID_STATUS);
  uiCreateSizeGroupHoriz (&sg);

  /* CONTEXT: configuration: status: information on how to edit a status entry */
  uiCreateLabel (&uiwidget, _("Double click on a field to edit."));
  uiBoxPackStart (&vbox, &uiwidget);

  uiCreateHorizBox (&hbox);
  uiBoxPackStartExpand (&vbox, &hbox);

  confuiMakeItemTable (gui, &hbox, CONFUI_ID_STATUS,
      CONFUI_TABLE_KEEP_FIRST | CONFUI_TABLE_KEEP_LAST);
  gui->tables [CONFUI_ID_STATUS].listcreatefunc = confuiStatusListCreate;
  gui->tables [CONFUI_ID_STATUS].savefunc = confuiStatusSave;
  confuiCreateStatusTable (gui);
  logProcEnd (LOG_PROC, "confuiBuildUIEditStatus", "");
}

void
confuiCreateStatusTable (confuigui_t *gui)
{
  ilistidx_t        iteridx;
  ilistidx_t        key;
  status_t          *status;
  uitree_t          *uitree;
  UIWidget          *uitreewidgetp;
  int               editable;

  logProcBegin (LOG_PROC, "confuiCreateStatusTable");

  status = bdjvarsdfGet (BDJVDF_STATUS);

  uitree = gui->tables [CONFUI_ID_STATUS].uitree;
  uitreewidgetp = uiTreeViewGetUIWidget (uitree);

  gui->tables [CONFUI_ID_STATUS].callbacks [CONFUI_TABLE_CB_CHANGED] =
      callbackInitLong (confuiTableChanged, gui);
  uiTreeViewSetEditedCallback (uitree,
      gui->tables [CONFUI_ID_STATUS].callbacks [CONFUI_TABLE_CB_CHANGED]);

  uiTreeViewCreateValueStore (uitree, CONFUI_STATUS_COL_MAX,
      TREE_TYPE_NUM, TREE_TYPE_STRING, TREE_TYPE_BOOLEAN, TREE_TYPE_END);

  statusStartIterator (status, &iteridx);

  editable = FALSE;
  while ((key = statusIterate (status, &iteridx)) >= 0) {
    char    *statusdisp;
    long playflag;

    statusdisp = statusGetStatus (status, key);
    playflag = statusGetPlayFlag (status, key);

    if (key == statusGetCount (status) - 1) {
      editable = FALSE;
    }

    uiTreeViewValueAppend (uitree);
    confuiStatusSet (uitree, editable, statusdisp, playflag);
    /* all cells other than the very first (Unrated) are editable */
    editable = TRUE;
    gui->tables [CONFUI_ID_STATUS].currcount += 1;
  }

  uiTreeViewAppendColumn (uitree, TREE_WIDGET_TEXT,
      TREE_COL_DISP_GROW, tagdefs [TAG_STATUS].displayname,
      TREE_COL_MODE_TEXT, CONFUI_STATUS_COL_STATUS,
      TREE_COL_MODE_EDITABLE, CONFUI_STATUS_COL_EDITABLE,
      TREE_COL_MODE_END);

  uiTreeViewAppendColumn (uitree, TREE_WIDGET_CHECKBOX,
      /* CONTEXT: configuration: status: title of the "playable" column */
      TREE_COL_DISP_NORM, _("Play?"),
      TREE_COL_MODE_ACTIVE, CONFUI_STATUS_COL_PLAY_FLAG,
      TREE_COL_MODE_END);

  logProcEnd (LOG_PROC, "confuiCreateStatusTable", "");
}

static bool
confuiStatusListCreate (void *udata)
{
  confuigui_t *gui = udata;
  char        *statusdisp;
  int         playflag;

  logProcBegin (LOG_PROC, "confuiStatusListCreate");
  statusdisp = uiTreeViewGetValueStr (gui->tables [CONFUI_ID_STATUS].uitree,
      CONFUI_STATUS_COL_STATUS);
  playflag = uiTreeViewGetValue (gui->tables [CONFUI_ID_STATUS].uitree,
      CONFUI_STATUS_COL_PLAY_FLAG);
  ilistSetStr (gui->tables [CONFUI_ID_STATUS].savelist,
      gui->tables [CONFUI_ID_STATUS].saveidx, STATUS_STATUS, statusdisp);
  ilistSetNum (gui->tables [CONFUI_ID_STATUS].savelist,
      gui->tables [CONFUI_ID_STATUS].saveidx, STATUS_PLAY_FLAG, playflag);
  gui->tables [CONFUI_ID_STATUS].saveidx += 1;
  dataFree (statusdisp);
  logProcEnd (LOG_PROC, "confuiStatusListCreate", "");
  return FALSE;
}

static void
confuiStatusSave (confuigui_t *gui)
{
  status_t    *status;

  logProcBegin (LOG_PROC, "confuiStatusSave");
  status = bdjvarsdfGet (BDJVDF_STATUS);
  statusSave (status, gui->tables [CONFUI_ID_STATUS].savelist);
  ilistFree (gui->tables [CONFUI_ID_STATUS].savelist);
  logProcEnd (LOG_PROC, "confuiStatusSave", "");
}

