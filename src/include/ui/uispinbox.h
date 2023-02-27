/*
 * Copyright 2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UISPINBOX_H
#define INC_UISPINBOX_H

#if BDJ4_USE_GTK
# include "ui-gtk3.h"
#endif

#include "callback.h"
#include "nlist.h"
#include "slist.h"

enum {
  SB_TEXT,
  SB_TIME_BASIC,
  SB_TIME_PRECISE,
};

typedef const char * (*uispinboxdisp_t)(void *, int);
typedef struct uispinbox uispinbox_t;

uispinbox_t *uiSpinboxInit (void);
void  uiSpinboxFree (uispinbox_t *spinbox);

void  uiSpinboxTextCreate (uispinbox_t *spinbox, void *udata);
void  uiSpinboxTextSet (uispinbox_t *spinbox, int min, int count,
    int maxWidth, slist_t *list, nlist_t *keylist, uispinboxdisp_t textGetProc);
int   uiSpinboxTextGetIdx (uispinbox_t *spinbox);
int   uiSpinboxTextGetValue (uispinbox_t *spinbox);
void  uiSpinboxTextSetValue (uispinbox_t *spinbox, int ivalue);
void  uiSpinboxDisable (uispinbox_t *spinbox);
void  uiSpinboxEnable (uispinbox_t *spinbox);

void  uiSpinboxIntCreate (UIWidget *uiwidget);
void  uiSpinboxDoubleCreate (UIWidget *uiwidget);

void  uiSpinboxDoubleDefaultCreate (uispinbox_t *spinbox);

uispinbox_t *uiSpinboxTimeInit (int sbtype);
void  uiSpinboxTimeCreate (uispinbox_t *spinbox, void *udata, callback_t *convcb);
ssize_t uiSpinboxTimeGetValue (uispinbox_t *spinbox);
void  uiSpinboxTimeSetValue (uispinbox_t *spinbox, ssize_t value);

void  uiSpinboxSetRange (uispinbox_t *spinbox, double min, double max);
void  uiSpinboxWrap (uispinbox_t *spinbox);
void  uiSpinboxSet (UIWidget *uispinbox, double min, double max);
double uiSpinboxGetValue (UIWidget *uispinbox);
void  uiSpinboxSetValue (UIWidget *uispinbox, double ivalue);
bool  uiSpinboxIsChanged (uispinbox_t *spinbox);
void  uiSpinboxResetChanged (uispinbox_t *spinbox);
void  uiSpinboxAlignRight (uispinbox_t *spinbox);
void  uiSpinboxAddClass (const char *classnm, const char *color);
UIWidget * uiSpinboxGetUIWidget (uispinbox_t *spinbox);
void uiSpinboxTextSetValueChangedCallback (uispinbox_t *spinbox, callback_t *uicb);
void uiSpinboxTimeSetValueChangedCallback (uispinbox_t *spinbox, callback_t *uicb);
void uiSpinboxSetValueChangedCallback (UIWidget *uiwidget, callback_t *uicb);

#endif /* INC_UISPINBOX_H */