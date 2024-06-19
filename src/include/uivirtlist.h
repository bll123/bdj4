/*
 * Copyright 2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UIVIRTLIST_H
#define INC_UIVIRTLIST_H

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

#include <stdint.h>

#include "callback.h"
#include "uiwcont.h"
#include "ui/uientry.h"

typedef enum {
  VL_TYPE_LABEL,
  VL_TYPE_IMAGE,
  VL_TYPE_ENTRY,
  VL_TYPE_RADIO_BUTTON,
  VL_TYPE_CHECK_BUTTON,
  VL_TYPE_SPINBOX_NUM,
  VL_TYPE_SPINBOX_TIME,
  VL_TYPE_INTERNAL_NUMERIC,
} vltype_t;

enum {
  VL_COL_HIDE = false,
  VL_COL_SHOW = true,
  VL_COL_WIDTH_FIXED = false,
  VL_COL_WIDTH_GROW = true,
  VL_COL_UNKNOWN = -1,
  VL_SEL_DISP_NORM = 0,
  VL_SEL_DISP_WIDGET = 1,
  VL_NO_HEADING = 0,
  VL_SHOW_HEADING = 1,
};

typedef struct uivirtlist uivirtlist_t;
typedef void (*uivlfillcb_t) (void *udata, uivirtlist_t *vl, int32_t rownum);
typedef void (*uivlselcb_t) (void *udata, uivirtlist_t *vl, int32_t rownum, int colidx);

/* basic */
uivirtlist_t *uiCreateVirtList (uiwcont_t *boxp, int initialdisprows, int headingflag);
void  uivlFree (uivirtlist_t *vl);
void  uivlSetNumRows (uivirtlist_t *vl, int32_t numrows);
void  uivlSetNumColumns (uivirtlist_t *vl, int numcols);
void  uivlSetDarkBackground (uivirtlist_t *vl);

/* make columns */
void  uivlMakeColumn (uivirtlist_t *vl, int colidx, vltype_t type);
void  uivlMakeColumnEntry (uivirtlist_t *vl, int colidx, int sz, int maxsz);
void  uivlMakeColumnSpinboxTime (uivirtlist_t *vl, int colidx, int sbtype, callback_t *uicb);
void  uivlMakeColumnSpinboxNum (uivirtlist_t *vl, int colidx, double min, double max, double incr, double pageincr);

/* column set */
void  uivlSetColumnHeading (uivirtlist_t *vl, int colidx, const char *heading);
void  uivlSetColumnMinWidth (uivirtlist_t *vl, int colidx, int minwidth);
void  uivlSetColumnEllipsizeOn (uivirtlist_t *vl, int col);
void  uivlSetColumnAlignEnd (uivirtlist_t *vl, int col);
void  uivlSetColumnGrow (uivirtlist_t *vl, int col, bool grow);
void  uivlSetColumnClass (uivirtlist_t *vl, int col, const char *class);
void  uivlSetColumnDisplay (uivirtlist_t *vl, int colidx, bool hidden);

/* column set specific to a row */
void  uivlSetRowColumnClass (uivirtlist_t *vl, int32_t rownum, int colidx, const char *class);
void  uivlSetRowColumnImage (uivirtlist_t *vl, int32_t rownum, int colidx, uiwcont_t *img, int width);
void  uivlSetRowColumnValue (uivirtlist_t *vl, int32_t rownum, int colidx, const char *value);
void  uivlSetRowColumnNum (uivirtlist_t *vl, int32_t rownum, int colidx, int32_t val);

/* column get specific to a row */
const char *uivlGetRowColumnEntryValue (uivirtlist_t *vl, int32_t rownum, int colidx);
int32_t uivlGetRowColumnNum (uivirtlist_t *vl, int32_t rownum, int colidx);

/* callbacks */
void  uivlSetSelectionCallback (uivirtlist_t *vl, uivlselcb_t cb, void *udata);
void  uivlSetDoubleClickCallback (uivirtlist_t *vl, uivlselcb_t cb, void *udata);
void  uivlSetRowFillCallback (uivirtlist_t *vl, uivlfillcb_t cb, void *udata);
void  uivlSetEntryValidation (uivirtlist_t *vl, int colidx, uientryval_t cb, void *udata);
void  uivlSetRadioChangeCallback (uivirtlist_t *vl, int colidx, callback_t *cb);
void  uivlSetCheckBoxChangeCallback (uivirtlist_t *vl, int colidx, callback_t *cb);
void  uivlSetSpinboxTimeChangeCallback (uivirtlist_t *vl, int colidx, callback_t *cb);
void  uivlSetSpinboxChangeCallback (uivirtlist_t *vl, int colidx, callback_t *cb);

/* processing */
void  uivlDisplay (uivirtlist_t *vl);
void  uivlPopulate (uivirtlist_t *vl);
void  uivlStartSelectionIterator (uivirtlist_t *vl, int32_t *iteridx);
int32_t uivlIterateSelection (uivirtlist_t *vl, int32_t *iteridx);
int32_t uivlSelectionCount (uivirtlist_t *vl);
int32_t uivlGetCurrSelection (uivirtlist_t *vl);
void uivlSetSelection (uivirtlist_t *vl, int32_t rownum);

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif

#endif /* INC_UIVIRTLIST_H */
