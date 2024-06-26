/*
 * Copyright 2023-2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UIDD_H
#define INC_UIDD_H

#include "callback.h"
#include "ilist.h"
#include "uiwcont.h"

typedef struct uidd uidd_t;

enum {
  DD_KEEP_TITLE,
  DD_REPLACE_TITLE,     // a combo-box (no entry)
  DD_LIST_TYPE_STR,
  DD_LIST_TYPE_NUM,
  DD_PACK_END,
  DD_PACK_START,
};

/* ilist indices */
enum {
  DD_LIST_KEY_STR,
  DD_LIST_KEY_NUM,
  DD_LIST_DISP,
};

enum {
  DD_NO_SELECTION = -1,
};

uidd_t *uiddCreate (const char *tag, uiwcont_t *parentwin, uiwcont_t *boxp, int where, ilist_t *keylist, int listtype, const char *title, int titleflag, callback_t *ddcb);
void uiddFree (uidd_t *dd);
uiwcont_t *uiddGetButton (uidd_t *dd);
void uiddSetSelection (uidd_t *dd, nlistidx_t idx);
void uiddSetState (uidd_t *dd, int state);
const char *uiddGetSelectionStr (uidd_t *dd);
int uiddGetSelectionNum (uidd_t *dd);

#endif /* INC_UIDD_H */
