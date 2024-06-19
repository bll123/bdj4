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
  VL_MIN_WIDTH_ANY = -1,
  VL_SCROLL_NORM = false,
  VL_SCROLL_FORCE = true,
};

enum {
  VL_W_SCROLL_WIN,
  VL_W_HEADBOX,
  VL_W_EVENT_BOX,
  VL_W_MAIN_HBOX,
  VL_W_MAIN_VBOX,
  VL_W_SB,
  VL_W_SB_SZGRP,
  VL_W_HEAD_FILLER,
  VL_W_KEYH,
  VL_W_MAX,
};

enum {
  VL_CB_SB,
  VL_CB_KEY,
  VL_CB_MBUTTON,
  VL_CB_SCROLL,
  VL_CB_VBOX_SZ_CHG,
  VL_CB_ROW_SZ_CHG,
  VL_CB_MAX,
};

enum {
  VL_USER_CB_ENTRY_VAL,
  VL_USER_CB_RB_CHG,
  VL_USER_CB_CB_CHG,
  VL_USER_CB_MAX,
};

enum {
  VL_IDENT_COLDATA  = 0x766c636f6c647400,
  VL_IDENT_COL      = 0x766c636f6c00bbcc,
  VL_IDENT_ROW      = 0x766c726f7700bbcc,
  VL_IDENT          = 0x766c00bbccddeeff,
};

static const char * const VL_SELECTED_CLASS = "bdj-selected";
static const char * const VL_SEL_WIDGET_CLASS = "bdj-sel-widget";
static const char * const VL_LIST_CLASS = "bdj-listing";
static const char * const VL_HEAD_CLASS = "bdj-heading";

typedef struct {
  uiwcont_t   *szgrp;
  uint64_t    ident;
  /* the following data is specific to a column */
  vltype_t    type;
  /* the baseclass is always applied */
  char        *baseclass;
  int         colident;
  int         minwidth;
  int         entrySz;
  int         entryMaxSz;
  int         sbtype;
  callback_t  *sbcb;
  double      sbmin;
  double      sbmax;
  double      sbincr;
  double      sbpageincr;
  bool        alignend: 1;
  bool        ellipsize : 1;
  bool        grow : 1;
  bool        hidden : 1;
} uivlcoldata_t;

typedef struct {
  uint64_t  ident;
  uiwcont_t *uiwidget;
  /* class needs to be held temporarily so it can be removed */
  char      *class;
} uivlcol_t;

typedef struct {
  uivirtlist_t    *vl;
  int             dispidx;
  callback_t      *focuscb;
} uivlrowcb_t;

typedef struct {
  uivirtlist_t  *vl;
  uint64_t      ident;
  uiwcont_t     *hbox;
  uivlcol_t     *cols;
  uivlrowcb_t   *rowcb;             // must have a stable address
  bool          hidden : 1;
  bool          selected : 1;       // a temporary flag to ease processing
  bool          initialized : 1;
} uivlrow_t;

/* need a container of stable allocated addresses */
typedef struct {
  uivlrowcb_t     *rowcb;
} uivlcbcont_t;

typedef struct uivirtlist {
  uint64_t      ident;
  uiwcont_t     *wcont [VL_W_MAX];
  callback_t    *callbacks [VL_CB_MAX];
  callback_t    *usercb [VL_USER_CB_MAX];
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
  int32_t       currSelection;
  nlist_t       *selected;
  int           initialized;
  /* user callbacks */
  uivlfillcb_t  fillcb;
  void          *filludata;
  uivlselcb_t   selcb;
  void          *seludata;
  /* flags */
  bool          inscroll : 1;
} uivirtlist_t;

static void uivlFreeRow (uivirtlist_t *vl, uivlrow_t *row);
static void uivlFreeCol (uivlcol_t *col);
static void uivlCreateRow (uivirtlist_t *vl, uivlrow_t *row, int dispidx, bool isheading);
static uivlrow_t * uivlGetRow (uivirtlist_t *vl, int32_t rownum);
static void uivlPackRow (uivirtlist_t *vl, uivlrow_t *row);
static bool uivlScrollbarCallback (void *udata, double value);
static void uivlPopulate (uivirtlist_t *vl);
static bool uivlKeyEvent (void *udata);
static bool uivlButtonEvent (void *udata, int32_t dispidx, int32_t colidx);
static bool uivlScrollEvent (void *udata, int32_t dir);
static void uivlClearDisplaySelections (uivirtlist_t *vl);
static void uivlSetDisplaySelections (uivirtlist_t *vl);
static void uivlClearSelections (uivirtlist_t *vl);
static void uivlAddSelection (uivirtlist_t *vl, uint32_t rownum);
static void uivlProcessScroll (uivirtlist_t *vl, int32_t start, int sctype);
static bool uivlVboxSizeChg (void *udata, int32_t width, int32_t height);
static bool uivlRowSizeChg (void *udata, int32_t width, int32_t height);
static void uivlRowBasicInit (uivirtlist_t *vl, uivlrow_t *row, int dispidx);
static bool uivlFocusCallback (void *udata);
static void uivlUpdateSelections (uivirtlist_t *vl, int32_t rownum);
int32_t uivlRowOffsetLimit (uivirtlist_t *vl, int32_t rowoffset);
int32_t uivlRownumLimit (uivirtlist_t *vl, int32_t rownum);
static void uivlSelectionHandler (uivirtlist_t *vl, int32_t rownum, int32_t colidx);

uivirtlist_t *
uiCreateVirtList (uiwcont_t *boxp, int disprows)
{
  uivirtlist_t  *vl;

  vl = mdmalloc (sizeof (uivirtlist_t));
  vl->ident = VL_IDENT;
  vl->fillcb = NULL;
  vl->filludata = NULL;
  vl->selcb = NULL;
  vl->seludata = NULL;
  vl->inscroll = false;
  vl->vboxheight = -1;
  vl->rowheight = -1;

  for (int i = 0; i < VL_W_MAX; ++i) {
    vl->wcont [i] = NULL;
  }
  for (int i = 0; i < VL_CB_MAX; ++i) {
    vl->callbacks [i] = NULL;
  }
  for (int i = 0; i < VL_USER_CB_MAX; ++i) {
    vl->usercb [i] = NULL;
  }

  vl->wcont [VL_W_KEYH] = uiEventAlloc ();
  vl->callbacks [VL_CB_KEY] = callbackInit (uivlKeyEvent, vl, NULL);
  vl->callbacks [VL_CB_MBUTTON] = callbackInitII (uivlButtonEvent, vl);
  vl->callbacks [VL_CB_SCROLL] = callbackInitI (uivlScrollEvent, vl);
  vl->callbacks [VL_CB_VBOX_SZ_CHG] = callbackInitII (uivlVboxSizeChg, vl);
  vl->callbacks [VL_CB_ROW_SZ_CHG] = callbackInitII (uivlRowSizeChg, vl);

  vl->wcont [VL_W_HEADBOX] = uiCreateHorizBox ();
  uiWidgetExpandHoriz (vl->wcont [VL_W_HEADBOX]);
  uiBoxPackStart (boxp, vl->wcont [VL_W_HEADBOX]);

  /* a scrolled window is necessary to allow the window to shrink */
  vl->wcont [VL_W_SCROLL_WIN] = uiCreateScrolledWindow (400);
  uiWindowSetPolicyExternal (vl->wcont [VL_W_SCROLL_WIN]);
  uiWidgetExpandHoriz (vl->wcont [VL_W_SCROLL_WIN]);
  uiBoxPackStartExpand (boxp, vl->wcont [VL_W_SCROLL_WIN]);

  vl->wcont [VL_W_MAIN_HBOX] = uiCreateHorizBox ();
  uiWindowPackInWindow (vl->wcont [VL_W_SCROLL_WIN], vl->wcont [VL_W_MAIN_HBOX]);

  vl->wcont [VL_W_MAIN_VBOX] = uiCreateVertBox ();
  uiWidgetExpandHoriz (vl->wcont [VL_W_MAIN_VBOX]);
  uiWidgetEnableFocus (vl->wcont [VL_W_MAIN_VBOX]);    // for keyboard events

  vl->wcont [VL_W_EVENT_BOX] = uiEventCreateEventBox (vl->wcont [VL_W_MAIN_VBOX]);
  uiWidgetExpandHoriz (vl->wcont [VL_W_EVENT_BOX]);
  uiBoxPackStartExpand (vl->wcont [VL_W_MAIN_HBOX], vl->wcont [VL_W_EVENT_BOX]);

  uiBoxSetSizeChgCallback (vl->wcont [VL_W_MAIN_VBOX], vl->callbacks [VL_CB_VBOX_SZ_CHG]);

  vl->wcont [VL_W_SB_SZGRP] = uiCreateSizeGroupHoriz ();
  vl->wcont [VL_W_SB] = uiCreateVerticalScrollbar (10.0);
  uiSizeGroupAdd (vl->wcont [VL_W_SB_SZGRP], vl->wcont [VL_W_SB]);
  uiBoxPackEnd (vl->wcont [VL_W_MAIN_HBOX], vl->wcont [VL_W_SB]);

  vl->callbacks [VL_CB_SB] = callbackInitD (uivlScrollbarCallback, vl);
  uiScrollbarSetPageIncrement (vl->wcont [VL_W_SB], (double) (disprows / 2));
  uiScrollbarSetStepIncrement (vl->wcont [VL_W_SB], 1.0);
  uiScrollbarSetPageSize (vl->wcont [VL_W_SB], (double) disprows);
  uiScrollbarSetPosition (vl->wcont [VL_W_SB], 0.0);
  uiScrollbarSetUpper (vl->wcont [VL_W_SB], 0.0);
  uiScrollbarSetChangeCallback (vl->wcont [VL_W_SB], vl->callbacks [VL_CB_SB]);

  vl->wcont [VL_W_HEAD_FILLER] = uiCreateLabel ("");
  uiSizeGroupAdd (vl->wcont [VL_W_SB_SZGRP], vl->wcont [VL_W_HEAD_FILLER]);
  uiBoxPackEnd (vl->wcont [VL_W_HEADBOX], vl->wcont [VL_W_HEAD_FILLER]);

  vl->coldata = NULL;
  vl->rows = NULL;
  vl->numcols = 0;
  vl->numrows = 0;
  vl->rowoffset = 0;
  vl->currSelection = 0;
  vl->selected = nlistAlloc ("vl-selected", LIST_ORDERED, NULL);
  /* default selection */
  nlistSetNum (vl->selected, 0, true);
  vl->initialized = VL_INIT_NONE;

  vl->disprows = disprows;
  vl->allocrows = disprows;
  vl->rows = mdmalloc (sizeof (uivlrow_t) * vl->allocrows);
  for (int dispidx = 0; dispidx < disprows; ++dispidx) {
    uivlRowBasicInit (vl, &vl->rows [dispidx], dispidx);
  }

  uivlRowBasicInit (vl, &vl->headingrow, VL_ROW_HEADING);

  uiEventSetKeyCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_MAIN_VBOX],
      vl->callbacks [VL_CB_KEY]);
  uiEventSetButtonCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_EVENT_BOX],
      vl->callbacks [VL_CB_MBUTTON]);
  uiEventSetScrollCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_EVENT_BOX],
      vl->callbacks [VL_CB_SCROLL]);
  uiEventSetButtonCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_MAIN_VBOX],
      vl->callbacks [VL_CB_MBUTTON]);
  uiEventSetScrollCallback (vl->wcont [VL_W_KEYH], vl->wcont [VL_W_MAIN_VBOX],
      vl->callbacks [VL_CB_SCROLL]);

  return vl;
}

void
uivlFree (uivirtlist_t *vl)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }

  for (int i = 0; i < VL_CB_MAX; ++i) {
    callbackFree (vl->callbacks [i]);
  }

  uivlFreeRow (vl, &vl->headingrow);

  for (int dispidx = 0; dispidx < vl->allocrows; ++dispidx) {
    uivlFreeRow (vl, &vl->rows [dispidx]);
  }
  dataFree (vl->rows);

  for (int colidx = 0; colidx < vl->numcols; ++colidx) {
    dataFree (vl->coldata [colidx].baseclass);
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
    vl->coldata [colidx].baseclass = NULL;
    vl->coldata [colidx].colident = 0;
    vl->coldata [colidx].minwidth = VL_MIN_WIDTH_ANY;
    vl->coldata [colidx].entrySz = 0;
    vl->coldata [colidx].entryMaxSz = 0;
    vl->coldata [colidx].sbtype = 0;
    vl->coldata [colidx].sbcb = NULL;
    vl->coldata [colidx].sbmin = 0;
    vl->coldata [colidx].sbmax = 10;
    vl->coldata [colidx].sbincr = 1;
    vl->coldata [colidx].sbpageincr = 5;
    vl->coldata [colidx].alignend = false;
    vl->coldata [colidx].ellipsize = false;
    vl->coldata [colidx].grow = VL_COL_WIDTH_FIXED;
    vl->coldata [colidx].hidden = VL_COL_SHOW;
  }

  vl->initialized = VL_INIT_BASIC;
}

void
uivlSetHeadingClass (uivirtlist_t *vl, int colidx, const char *class)
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
uivlSetColumnHeading (uivirtlist_t *vl, int colidx, const char *heading)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  if (vl->initialized < VL_INIT_ROWS) {
    uivlCreateRow (vl, &vl->headingrow, VL_ROW_HEADING, true);

    for (int dispidx = 0; dispidx < vl->disprows; ++dispidx) {
      uivlrow_t *row;

      row = &vl->rows [dispidx];
      uivlCreateRow (vl, row, dispidx, false);
    }
    vl->initialized = VL_INIT_ROWS;
  }

  uivlSetRowColumnValue (vl, VL_ROW_HEADING, colidx, heading);
}

void
uivlMakeColumn (uivirtlist_t *vl, int colidx, vltype_t type,
    int colident, bool hidden)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  vl->coldata [colidx].type = type;
  vl->coldata [colidx].colident = colident;
  vl->coldata [colidx].hidden = hidden;
}

void
uivlMakeColumnEntry (uivirtlist_t *vl, int colidx, int colident, int sz, int maxsz)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  vl->coldata [colidx].type = VL_TYPE_ENTRY;
  vl->coldata [colidx].colident = colident;
  vl->coldata [colidx].entrySz = sz;
  vl->coldata [colidx].entryMaxSz = maxsz;
}

void
uivlMakeColumnSpinboxTime (uivirtlist_t *vl, int colidx, int colident,
    int sbtype, callback_t *uicb)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  vl->coldata [colidx].type = VL_TYPE_SPINBOX_TIME;
  vl->coldata [colidx].colident = colident;
  vl->coldata [colidx].sbtype = sbtype;
  vl->coldata [colidx].sbcb = uicb;
}

void
uivlMakeColumnSpinboxNum (uivirtlist_t *vl, int colidx, int colident,
    double min, double max, double incr, double pageincr)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  vl->coldata [colidx].type = VL_TYPE_SPINBOX_NUM;
  vl->coldata [colidx].colident = colident;
  vl->coldata [colidx].sbmin = min;
  vl->coldata [colidx].sbmax = max;
  vl->coldata [colidx].sbincr = incr;
  vl->coldata [colidx].sbpageincr = pageincr;
}

void
uivlSetColumnMinWidth (uivirtlist_t *vl, int colidx, int minwidth)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  vl->coldata [colidx].minwidth = minwidth;
}

void
uivlSetColumnEllipsizeOn (uivirtlist_t *vl, int colidx)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  vl->coldata [colidx].ellipsize = true;
  vl->coldata [colidx].grow = true;
}

void
uivlSetColumnAlignEnd (uivirtlist_t *vl, int colidx)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  vl->coldata [colidx].alignend = true;
}

void
uivlSetColumnGrow (uivirtlist_t *vl, int colidx, bool grow)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  vl->coldata [colidx].grow = grow;
}

void
uivlSetColumnClass (uivirtlist_t *vl, int colidx, const char *class)
{
  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  dataFree (vl->coldata [colidx].baseclass);
  vl->coldata [colidx].baseclass = mdstrdup (class);
}

void
uivlSetRowColumnClass (uivirtlist_t *vl, int32_t rownum, int colidx, const char *class)
{
  uivlrow_t *row = NULL;
  uivlcol_t *col = NULL;

  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }
  if (rownum != VL_ROW_HEADING && (rownum < 0 || rownum >= vl->numrows)) {
    return;
  }

  row = uivlGetRow (vl, rownum);
  if (row == NULL) {
    return;
  }

  col = &row->cols [colidx];
  dataFree (col->class);
  col->class = mdstrdup (class);    // save for removal process
  uiWidgetAddClass (col->uiwidget, class);
}

void
uivlSetRowColumnValue (uivirtlist_t *vl, int32_t rownum, int colidx, const char *value)
{
  uivlrow_t   *row = NULL;
  vltype_t    type;

  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }
  if (rownum != VL_ROW_HEADING && (rownum < 0 || rownum >= vl->numrows)) {
    return;
  }

  row = uivlGetRow (vl, rownum);
  if (row == NULL) {
    return;
  }

  type = vl->coldata [colidx].type;
  if (rownum == VL_ROW_HEADING) {
    type = VL_TYPE_LABEL;
  }

  switch (type) {
    case VL_TYPE_LABEL: {
      uiLabelSetText (row->cols [colidx].uiwidget, value);
      break;
    }
    case VL_TYPE_ENTRY: {
      uiEntrySetValue (row->cols [colidx].uiwidget, value);
      break;
    }
    case VL_TYPE_IMAGE:
    case VL_TYPE_INT_NUMERIC:
    case VL_TYPE_RADIO_BUTTON:
    case VL_TYPE_CHECK_BUTTON:
    case VL_TYPE_SPINBOX_NUM:
    case VL_TYPE_SPINBOX_TIME: {
      /* not handled here */
      break;
    }
  }
}

void
uivlSetRowColumnImage (uivirtlist_t *vl, int32_t rownum, int colidx,
    uiwcont_t *img, int width)
{
  uivlrow_t   *row = NULL;

  if (vl == NULL || vl->ident != VL_IDENT) {
    return;
  }
  if (vl->initialized < VL_INIT_BASIC) {
    return;
  }
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }
  if (rownum != VL_ROW_HEADING && (rownum < 0 || rownum >= vl->numrows)) {
    return;
  }

  row = uivlGetRow (vl, rownum);
  if (row == NULL) {
    return;
  }

  switch (vl->coldata [colidx].type) {
    case VL_TYPE_IMAGE: {
      uiWidgetSetSizeRequest (row->cols [colidx].uiwidget, width, -1);
      uiImageSetFromPixbuf (row->cols [colidx].uiwidget, img);
      break;
    }
    default: {
      /* not handled here */
      break;
    }
  }
}

void
uivlSetRowColumnNum (uivirtlist_t *vl, int32_t rownum, int colidx, int32_t val)
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
  if (colidx < 0 || colidx >= vl->numcols) {
    return;
  }

  row = uivlGetRow (vl, rownum);
  if (row == NULL) {
    return;
  }

  switch (vl->coldata [colidx].type) {
    case VL_TYPE_LABEL:
    case VL_TYPE_IMAGE:
    case VL_TYPE_ENTRY: {
      /* not handled here */
      break;
    }
    case VL_TYPE_INT_NUMERIC: {
      break;
    }
    case VL_TYPE_RADIO_BUTTON:
    case VL_TYPE_CHECK_BUTTON: {
      uiToggleButtonSetState (row->cols [colidx].uiwidget, val);
      break;
    }
    case VL_TYPE_SPINBOX_NUM: {
      uiSpinboxSetValue (row->cols [colidx].uiwidget, val);
      break;
    }
    case VL_TYPE_SPINBOX_TIME: {
      uiSpinboxTimeSetValue (row->cols [colidx].uiwidget, val);
      break;
    }
  }
}

/* column get */

int
uivlGetColumnIdent (uivirtlist_t *vl, int colidx)
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
uivlGetRowColumnValue (uivirtlist_t *vl, int row, int colidx)
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
uivlGetRowColumnEntryValue (uivirtlist_t *vl, int row, int colidx)
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

  vl->selcb = cb;
  vl->seludata = udata;
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
  vl->filludata = udata;
}

/* processing */

/* the initial display */
void
uivlDisplay (uivirtlist_t *vl)
{
  uivlrow_t   *row;

  uiBoxPackStartExpand (vl->wcont [VL_W_HEADBOX], vl->headingrow.hbox);

  uivlPopulate (vl);

  for (int dispidx = 0; dispidx < vl->disprows; ++dispidx) {
    row = &vl->rows [dispidx];
    uivlPackRow (vl, row);
    if (dispidx == 0) {
      uiBoxSetSizeChgCallback (row->hbox, vl->callbacks [VL_CB_ROW_SZ_CHG]);
    }
  }

  vl->initialized = VL_INIT_DISP;
  uiScrollbarSetPosition (vl->wcont [VL_W_SB], 0.0);
}

void
uivlStartSelectionIterator (uivirtlist_t *vl, int32_t *iteridx)
{
  nlistStartIterator (vl->selected, iteridx);
}

int32_t
uivlIterateSelection (uivirtlist_t *vl, int32_t *iteridx)
{
  nlistidx_t    key;

  key = nlistIterateKey (vl->selected, iteridx);
  return key;
}

int32_t
uivlSelectionCount (uivirtlist_t *vl)
{
  return nlistGetCount (vl->selected);
}

/* internal routines */

static void
uivlFreeRow (uivirtlist_t *vl, uivlrow_t *row)
{
  if (row == NULL || row->ident != VL_IDENT_ROW) {
    return;
  }

  if (row->rowcb != NULL) {
    callbackFree (row->rowcb->focuscb);
    dataFree (row->rowcb);
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
uivlCreateRow (uivirtlist_t *vl, uivlrow_t *row, int dispidx, bool isheading)
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
    vltype_t      type;
    uivlcol_t     *col;
    uivlcoldata_t *coldata;

    col = &row->cols [colidx];
    coldata = &vl->coldata [colidx];

    col->ident = VL_IDENT_COL;
    type = coldata->type;
    if (isheading) {
      type = VL_TYPE_LABEL;
    }
    switch (type) {
      case VL_TYPE_LABEL: {
        col->uiwidget = uiCreateLabel ("");
        break;
      }
      case VL_TYPE_IMAGE: {
        col->uiwidget = uiImageNew ();
        uiImageClear (col->uiwidget);
        uiWidgetSetMarginStart (col->uiwidget, 1);
        break;
      }
      case VL_TYPE_ENTRY: {
        col->uiwidget = uiEntryInit (coldata->entrySz, coldata->entryMaxSz);
        uiWidgetEnableFocus (col->uiwidget);
        uiEntrySetFocusCallback (col->uiwidget, row->rowcb->focuscb);
        break;
      }
      case VL_TYPE_RADIO_BUTTON: {
        if (dispidx == 0) {
          col->uiwidget = uiCreateRadioButton (NULL, "", 0);
        } else {
          uivlrow_t   *trow;

          trow = &vl->rows [0];
          col->uiwidget =
              uiCreateRadioButton (trow->cols [colidx].uiwidget, "", 0);
        }
        uiWidgetEnableFocus (col->uiwidget);
        uiToggleButtonSetCallback (col->uiwidget, row->rowcb->focuscb);
        break;
      }
      case VL_TYPE_CHECK_BUTTON: {
        col->uiwidget = uiCreateCheckButton ("", 0);
        uiWidgetEnableFocus (col->uiwidget);
        uiToggleButtonSetCallback (col->uiwidget, row->rowcb->focuscb);
        break;
      }
      case VL_TYPE_SPINBOX_NUM: {
        col->uiwidget = uiSpinboxIntCreate ();
        uiSpinboxSetRange (col->uiwidget, coldata->sbmin, coldata->sbmax);
        uiSpinboxSetIncrement (col->uiwidget, coldata->sbincr, coldata->sbpageincr);
        uiWidgetEnableFocus (col->uiwidget);
        uiSpinboxSetFocusCallback (col->uiwidget, row->rowcb->focuscb);
        break;
      }
      case VL_TYPE_SPINBOX_TIME: {
        col->uiwidget = uiSpinboxTimeCreate (coldata->sbtype,
            vl, "", coldata->sbcb);
        uiWidgetEnableFocus (col->uiwidget);
        uiSpinboxSetFocusCallback (col->uiwidget, row->rowcb->focuscb);
        break;
      }
      case VL_TYPE_INT_NUMERIC: {
        /* an internal numeric type will always be hidden */
        /* used to associate values with the row */
        coldata->hidden = VL_COL_HIDE;
        break;
      }
    }
    uiWidgetSetMarginEnd (col->uiwidget, 3);
    if (coldata->grow == VL_COL_WIDTH_GROW) {
      uiWidgetAlignHorizFill (col->uiwidget);
    }
    uiWidgetAddClass (col->uiwidget, VL_LIST_CLASS);
    if (isheading) {
      uiWidgetAddClass (col->uiwidget, VL_HEAD_CLASS);
    }

    if (coldata->hidden == VL_COL_SHOW) {
      if (coldata->grow == VL_COL_WIDTH_GROW) {
        uiBoxPackStartExpand (row->hbox, col->uiwidget);
      } else {
        uiBoxPackStart (row->hbox, col->uiwidget);
      }
      uiSizeGroupAdd (coldata->szgrp, col->uiwidget);
    }
    if (coldata->baseclass != NULL) {
      uiWidgetAddClass (col->uiwidget, coldata->baseclass);
    }
    if (coldata->type == VL_TYPE_LABEL) {
      if (coldata->minwidth != VL_MIN_WIDTH_ANY) {
        uiLabelSetMinWidth (col->uiwidget, coldata->minwidth);
      }
      if (! isheading && coldata->ellipsize) {
        uiLabelEllipsizeOn (col->uiwidget);
      }
      if (coldata->alignend) {
        uiLabelAlignEnd (col->uiwidget);
      }
    }
    col->class = NULL;
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

  uiBoxPackStartExpand (vl->wcont [VL_W_MAIN_VBOX], row->hbox);
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

  uivlProcessScroll (vl, start, VL_SCROLL_FORCE);

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
      vl->fillcb (vl->filludata, vl, dispidx + vl->rowoffset);
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

  if (uiEventIsKeyReleaseEvent (vl->wcont [VL_W_KEYH])) {
    return UICB_CONT;
  }

  if (uiEventIsMovementKey (vl->wcont [VL_W_KEYH])) {
    int32_t     dir = 1;
    int32_t     nsel;

    if (uiEventIsKeyPressEvent (vl->wcont [VL_W_KEYH])) {
      if (uiEventIsPageUpDownKey (vl->wcont [VL_W_KEYH])) {
        dir = vl->disprows;
      }
      if (uiEventIsUpKey (vl->wcont [VL_W_KEYH])) {
        dir = - dir;
      }
    }

    if (nlistGetCount (vl->selected) == 1) {
      nsel = vl->currSelection + dir;
      nsel = uivlRownumLimit (vl, nsel);
      uivlProcessScroll (vl, nsel, VL_SCROLL_NORM);
      uivlSelectionHandler (vl, nsel, VL_COL_UNKNOWN);
      uivlUpdateSelections (vl, nsel);
    }

    /* movement keys are handled internally */
    return UICB_STOP;
  }

  return UICB_CONT;
}

static bool
uivlButtonEvent (void *udata, int32_t dispidx, int32_t colidx)
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

  uiWidgetGrabFocus (vl->wcont [VL_W_MAIN_VBOX]);
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

    uivlProcessScroll (vl, start, VL_SCROLL_FORCE);
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

  uivlSelectionHandler (vl, rownum, colidx);
  uivlUpdateSelections (vl, rownum);

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

  uiWidgetGrabFocus (vl->wcont [VL_W_MAIN_VBOX]);

  start = vl->rowoffset;
  if (dir == UIEVENT_DIR_PREV || dir == UIEVENT_DIR_LEFT) {
    start -= 1;
  }
  if (dir == UIEVENT_DIR_NEXT || dir == UIEVENT_DIR_RIGHT) {
    start += 1;
  }
  uivlProcessScroll (vl, start, VL_SCROLL_FORCE);

  return UICB_CONT;
}

static void
uivlClearDisplaySelections (uivirtlist_t *vl)
{
  for (int dispidx = 0; dispidx < vl->disprows; ++dispidx) {
    uivlrow_t   *row = &vl->rows [dispidx];

    if (row->selected) {
      uiWidgetRemoveClass (row->hbox, VL_SELECTED_CLASS);
      uiWidgetRemoveClass (row->hbox, VL_SEL_WIDGET_CLASS);

      for (int colidx = 0; colidx < vl->numcols; ++colidx) {
        uiWidgetRemoveClass (row->cols [colidx].uiwidget, VL_SELECTED_CLASS);
        uiWidgetRemoveClass (row->cols [colidx].uiwidget, VL_SEL_WIDGET_CLASS);
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
//      uiWidgetAddClass (row->hbox, VL_SELECTED_CLASS);
      uiWidgetAddClass (row->hbox, VL_SEL_WIDGET_CLASS);
      row->selected = true;
      for (int colidx = 0; colidx < vl->numcols; ++colidx) {
//        uiWidgetAddClass (row->cols [colidx].uiwidget, VL_SELECTED_CLASS);
        uiWidgetAddClass (row->cols [colidx].uiwidget, VL_SEL_WIDGET_CLASS);
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
  nlistSetNum (vl->selected, rownum, true);

  vl->currSelection = rownum;
}

static void
uivlProcessScroll (uivirtlist_t *vl, int32_t start, int sctype)
{
  int32_t       wantrow = start;

  vl->inscroll = true;

  start = uivlRowOffsetLimit (vl, start);

  if (start == vl->rowoffset) {
    vl->inscroll = false;
    return;
  }

  /* if this is a keyboard movement, and there's only one selection */
  /* and the requested row-offset is on-screen */
  if (sctype == VL_SCROLL_NORM &&
      nlistGetCount (vl->selected) == 1 &&
      wantrow >= vl->rowoffset &&
      wantrow < vl->rowoffset + vl->disprows) {
    if (wantrow < vl->currSelection) {
      /* selection up */
      if (wantrow < vl->rowoffset + vl->disprows / 2 - 1) {
        start = vl->rowoffset - 1;
      } else {
        vl->inscroll = false;
        return;
      }
    } else {
      /* selection down */
      if (wantrow >= vl->rowoffset + vl->disprows / 2) {
        start = vl->rowoffset + 1;
      } else {
        vl->inscroll = false;
        return;
      }
    }

    start = uivlRowOffsetLimit (vl, start);
  }

  vl->rowoffset = start;
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


  vl->vboxheight = height;

  if (vl->vboxheight > 0 && vl->rowheight > 0) {
    theight = vl->vboxheight;
    calcrows = theight / vl->rowheight;

    if (calcrows != vl->disprows) {

      /* only if the number of rows has increased */
      if (vl->allocrows < calcrows) {
        vl->rows = mdrealloc (vl->rows, sizeof (uivlrow_t) * calcrows);

        for (int dispidx = vl->disprows; dispidx < calcrows; ++dispidx) {
          uivlrow_t *row;

          row = &vl->rows [dispidx];
          uivlRowBasicInit (vl, row, dispidx);
          uivlCreateRow (vl, row, dispidx, false);
          uivlPackRow (vl, row);
        }

        vl->allocrows = calcrows;
      }

      vl->disprows = calcrows;
      uiScrollbarSetPageIncrement (vl->wcont [VL_W_SB],
          (double) (vl->disprows / 2));
      uiScrollbarSetPageSize (vl->wcont [VL_W_SB], (double) vl->disprows);
      uivlPopulate (vl);
    }
  }
  return UICB_CONT;
}

static bool
uivlRowSizeChg (void *udata, int32_t width, int32_t height)
{
  uivirtlist_t  *vl = udata;

  vl->rowheight = height;
  return UICB_CONT;
}

static void
uivlRowBasicInit (uivirtlist_t *vl, uivlrow_t *row, int dispidx)
{
  row->vl = vl;
  row->ident = VL_IDENT_ROW;
  row->hbox = NULL;
  row->cols = NULL;
  row->initialized = false;
  row->rowcb = NULL;

  if (dispidx != VL_ROW_HEADING) {
    row->rowcb = mdmalloc (sizeof (uivlrowcb_t));
    row->rowcb->vl = vl;
    row->rowcb->dispidx = dispidx;
    row->rowcb->focuscb = callbackInit (uivlFocusCallback, row->rowcb, NULL);
  }
}

/* this handler must return UICB_CONT */
static bool
uivlFocusCallback (void *udata)
{
  uivlrowcb_t   *rowcb = udata;
  uivirtlist_t  *vl = rowcb->vl;
  int32_t       rownum;

  rownum = rowcb->dispidx + vl->rowoffset;
  uivlSelectionHandler (vl, rownum, VL_COL_UNKNOWN);
  uivlUpdateSelections (vl, rownum);

  return UICB_CONT;
}

static void
uivlUpdateSelections (uivirtlist_t *vl, int32_t rownum)
{
  if (vl->wcont [VL_W_KEYH] != NULL) {
    if (! uiEventIsControlPressed (vl->wcont [VL_W_KEYH])) {
      uivlClearSelections (vl);
      uivlClearDisplaySelections (vl);
    }
  }
  uivlAddSelection (vl, rownum);
  uivlSetDisplaySelections (vl);
}


int32_t
uivlRowOffsetLimit (uivirtlist_t *vl, int32_t rowoffset)
{
  if (rowoffset < 0) {
    rowoffset = 0;
  }
  if (rowoffset > vl->numrows - vl->disprows) {
    rowoffset = vl->numrows - vl->disprows;
  }

  return rowoffset;
}

int32_t
uivlRownumLimit (uivirtlist_t *vl, int32_t rownum)
{
  if (rownum < 0) {
    rownum = 0;
  }
  if (rownum >= vl->numrows) {
    rownum = vl->numrows - 1;
  }

  return rownum;
}

static void
uivlSelectionHandler (uivirtlist_t *vl, int32_t rownum, int32_t colidx)
{
  if (vl->selcb != NULL) {
    vl->selcb (vl->seludata, vl, rownum, colidx);
  }
}
