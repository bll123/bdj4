/*
 * Copyright 2023-2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_UISIZEGRP_H
#define INC_UISIZEGRP_H

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

#include "uiwcont.h"

uiwcont_t *uiCreateSizeGroupHoriz (void);
void uiSizeGroupAdd (uiwcont_t *uisg, uiwcont_t *uiwidget);

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif

#endif /* INC_UISIZEGRP_H */
