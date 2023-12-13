/*
 * Copyright 2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UICHGIND_H
#define INC_UICHGIND_H

#include "uiwcont.h"

uiwcont_t *uiCreateChangeIndicator (uiwcont_t *boxp);
void  uichgindFree (uiwcont_t *uiwidget);
void  uichgindMarkNormal (uiwcont_t *uiwidget);
void  uichgindMarkError (uiwcont_t *uiwidget);
void  uichgindMarkChanged (uiwcont_t *uiwidget);

#endif /* INC_UIWIDGET_H */
