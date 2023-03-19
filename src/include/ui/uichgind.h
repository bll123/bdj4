/*
 * Copyright 2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UICHGIND_H
#define INC_UICHGIND_H

#if BDJ4_USE_GTK
# include "ui-gtk3.h"
#endif

typedef struct uichgind uichgind_t;

uichgind_t *uiCreateChangeIndicator (uiwcont_t *boxp);
void  uichgindFree (uichgind_t *uichgind);
void  uichgindMarkNormal (uichgind_t *uichgind);
void  uichgindMarkError (uichgind_t *uichgind);
void  uichgindMarkChanged (uichgind_t *uichgind);

#endif /* INC_UICHGIND_H */
