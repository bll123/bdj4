/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include <gtk/gtk.h>

#include "ui/uiinternal.h"
#include "ui/uimiscbutton.h"

void
uiCreateFontButton (uiwidget_t *uiwidget, const char *fontname)
{
  GtkWidget   *fb;

  if (fontname != NULL && *fontname) {
    fb = gtk_font_button_new_with_font (fontname);
  } else {
    fb = gtk_font_button_new ();
  }
  uiwidget->widget = fb;
}

const char *
uiFontButtonGetFont (uiwidget_t *uiwidget)
{
  const char *sval;

  sval = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (uiwidget->widget));
  return sval;
}

void
uiCreateColorButton (uiwidget_t *uiwidget, const char *color)
{
  GtkWidget   *cb;
  GdkRGBA     rgba;

  if (color != NULL && *color) {
    gdk_rgba_parse (&rgba, color);
    cb = gtk_color_button_new_with_rgba (&rgba);
  } else {
    cb = gtk_color_button_new ();
  }
  uiwidget->widget = cb;
}

void
uiColorButtonGetColor (uiwidget_t *uiwidget, char *tbuff, size_t sz)
{
  GdkRGBA     gcolor;

  gtk_color_chooser_get_rgba (
      GTK_COLOR_CHOOSER (uiwidget->widget), &gcolor);
  snprintf (tbuff, sz, "#%02x%02x%02x",
      (int) round (gcolor.red * 255.0),
      (int) round (gcolor.green * 255.0),
      (int) round (gcolor.blue * 255.0));
}
