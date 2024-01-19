/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UIRATING_H
#define INC_UIRATING_H

#include "callback.h"
#include "ui.h"

typedef struct uirating uirating_t;

enum {
  UIRATING_NORM = false,
  UIRATING_ALL = true,
};

uirating_t * uiratingSpinboxCreate (uiwcont_t *boxp, bool allflag);
void uiratingFree (uirating_t *uirating);
int uiratingGetValue (uirating_t *uirating);
void uiratingSetValue (uirating_t *uirating, int value);
void uiratingSetState (uirating_t *uirating, int state);
void uiratingSizeGroupAdd (uirating_t *uirating, uiwcont_t *sg);
void uiratingSetChangedCallback (uirating_t *uirating, callback_t *cb);

#endif /* INC_UIRATING_H */
