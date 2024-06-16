/*
 * Copyright 2024 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "callback.h"
#include "mdebug.h"
#include "nlist.h"
#include "ui.h"
#include "uivirtlist.h"
#include "uiwcont.h"

/* initialization states */
enum {
  VL_INIT_NONE = 0,
  VL_INIT_BASIC = 1,
  VL_INIT_ROWS = 3,
  VL_INIT_DISP = 4,
};

enum {
  VL_ROW_UNKNOWN = -1,
  VL_ROW_HEADING = -2,
  VL_MAX_WIDTH_ANY = -1,
};

enum {
  VL_W_HEADBOX,
  VL_W_EVENT_BOX,
  VL_W_HBOX,
  VL_W_VBOX,
  VL_W_SB,
  VL_W_SB_SZGRP,
  VL_W_FILLER,
  VL_W_KEYH,
  VL_W_MAX,
};

enum {
  VL_CB_SB,
  VL_CB_KEY,
  VL_CB_MBUTTON,
  VL_CB_SCROLL,
  VL_CB_ENTER,
  VL_CB_VBOX_SZ_CHG,
  VL_CB_ROW_SZ_CHG,
  VL_CB_MAX,
};

enum {
  VL_IDENT_COLDATA  = 0x766c636f6c646174,
  VL_IDENT_COL      = 0x766c636f6caabbcc,
  VL_IDENT_ROW      = 0x766c726f77aabbcc,
  VL_IDENT          = 0x766caabbccddeeff,
};

static const char * const VL_SELECTED_CLASS = "bdj-selected";

typedef struct {
  uiwcont_t *szgrp;
  uint64_t  ident;
  /* the following data is specific to a column */
  vltype_t  type;
  int       colident;
  int       minwidth;
  bool      alignend: 1;
  bool      hidden : 1;
  bool      ellipsize : 1;
} uivlcoldata_t;

typedef struct {
  uint64_t  ident;
  uiwcont_t *uiwidget;
  /* class needs to be held temporarily so it can be removed */
  char      *class;
} uivlcol_t;

typedef struct {
  uint64_t    ident;
  uiwcont_t   *hbox;
  uivlcol_t   *cols;
  bool        hidden : 1;
  bool        selected : 1;       // a temporary flag to ease processing
  bool        initialized : 1;
} uivlrow_t;

typedef struct uivirtlist {
  uint64_t      ident;
  uiwcont_t     *wcont [VL_W_MAX];
  callback_t    *callbacks [VL_CB_MAX];
  int           numcols;
  uivlcoldata_t *coldata;
  uivlrow_t     *rows;
  uivlrow_t     headingrow;
  int           disprows;
  int           allocrows;
  int           vboxheight;
  int           rowheight;
  int32_t       numrows;
  int32_t       rowoffset;
  nlist_t       *selected;
  int           initialized;
  /* user callbacks */
  uivlfillcb_t  fillcb;
  uivlselcb_t   selcb;
  uivlchangecb_t changecb;
  void          *udata;
  /* flags */
  bool          inscroll : 1;
} uivirtlist_t;

static void uivlFreeRow (uivirtlist_t *vl, uivlrow_t *row);
static void uivlFreeCol (uivlcol_t *col);
static void uivlInitRow (uivirtlist_t *vl, uivlrow_t *row, bool isheading);
static uivlrow_t * uivlGetRow (uivirtlist_t *vl, int32_t rownum);
static void uivlPackRow (uivirtlist_t *vl, uivlrow_t *row);
static bool uivlScrollbarCallback (void *udata, double value);
static void uivlPopulate (uivirtlist_t *vl);
static bool uivlKeyEvent (void *udata);
static bool uivlButtonEvent (void *udata, int32_t dispidx, int32_t colnum);
static bool uivlScrollEvent (void *udata, int32_t dir);
static bool uivlEnterLeaveEvent (void *udata, int32_t el);
static void uivlClearDisplaySelections (uivirtlist_t *vl);
static void uivlSetDisplaySelections (uivirtlist_t *vl);
static void uivlClearSelections (uivirtlist_t *vl);
static void uivlAddSelection (uivirtlist_t *vl, uint32_t rownum);
static void uivlProcessScroll (uivirtlist_t *vl, int32_t start);
static bool uivlVboxSizeChg (void *udata, int32_t width, int32_t height);
static bool uivlRowSizeChg (void *udata, int32_t width, int32_t height);
static void uivlRowBasicInit (uivlrow_t *row);

uivirtlist_t *
uiCreateVirtList (uiwcont_t *boxp, int disprows)
{
  uivirtlist_t  *vl;

  vl = mdmalloc (sizeof (uivirtlist_t));
  vl->ident = VL_IDENT;
  vl->fillcb = NULL;
  vl->selcb = NULL;
  vl->changecb = NULL;
  vl->udata = NULL;
  vl->inscroll = false;
  vl->vboxheight = -1;
  vl->rowheight = -1;

  for (int i = 0; i < VL_W_MAX; ++i) {
    vl->wcont [i] = NULL;
  }

  vl->wcont [VL_W_KEYH] = uiEventAlloc ();
  vl->callbacks [VL_CB_KEY] = callbackInit (uivlKeyEvent, vl, NULL);
  vl->callbacks [VL_CB_MBUTTON] = callbackInitII (uivlButtonEvent, vl);
  vl->callbacks [VL_CB_SCROLL] = callbackInitI (uivlScrollEvent, vl);
  vl->callbacks [VL_CB_ENTER] = callbackInitI (uivlEnterLeaveEvent, vl);
  vl->callbacks [VL_CB_VBOX_SZ_CHG] = callbackInitII (uivlVboxSizeChg, vl);
  vl->callbacks [VL_CB_ROW_SZ_CHG] = callbackInitII (uivlRowSizeChg, vl);

  vl->wcont [VL_W_HEADBOX] = uiCreateHorizBox ();
  uiWidgetAlignVertStart (vl->wcont [VL_W_HEADBOX]);
  uiWidgetAlignHorizFill (vl->wcont [VL_W_HEADBOX]);
//  uiWidgetExpandHoriz (vl->wcont [VL_W_HEADBOX]);
  uiBoxPackStartExpand (boxp, vl->wcont [VL_W_HEADBOX]);

  vl->wcont [VL_W_HBOX] = uiCreateHorizBox ();
  uiWidgetAlignHorizFill (vl->wcont [VL_W_HBOX]);
  uiWidgetAlignVertFill (vl->wcont [VL_W_HBOX]);
//  uiWidgetExpandHoriz (vl->wcont [VL_W_HBOX]);
//  uiWidgetExpandVert (vl->wcont [VL_W_HBOX]);
  uiBoxPackStartExpand (boxp, vl->wcont [VL_W_HBOX]);

  vl->wcont [VL_W_VBOX] = uiCreateVertBox ();
  uiWidgetAlignHorizFill (vl->wcont [VL_W_VBOX]);
  uiWidgetAlignVertFill (vl->wcont [VL_W_VBOX]);
//  uiWidgetExpandHoriz (vl->wcont [VL_W_VBOX]);
//  uiWidgetExpandVert (vl->wcont [VL_W_VBOX]);
  uiWidgetEnableFocus (vl->wcont [VL_W_VBOX]);    // for keyboard events

  vl->wcont [VL_W_EVENT_BOX] = uiEventCreateEventBox (vl->wcont [VL_W_VBOX]);
  uiWidgetAlignHorizFill (vl->wcont [VL_W_EVENT_BOX]);
  uiWidgetAlignVertFill(vl->wcont [VL_W_EVENT_BOX]);
//  uiWidgetExpandHoriz (vl->wcont [VL_W_EVENT_BOX]);
//  uiWidgetExpandVert (vl->wcont [VL_W_EVENT_BOX]);
  uiBoxPackStartExpand (vl->wcont [VL_W_HBOX], vl->wcont [VL_W_EVENT_BOX]);

  uiBoxSetSizeChgCallback (vl->wcont [VL_W_VBOX], vl->callbacks [VL_CB_VBOX_SZ_CHG]);

  vl->wcont [VL_W_SB_SZGRP] = uiCreateSizeGroupHoriz ();
  vl->wcont [VL_W_SB] = uiCreateVerticalScrollbar (10.0);
  uiSizeGroupAdd (vl->wcont [VL_W_SB_SZGRP], vl->wcont [VL_W_SB]);
  uiBoxPackEnd (vl->wcont [VL_W_HBOX], vl->wcont [VL_W_SB]);

  vl->callbacks [VL_CB_SB] = callbackInitD (uivlScrollbarCallback, vl);
  uiScrollbarSetPageIncrement (vl->wcont [VL_W_SB], floor ((double) disprows / 2));
  uiScrollbarSetStepIncrement (vl->wcont [VL_W_SB], 1.0);
  uiScrollbarSetPosition (vl->wcont [VL_W_SB], 0.0);
  uiScrollbarSetUpper (vl->wcont [VL_W_SB], (double) disprows);
  uiScrollbarSetChangeCallback (vl->wcont [VL_W_SB], vl->callbacks [VL_CB_SB]);

  vl->wcont [VL_W_FILLER] = uiCreateLabel ("");
  uiSizeGroupAdd (vl->wcont [VL_W_SB_SZGRP], vl->wcont [VL_W_FILLER]);

  vl->coldata = NULL;
  vl->rows = NULL;
  vl->numcols = 0;
  vl->numrows = 0;
  vl->rowoffset = 0;
  vl->selected = nlistAlloc ("vl-selected", LIST_ORDERED, NULL);
  /* default selection */
  nlistSetNum (vl->selected, 0, 1);
  vl->initialized = VL_INIT_NONE;

fprintf (stderr, "init: disprows: %d\n", disprows);
  vl->disprows = disprows;
  vl->allocrows = disprows;
  vl->rows = mdmalloc (sizeof (uivlrow_t) * vl->allocrows);
  for (int dispidx = 0; dispidx < disprows; ++dispidx) {
    uivlRowBasicInit (&vl->rows [dispidx]);
  }

  uivlRowBasicInit (&vl->headingrow);

  uiEventSetKeyCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_VBOX],
      vl->callbacks [VL_CB_KEY]);
  uiEventSetButtonCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_EVENT_BOX],
      vl->callbacks [VL_CB_MBUTTON]);
  uiEventSetScrollCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_EVENT_BOX],
      vl->callbacks [VL_CB_SCROLL]);
  /* the vbox does not receive an enter/leave event */
  /* (as it is overlaid presumably) */
  /* set the enter/leave on the main event-box, and the handler */
  /* grabs the focus on to the vbox so the keyboard events work */
  uiEventSetEnterLeaveCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_EVENT_BOX],
      vl->callbacks [VL_CB_ENTER]);

  return vl;
}

void
uivlFree (uivirtlist_t *vl)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }

  uivlFreeRow (vl, &vl->headingrow);

  for (int dispidx = 0; dispidx < vl->allocrows; ++dispidx) {
    uivlFreeRow (vl, &vl->rows [dispidx]);
  }
  dataFree (vl->rows);

  for (int colidx = 0; colidx < vl->numcols; ++colidx) {
    uiwcontFree (vl->coldata [colidx].szgrp);
  }
  dataFree (vl->coldata);

  nlistFree (vl->selected);

  for (int i = 0; i < VL_W_MAX; ++i) {
    uiwcontFree (vl->wcont [i]);
  }

  vl->ident = 0;
  mdfree (vl);
}

void
uivlSetNumRows (uivirtlist_t *vl, uint32_t numrows)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }

  vl->numrows = numrows;
  uiScrollbarSetUpper (vl->wcont [VL_W_SB], (double) numrows);
}

void
uivlSetNumColumns (uivirtlist_t *vl, int numcols)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }

  vl->numcols = numcols;
  vl->coldata = mdmalloc (sizeof (uivlcoldata_t) * numcols);
  for (int colidx = 0; colidx < numcols; ++colidx) {
    vl->coldata [colidx].szgrp = uiCreateSizeGroupHoriz ();
    vl->coldata [colidx].ident = VL_IDENT_COLDATA;
    vl->coldata [colidx].type = VL_TYPE_LABEL;
    vl->coldata [colidx].colident = 0;
    vl->coldata [colidx].minwidth = VL_MAX_WIDTH_ANY;
    vl->coldata [colidx].hidden = VL_COL_SHOW;
    vl->coldata [colidx].ellipsize = false;
  }

  vl->initialized = VL_INIT_BASIC;
}

void
uivlSetHeadingClass (uivirtlist_t *vl, int colnum, const char *class)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
}

/* column set */

void
uivlSetColumnHeading (uivirtlist_t *vl, int colnum, const char *heading)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  if (vl->initialized < VL_INIT_ROWS) {
fprintf (stderr, "head-row-init\n");
    uivlInitRow (vl, &vl->headingrow, true);

    for (int dispidx = 0; dispidx < vl->disprows; ++dispidx) {
      uivlrow_t *row;

      row = &vl->rows [dispidx];
fprintf (stderr, "row-init: %d\n", dispidx);
      uivlInitRow (vl, row, false);
    }
    vl->initialized = VL_INIT_ROWS;
  }

  uivlSetColumnValue (vl, VL_ROW_HEADING, colnum, heading);
}

void
uivlSetColumn (uivirtlist_t *vl, int colnum, vltype_t type, int colident, bool hidden)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  vl->coldata [colnum].type = type;
  vl->coldata [colnum].colident = colident;
  vl->coldata [colnum].hidden = hidden;
  vl->coldata [colnum].alignend = false;
}

void
uivlSetColumnMinWidth (uivirtlist_t *vl, int colnum, int minwidth)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  vl->coldata [colnum].minwidth = minwidth;
}

void
uivlSetColumnEllipsizeOn (uivirtlist_t *vl, int colnum)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  vl->coldata [colnum].ellipsize = true;
}

void
uivlSetColumnAlignEnd (uivirtlist_t *vl, int colnum)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  vl->coldata [colnum].alignend = true;
}

void
uivlSetColumnClass (uivirtlist_t *vl, int32_t rownum, int colnum, const char *class)
{
  uivlrow_t *row = NULL;
  uivlcol_t *col = NULL;

  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }
  if (rownum != VL_ROW_HEADING && (rownum < 0 || rownum >= vl->numrows)) {
    return;
  }

  row = uivlGetRow (vl, rownum);
  if (row == NULL) {
    return;
  }

  col = &row->cols [colnum];
  dataFree (col->class);
  col->class = mdstrdup (class);    // save for removal process
  uiWidgetSetClass (col->uiwidget, class);
}

void
uivlSetColumnValue (uivirtlist_t *vl, int32_t rownum, int colnum, const char *value)
{
  uivlrow_t       *row = NULL;

  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }
  if (rownum != VL_ROW_HEADING && (rownum < 0 || rownum >= vl->numrows)) {
    return;
  }

  row = uivlGetRow (vl, rownum);
  if (row == NULL) {
    return;
  }

  switch (vl->coldata [colnum].type) {
    case VL_TYPE_LABEL: {
      uiLabelSetText (row->cols [colnum].uiwidget, value);
      break;
    }
    case VL_TYPE_IMAGE: {
      break;
    }
    case VL_TYPE_ENTRY: {
      break;
    }
    case VL_TYPE_INT_NUMERIC:
    case VL_TYPE_RADIO_BUTTON:
    case VL_TYPE_CHECK_BUTTON:
    case VL_TYPE_SPINBOX_NUM: {
      /* not handled here */
      break;
    }
  }
}

void
uivlSetColumnValueNum (uivirtlist_t *vl, int32_t rownum, int colnum, int32_t val)
{
  uivlrow_t       *row = NULL;

  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (rownum != VL_ROW_HEADING && (rownum < 0 || rownum >= vl->numrows)) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  row = uivlGetRow (vl, rownum);
  if (row == NULL) {
    return;
  }

  switch (vl->coldata [colnum].type) {
    case VL_TYPE_LABEL:
    case VL_TYPE_IMAGE:
    case VL_TYPE_ENTRY: {
      /* not handled here */
      break;
    }
    case VL_TYPE_INT_NUMERIC: {
      break;
    }
    case VL_TYPE_RADIO_BUTTON: {
      break;
    }
    case VL_TYPE_CHECK_BUTTON: {
      break;
    }
    case VL_TYPE_SPINBOX_NUM: {
      break;
    }
  }
}

/* column get */

int
uivlGetColumnIdent (uivirtlist_t *vl, int colnum)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return VL_COL_UNKNOWN;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return VL_COL_UNKNOWN;
  }

  return 0;
}

const char *
uivlGetColumnValue (uivirtlist_t *vl, int row, int colnum)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return NULL;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return NULL;
  }
  return NULL;
}

const char *
uivlGetColumnEntryValue (uivirtlist_t *vl, int row, int colnum)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return NULL;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return NULL;
  }
  return NULL;
}

/* callbacks */

void
uivlSetSelectionCallback (uivirtlist_t *vl, uivlselcb_t cb, void *udata)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
}

void
uivlSetChangeCallback (uivirtlist_t *vl, uivlchangecb_t cb, void *udata)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
}

void
uivlSetRowFillCallback (uivirtlist_t *vl, uivlfillcb_t cb, void *udata)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (cb == NULL) {
    return;
  }

  vl->fillcb = cb;
  vl->udata = udata;
}

/* processing */

/* the initial display */
void
uivlDisplay (uivirtlist_t *vl)
{
  uivlrow_t   *row;

  uiBoxPackEnd (vl->headingrow.hbox, vl->wcont [VL_W_FILLER]);
  uiBoxPackStartExpand (vl->wcont [VL_W_HEADBOX], vl->headingrow.hbox);

fprintf (stderr, "disp-pop\n");
  uivlPopulate (vl);

  for (int dispidx = 0; dispidx < vl->disprows; ++dispidx) {
    row = &vl->rows [dispidx];
fprintf (stderr, "disp-pack: %d\n", dispidx);
    uivlPackRow (vl, row);
    if (dispidx == 0) {
      uiBoxSetSizeChgCallback (row->hbox, vl->callbacks [VL_CB_ROW_SZ_CHG]);
    }
  }

  vl->initialized = VL_INIT_DISP;
  uiScrollbarSetPosition (vl->wcont [VL_W_SB], 0.0);
}

/* internal routines */

static void
uivlFreeRow (uivirtlist_t *vl, uivlrow_t *row)
{
  if (row == NULL || row->ident != VL_IDENT_ROW) {
    return;
  }

  for (int colidx = 0; colidx < vl->numcols; ++colidx) {
    if (row->cols != NULL) {
      uivlFreeCol (&row->cols [colidx]);
    }
  }
  uiwcontFree (row->hbox);
  dataFree (row->cols);
  row->ident = 0;
}

static void
uivlFreeCol (uivlcol_t *col)
{
  if (col == NULL || col->ident != VL_IDENT_COL) {
    return;
  }

  dataFree (col->class);
  col->class = NULL;
  uiwcontFree (col->uiwidget);
  col->uiwidget = NULL;
  col->ident = 0;
}

static void
uivlInitRow (uivirtlist_t *vl, uivlrow_t *row, bool isheading)
{
  if (row == NULL) {
    return;
  }
  if (row->initialized) {
    return;
  }

  row->hbox = uiCreateHorizBox ();
  uiWidgetAlignHorizFill (row->hbox);
  uiWidgetAlignVertStart (row->hbox);

  row->cols = mdmalloc (sizeof (uivlcol_t) * vl->numcols);

  for (int colidx = 0; colidx < vl->numcols; ++colidx) {
    row->cols [colidx].ident = VL_IDENT_COL;
    row->cols [colidx].uiwidget = uiCreateLabel ("");
    uiWidgetSetMarginEnd (row->cols [colidx].uiwidget, 3);
    uiWidgetAlignHorizFill (row->cols [colidx].uiwidget);

    if (vl->coldata [colidx].hidden == VL_COL_SHOW) {
      uiBoxPackStartExpand (row->hbox, row->cols [colidx].uiwidget);
      uiSizeGroupAdd (vl->coldata [colidx].szgrp, row->cols [colidx].uiwidget);
    }
    if (vl->coldata [colidx].type == VL_TYPE_LABEL) {
      if (vl->coldata [colidx].minwidth != VL_MAX_WIDTH_ANY) {
        uiLabelSetMinWidth (row->cols [colidx].uiwidget, vl->coldata [colidx].minwidth);
      }
      if (! isheading && vl->coldata [colidx].ellipsize) {
        uiLabelEllipsizeOn (row->cols [colidx].uiwidget);
      }
      if (vl->coldata [colidx].alignend) {
        uiLabelAlignEnd (row->cols [colidx].uiwidget);
      }
    }
    row->cols [colidx].class = NULL;
  }
  row->hidden = false;
  row->selected = false;
  row->initialized = true;
}

static uivlrow_t *
uivlGetRow (uivirtlist_t *vl, int32_t rownum)
{
  uivlrow_t   *row = NULL;

  if (rownum == VL_ROW_HEADING) {
    row = &vl->headingrow;
  } else {
    int32_t   rowidx;

    rowidx = rownum - vl->rowoffset;
    if (rowidx >= 0 && rowidx < vl->disprows) {
      row = &vl->rows [rowidx];
      if (row->ident != VL_IDENT_ROW) {
        fprintf (stderr, "ERR: invalid row: rownum %" PRId32 " rowoffset: %" PRId32 " rowidx: %" PRId32 "\n", rownum, vl->rowoffset, rowidx);
      }
    }
  }

  return row;
}

static void
uivlPackRow (uivirtlist_t *vl, uivlrow_t *row)
{
  if (row == NULL) {
    return;
  }

  uiBoxPackStartExpand (vl->wcont [VL_W_VBOX], row->hbox);
  /* rows packed after the initial display need */
  /* to have their contents shown */
  uiWidgetShowAll (row->hbox);
  row->hidden = false;
}

static bool
uivlScrollbarCallback (void *udata, double value)
{
  uivirtlist_t  *vl = udata;
  int32_t       start;

  if (vl->inscroll) {
    return UICB_STOP;
  }

  start = (uint32_t) floor (value);

  uivlProcessScroll (vl, start);

  return UICB_CONT;
}

static void
uivlPopulate (uivirtlist_t *vl)
{
  for (int dispidx = vl->disprows; dispidx < vl->allocrows; ++dispidx) {
    vl->rows [dispidx].hidden = true;
    uiWidgetHide (vl->rows [dispidx].hbox);
  }

  for (int dispidx = 0; dispidx < vl->disprows; ++dispidx) {
    uivlrow_t   *row;
    uivlcol_t   *col;

    row = &vl->rows [dispidx];
fprintf (stderr, "pop: %d %p\n", dispidx, row);

    for (int colidx = 0; colidx < vl->numcols; ++colidx) {
      col = &row->cols [colidx];
      if (col->class != NULL) {
        uiWidgetRemoveClass (col->uiwidget, col->class);
        dataFree (col->class);
        col->class = NULL;
      }
    }
  }

  for (int dispidx = 0; dispidx < vl->disprows; ++dispidx) {
    uivlrow_t   *row;

    row = &vl->rows [dispidx];
    if (vl->fillcb != NULL) {
      vl->fillcb (vl->udata, vl, dispidx + vl->rowoffset);
    }

    if (row->hidden) {
      row->hidden = false;
      uiWidgetShow (row->hbox);
    }
  }

  uivlClearDisplaySelections (vl);
  uivlSetDisplaySelections (vl);
}

static bool
uivlKeyEvent (void *udata)
{
  uivirtlist_t  *vl = udata;

  if (vl == NULL) {
    return UICB_CONT;
  }
  if (vl->inscroll) {
    return UICB_CONT;
  }

  if (uiEventIsMovementKey (vl->wcont [VL_W_KEYH])) {
    int32_t     start;
    int32_t     offset = 1;

    start = vl->rowoffset;

    if (uiEventIsKeyPressEvent (vl->wcont [VL_W_KEYH])) {
      if (uiEventIsPageUpDownKey (vl->wcont [VL_W_KEYH])) {
        offset = vl->disprows;
      }
      if (uiEventIsUpKey (vl->wcont [VL_W_KEYH])) {
        start -= offset;
      }
      if (uiEventIsDownKey (vl->wcont [VL_W_KEYH])) {
        start += offset;
      }

      uivlProcessScroll (vl, start);
    }

    /* movement keys are handled internally */
    return UICB_STOP;
  }

  return UICB_CONT;
}

static bool
uivlButtonEvent (void *udata, int32_t dispidx, int32_t colnum)
{
  uivirtlist_t  *vl = udata;
  int           button;
  int32_t       rownum = -1;

  if (vl == NULL) {
    return UICB_CONT;
  }
  if (vl->inscroll) {
    return UICB_CONT;
  }

  if (! uiEventIsButtonPressEvent (vl->wcont [VL_W_KEYH])) {
    return UICB_CONT;
  }

  button = uiEventButtonPressed (vl->wcont [VL_W_KEYH]);

  /* button 4 and 5 cause a single scroll event */
  if (button == UIEVENT_BUTTON_4 || button == UIEVENT_BUTTON_5) {
    int32_t     start;

    start = vl->rowoffset;

    if (button == UIEVENT_BUTTON_4) {
      start -= 1;
    }
    if (button == UIEVENT_BUTTON_5) {
      start += 1;
    }

    uivlProcessScroll (vl, start);
    return UICB_CONT;
  }

  /* all other buttons (1-3) cause a selection */

  if (dispidx >= 0) {
    rownum = dispidx + vl->rowoffset;
  }

  if (rownum < 0) {
    /* not found */
    return UICB_CONT;
  }

  if (! uiEventIsControlPressed (vl->wcont [VL_W_KEYH])) {
    uivlClearSelections (vl);
    uivlClearDisplaySelections (vl);
  }
  uivlAddSelection (vl, rownum);
  uivlSetDisplaySelections (vl);

  return UICB_CONT;
}

static bool
uivlScrollEvent (void *udata, int32_t dir)
{
  uivirtlist_t  *vl = udata;
  int32_t       start;

  if (vl == NULL) {
    return UICB_CONT;
  }
  if (vl->inscroll) {
    return UICB_CONT;
  }

  start = vl->rowoffset;
  if (dir == UIEVENT_DIR_PREV || dir == UIEVENT_DIR_LEFT) {
    start -= 1;
  }
  if (dir == UIEVENT_DIR_NEXT || dir == UIEVENT_DIR_RIGHT) {
    start += 1;
  }
  uivlProcessScroll (vl, start);

  return UICB_CONT;
}

static bool
uivlEnterLeaveEvent (void *udata, int32_t el)
{
  uivirtlist_t  *vl = udata;

  if (el == UIEVENT_EV_ENTER) {
    uiWidgetGrabFocus (vl->wcont [VL_W_VBOX]);
  }
  return UICB_CONT;
}

static void
uivlClearDisplaySelections (uivirtlist_t *vl)
{
  for (int dispidx = 0; dispidx < vl->disprows; ++dispidx) {
    uivlrow_t   *row = &vl->rows [dispidx];

    if (row->selected) {
      uiWidgetRemoveClass (row->hbox, "bdj-selected");
      for (int colidx = 0; colidx < vl->numcols; ++colidx) {
        uiWidgetRemoveClass (row->cols [colidx].uiwidget, "bdj-selected");
      }
      row->selected = false;
    }
  }
}

static void
uivlSetDisplaySelections (uivirtlist_t *vl)
{
  nlistidx_t    iter;
  nlistidx_t    val;

  nlistStartIterator (vl->selected, &iter);
  while ((val = nlistIterateKey (vl->selected, &iter)) >= 0) {
    int32_t     tval;

    tval = val - vl->rowoffset;
    if (tval >= 0 && tval < vl->disprows) {
      uivlrow_t   *row;

      row = uivlGetRow (vl, val);
      uiWidgetSetClass (row->hbox, "bdj-selected");
      row->selected = true;
      for (int colidx = 0; colidx < vl->numcols; ++colidx) {
        uiWidgetSetClass (row->cols [colidx].uiwidget, "bdj-selected");
      }
    }
  }
}

static void
uivlClearSelections (uivirtlist_t *vl)
{
  nlistFree (vl->selected);
  vl->selected = nlistAlloc ("vl-selected", LIST_ORDERED, NULL);
}

static void
uivlAddSelection (uivirtlist_t *vl, uint32_t rownum)
{
  uivlrow_t   *row = NULL;
  int32_t     rowidx;

  rowidx = rownum - vl->rowoffset;
  if (rowidx >= 0 && rowidx < vl->disprows) {
    row = &vl->rows [rowidx];
  }
  nlistSetNum (vl->selected, rownum, rownum);
}

static void
uivlProcessScroll (uivirtlist_t *vl, int32_t start)
{
  vl->inscroll = true;

  if (start < 0) {
    start = 0;
  }
  if (start > vl->numrows - vl->disprows) {
    start = vl->numrows - vl->disprows;
  }

  if (start == vl->rowoffset) {
    vl->inscroll = false;
    return;
  }

  vl->rowoffset = start;
fprintf (stderr, "scroll-pop\n");
  uivlPopulate (vl);
  uiScrollbarSetPosition (vl->wcont [VL_W_SB], (double) start);
  vl->inscroll = false;
}

static bool
uivlVboxSizeChg (void *udata, int32_t width, int32_t height)
{
  uivirtlist_t  *vl = udata;
  int           calcrows;
  int           theight;

fprintf (stderr, "vbox: size-chg: %d %d\n", width, height);

  vl->vboxheight = height;

  if (vl->vboxheight > 0 && vl->rowheight > 0) {
    theight = vl->vboxheight;
fprintf (stderr, "  theight: %d\n", theight);
    calcrows = theight / vl->rowheight;
fprintf (stderr, "  calcrows: %d\n", calcrows);

    if (calcrows != vl->disprows) {

      /* only if the number of rows has increased */
      if (vl->allocrows < calcrows) {
        vl->rows = mdrealloc (vl->rows, sizeof (uivlrow_t) * calcrows);
        for (int dispidx = vl->disprows; dispidx < calcrows; ++dispidx) {
          uivlrow_t *row;

          row = &vl->rows [dispidx];
          uivlRowBasicInit (row);
fprintf (stderr, "sz-row-init: %d\n", dispidx);
          uivlInitRow (vl, row, false);

fprintf (stderr, "sz-chg-pack: %d\n", dispidx);
          uivlPackRow (vl, row);
        }

        vl->allocrows = calcrows;
      }

      vl->disprows = calcrows;
fprintf (stderr, "sz-pop: disprows: %d\n", vl->disprows);
      uivlPopulate (vl);
    }
  }
  return UICB_CONT;
}

static bool
uivlRowSizeChg (void *udata, int32_t width, int32_t height)
{
  uivirtlist_t  *vl = udata;

fprintf (stderr, "row: size-chg: %d %d\n", width, height);
  vl->rowheight = height;
  return UICB_CONT;
}

static void
uivlRowBasicInit (uivlrow_t *row)
{
  row->ident = VL_IDENT_ROW;
  row->hbox = NULL;
  row->cols = NULL;
  row->initialized = false;
}
