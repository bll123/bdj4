/*
 * Copyright 2024 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
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
  VL_NOT_INIT = 0,
  VL_INIT_COLS = 1,
  VL_INIT_DISP = 2,
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
  VL_IDENT_COLDATA  = 0x766c636f6c646174,
  VL_IDENT_COL      = 0x766c636f6caabbcc,
  VL_IDENT_ROW      = 0x766c726f77aabbcc,
  VL_IDENT          = 0x766caabbccddeeff,
};

#define VL_SELECTED_CLASS "bdj-selected"

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
  /* the following data is specific to a row and column */
  char      *class;
} uivlcol_t;

typedef struct {
  uint64_t    ident;
  uiwcont_t   *eventbox;
  uiwcont_t   *hbox;
  uivlcol_t   *cols;
  bool        selected : 1;
  bool        initialized : 1;
} uivlrow_t;

typedef struct uivirtlist {
  uint64_t      ident;
  uiwcont_t     *wcont [VL_W_MAX];
  callback_t    *sbcb;
  callback_t    *keycb;
  callback_t    *clickcb;
  int           numcols;
  uivlcoldata_t *coldata;
  uivlrow_t     *rows;
  uivlrow_t     headingrow;
  int           disprows;
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
static bool uivlButtonEvent (void *udata);
static void uivlClearDisplaySelections (uivirtlist_t *vl);
static void uivlSetDisplaySelections (uivirtlist_t *vl);
static void uivlClearSelections (uivirtlist_t *vl);
static void uivlAddSelection (uivirtlist_t *vl, uint32_t rownum);
static void uivlProcessScroll (uivirtlist_t *vl, int32_t start);

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

  for (int i = 0; i < VL_W_MAX; ++i) {
    vl->wcont [i] = NULL;
  }

  vl->wcont [VL_W_KEYH] = uiKeyAlloc ();
  vl->keycb = callbackInit (uivlKeyEvent, vl, NULL);
  vl->clickcb = callbackInit (uivlButtonEvent, vl, NULL);

  vl->wcont [VL_W_HEADBOX] = uiCreateHorizBox ();
  uiWidgetAlignHorizFill (vl->wcont [VL_W_HEADBOX]);
  uiBoxPackStartExpand (boxp, vl->wcont [VL_W_HEADBOX]);

  vl->wcont [VL_W_HBOX] = uiCreateHorizBox ();
  uiWidgetAlignHorizFill (vl->wcont [VL_W_HBOX]);

  vl->wcont [VL_W_VBOX] = uiCreateVertBox ();
  uiWidgetAlignHorizFill (vl->wcont [VL_W_VBOX]);
  uiWidgetAlignVertFill (vl->wcont [VL_W_VBOX]);
  uiBoxPackStartExpand (vl->wcont [VL_W_HBOX], vl->wcont [VL_W_VBOX]);
  uiWidgetEnableFocus (vl->wcont [VL_W_VBOX]);

  vl->wcont [VL_W_SB_SZGRP] = uiCreateSizeGroupHoriz ();
  vl->wcont [VL_W_SB] = uiCreateVerticalScrollbar (10.0);
  uiSizeGroupAdd (vl->wcont [VL_W_SB_SZGRP], vl->wcont [VL_W_SB]);
  uiBoxPackEnd (vl->wcont [VL_W_HBOX], vl->wcont [VL_W_SB]);
  vl->sbcb = callbackInitDouble (uivlScrollbarCallback, vl);
  uiScrollbarSetChangeCallback (vl->wcont [VL_W_SB], vl->sbcb);

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
  vl->initialized = VL_NOT_INIT;

  vl->disprows = disprows;
  vl->rows = mdmalloc (sizeof (uivlrow_t) * disprows);
  for (int i = 0; i < disprows; ++i) {
    vl->rows [i].ident = VL_IDENT_ROW;
    vl->rows [i].eventbox = NULL;
    vl->rows [i].hbox = NULL;
    vl->rows [i].cols = NULL;
    vl->rows [i].initialized = false;
  }

  vl->headingrow.ident = VL_IDENT_ROW;
  vl->headingrow.hbox = NULL;
  vl->headingrow.cols = NULL;
  vl->headingrow.initialized = false;

  uiBoxPackStartExpand (boxp, vl->wcont [VL_W_HBOX]);
  uiWidgetAlignVertStart (boxp);

  return vl;
}

void
uivlFree (uivirtlist_t *vl)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }

  uivlFreeRow (vl, &vl->headingrow);

  for (int i = 0; i < vl->disprows; ++i) {
    uivlFreeRow (vl, &vl->rows [i]);
  }
  dataFree (vl->rows);

  for (int i = 0; i < vl->numcols; ++i) {
    uiwcontFree (vl->coldata [i].szgrp);
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
  for (int i = 0; i < numcols; ++i) {
    vl->coldata [i].szgrp = uiCreateSizeGroupHoriz ();
    vl->coldata [i].ident = VL_IDENT_COLDATA;
    vl->coldata [i].type = VL_TYPE_LABEL;
    vl->coldata [i].colident = 0;
    vl->coldata [i].minwidth = VL_MAX_WIDTH_ANY;
    vl->coldata [i].hidden = VL_COL_SHOW;
    vl->coldata [i].ellipsize = false;
  }

  vl->initialized = VL_INIT_COLS;
}

void
uivlSetHeadingFont (uivirtlist_t *vl, int colnum, const char *font)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }

}

void
uivlSetHeadingClass (uivirtlist_t *vl, int colnum, const char *class)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
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
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  uivlInitRow (vl, &vl->headingrow, true);
//    uivlSetHeadingFont (vl, VL_ROW_HEADING, colnum, need-bold-listing-font);
  uivlSetColumnValue (vl, VL_ROW_HEADING, colnum, heading);
}

void
uivlSetColumn (uivirtlist_t *vl, int colnum, vltype_t type, int colident, bool hidden)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
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
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  vl->coldata [colnum].minwidth = minwidth;
}

void
uivlSetColumnFont (uivirtlist_t *vl, int colnum, const char *font)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }
}

void
uivlSetColumnEllipsizeOn (uivirtlist_t *vl, int colnum)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  vl->coldata [colnum].ellipsize = true;
}

void
uivlSetColumnClass (uivirtlist_t *vl, uint32_t rownum, int colnum, const char *class)
{
  uivlrow_t   *row;
  uivlcol_t   *col;

  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  row = uivlGetRow (vl, rownum);
  if (row == NULL) {
    return;
  }

  col = &row->cols [colnum];
  col->class = mdstrdup (class);
}

void
uivlSetColumnAlignEnd (uivirtlist_t *vl, int colnum)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
  if (colnum < 0 || colnum >= vl->numcols) {
    return;
  }

  vl->coldata [colnum].alignend = true;
}

void
uivlSetColumnValue (uivirtlist_t *vl, int32_t rownum, int colnum, const char *value)
{
  uivlrow_t       *row = NULL;

  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
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
  if (vl->initialized < VL_INIT_COLS) {
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
  if (vl->initialized < VL_INIT_COLS) {
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
  if (vl->initialized < VL_INIT_COLS) {
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
  if (vl->initialized < VL_INIT_COLS) {
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
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
}

void
uivlSetChangeCallback (uivirtlist_t *vl, uivlchangecb_t cb, void *udata)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
}

void
uivlSetRowFillCallback (uivirtlist_t *vl, uivlfillcb_t cb, void *udata)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_COLS) {
    return;
  }
  if (cb == NULL) {
    return;
  }

  vl->fillcb = cb;
  vl->udata = udata;
}

/* processing */

void
uivlDisplay (uivirtlist_t *vl)
{
  uivlrow_t   *row;

  uiBoxPackEnd (vl->headingrow.hbox, vl->wcont [VL_W_FILLER]);
  uiBoxPackStartExpand (vl->wcont [VL_W_HEADBOX], vl->headingrow.hbox);
  uivlPopulate (vl);

  for (int i = 0; i < vl->disprows; ++i) {
    row = uivlGetRow (vl, i + vl->rowoffset);
    if (row == NULL) {
      break;
    }
    uivlPackRow (vl, row);
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

  for (int i = 0; i < vl->numcols; ++i) {
    if (row->cols != NULL) {
      uivlFreeCol (&row->cols [i]);
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

  row->cols = mdmalloc (sizeof (uivlcol_t) * vl->numcols);

  for (int i = 0; i < vl->numcols; ++i) {
    row->cols [i].ident = VL_IDENT_COL;
    row->cols [i].uiwidget = uiCreateLabel ("");
    uiWidgetSetMarginEnd (row->cols [i].uiwidget, 2);

    if (vl->coldata [i].hidden == VL_COL_SHOW) {
      uiBoxPackStartExpand (row->hbox, row->cols [i].uiwidget);
      uiSizeGroupAdd (vl->coldata [i].szgrp, row->cols [i].uiwidget);
    }
    if (vl->coldata [i].type == VL_TYPE_LABEL) {
      if (vl->coldata [i].minwidth != VL_MAX_WIDTH_ANY) {
        uiLabelSetMinWidth (row->cols [i].uiwidget, vl->coldata [i].minwidth);
      }
      if (! isheading && vl->coldata [i].ellipsize) {
        uiLabelEllipsizeOn (row->cols [i].uiwidget);
      }
      if (vl->coldata [i].alignend) {
        uiLabelAlignEnd (row->cols [i].uiwidget);
      }
    }
    row->cols [i].class = NULL;
  }
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
        fprintf (stderr, "ERR: invalid row: rownum %d rowoffset: %d rowidx: %d\n", rownum, vl->rowoffset, rowidx);
      }
      if (! row->initialized) {
        uivlInitRow (vl, row, false);
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

  row->eventbox = uiKeyCreateEventBox (row->hbox);
  uiKeySetKeyCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_VBOX], vl->keycb);
  uiKeySetButtonCallback (vl->wcont [VL_W_KEYH], row->eventbox, vl->clickcb);
  uiBoxPackStartExpand (vl->wcont [VL_W_VBOX], row->eventbox);
}

static bool
uivlScrollbarCallback (void *udata, double value)
{
  uivirtlist_t  *vl = udata;
  int32_t       start;

  start = (uint32_t) floor (value);

  uivlProcessScroll (vl, start);

  return UICB_CONT;
}

static void
uivlPopulate (uivirtlist_t *vl)
{
  for (int i = 0; i < vl->disprows; ++i) {
    uivlrow_t   *row;

    row = uivlGetRow (vl, i + vl->rowoffset);
    if (row == NULL) {
      break;
    }
    if (vl->fillcb != NULL) {
      vl->fillcb (vl->udata, vl, i + vl->rowoffset);
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

fprintf (stderr, "got key event\n");
  if (uiKeyIsMovementKey (vl->wcont [VL_W_KEYH])) {
    int32_t     start;
    int32_t     offset = 1;

fprintf (stderr, "   is move\n");
    start = vl->rowoffset;

    if (uiKeyIsKeyPressEvent (vl->wcont [VL_W_KEYH])) {
fprintf (stderr, "   is press\n");
      if (uiKeyIsPageUpDownKey (vl->wcont [VL_W_KEYH])) {
        offset = vl->disprows;
      }
      if (uiKeyIsUpKey (vl->wcont [VL_W_KEYH])) {
        start -= offset;
      }
      if (uiKeyIsDownKey (vl->wcont [VL_W_KEYH])) {
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
uivlButtonEvent (void *udata)
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

  if (! uiKeyIsButtonPressEvent (vl->wcont [VL_W_KEYH])) {
    return UICB_CONT;
  }

  button = uiKeyButtonPressed (vl->wcont [VL_W_KEYH]);
fprintf (stderr, "  button %d\n", button);

  /* button 4 and 5 cause a single scroll event */
  if (button == UIKEY_BUTTON_4 || button == UIKEY_BUTTON_5) {
    int32_t     start;

    start = vl->rowoffset;

    if (button == UIKEY_BUTTON_4) {
      start -= 1;
    }
    if (button == UIKEY_BUTTON_5) {
      start += 1;
    }

    uivlProcessScroll (vl, start);
    return UICB_CONT;
  }

  /* all other buttons (1-3) cause a selection */

  for (int i = 0; i < vl->disprows; ++i) {
    if (uiKeyCheckWidget (vl->wcont [VL_W_KEYH], vl->rows [i].eventbox)) {
      fprintf (stderr, "  found at %d\n", i);
      rownum = i + vl->rowoffset;
      break;
    }
  }

fprintf (stderr, "  control-pressed %d\n", uiKeyIsControlPressed (vl->wcont [VL_W_KEYH]));
fprintf (stderr, "  shift-pressed %d\n", uiKeyIsShiftPressed (vl->wcont [VL_W_KEYH]));

  if (rownum < 0) {
    /* not found */
    return UICB_CONT;
  }

  if (! uiKeyIsControlPressed (vl->wcont [VL_W_KEYH])) {
    uivlClearSelections (vl);
    uivlClearDisplaySelections (vl);
  }
  uivlAddSelection (vl, rownum);
  uivlSetDisplaySelections (vl);

  return UICB_CONT;
}

static void
uivlClearDisplaySelections (uivirtlist_t *vl)
{
  for (int i = 0; i < vl->disprows; ++i) {
    uivlrow_t   *row = &vl->rows [i];

    if (row->selected) {
      uiWidgetRemoveClass (row->hbox, "bdj-selected");
      for (int j = 0; j < vl->numcols; ++j) {
        uiWidgetRemoveClass (row->cols [j].uiwidget, "bdj-selected");
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
      for (int j = 0; j < vl->numcols; ++j) {
        uiWidgetSetClass (row->cols [j].uiwidget, "bdj-selected");
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
  uivlPopulate (vl);
  vl->inscroll = false;
}