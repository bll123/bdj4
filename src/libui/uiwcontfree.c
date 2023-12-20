/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mdebug.h"
#include "uiwcont.h"

#include "ui/uiwcont-int.h"

#include "ui/uikeys.h"
#include "ui/uimenu.h"
#include "ui/uiscrollbar.h"
#include "ui/uiswitch.h"

void
uiwcontFree (uiwcont_t *uiwidget)
{
  if (uiwidget == NULL) {
    return;
  }

  switch (uiwidget->wtype) {
    case WCONT_T_UNKNOWN: {
      fprintf (stderr, "ERR: trying to free a uiwidget twice\n");
      break;
    }
    case WCONT_T_KEY: {
      uiKeyFree (uiwidget);
      break;
    }
    case WCONT_T_MENU: {
      uiMenuFree (uiwidget);
      break;
    }
    case WCONT_T_SCROLLBAR: {
      uiScrollbarFree (uiwidget);
      break;
    }
    case WCONT_T_SWITCH: {
      uiSwitchFree (uiwidget);
      break;
    }
    default: {
      break;
    }
  }

  uiwcontBaseFree (uiwidget);
}
