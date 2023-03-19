/*
 * Copyright 2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UISWITCH_H
#define INC_UISWITCH_H

#if BDJ4_USE_GTK
# include "ui-gtk3.h"
#endif

#include "callback.h"

typedef struct uiswitch uiswitch_t;

uiswitch_t *uiCreateSwitch (int value);
void uiSwitchFree (uiswitch_t *uiswitch);
void uiSwitchSetValue (uiswitch_t *uiswitch, int value);
int uiSwitchGetValue (uiswitch_t *uiswitch);
uiwidget_t *uiSwitchGetUIWidget (uiswitch_t *uiswitch);
void uiSwitchSetCallback (uiswitch_t *uiswitch, callback_t *uicb);
void uiSwitchSetState (uiswitch_t *uiswitch, int state);

#endif /* INC_UISWITCH_H */
