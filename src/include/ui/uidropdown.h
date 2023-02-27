/*
 * Copyright 2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UIDROPDOWN_H
#define INC_UIDROPDOWN_H

#if BDJ4_USE_GTK
# include "ui-gtk3.h"
#endif

#include "callback.h"
#include "nlist.h"
#include "slist.h"

typedef struct uidropdown uidropdown_t;

uidropdown_t *uiDropDownInit (void);
void uiDropDownFree (uidropdown_t *dropdown);
UIWidget *uiDropDownCreate (UIWidget *parentwin,
    const char *title, callback_t *uicb,
    uidropdown_t *dropdown, void *udata);
UIWidget *uiComboboxCreate (UIWidget *parentwin,
    const char *title, callback_t *uicb,
    uidropdown_t *dropdown, void *udata);
void uiDropDownSetList (uidropdown_t *dropdown, slist_t *list,
    const char *selectLabel);
void uiDropDownSetNumList (uidropdown_t *dropdown, slist_t *list,
    const char *selectLabel);
void uiDropDownSelectionSetNum (uidropdown_t *dropdown, nlistidx_t idx);
void uiDropDownSelectionSetStr (uidropdown_t *dropdown, const char *stridx);
void uiDropDownDisable (uidropdown_t *dropdown);
void uiDropDownEnable (uidropdown_t *dropdown);
char *uiDropDownGetString (uidropdown_t *dropdown);

#endif /* INC_UIDROPDOWN_H */