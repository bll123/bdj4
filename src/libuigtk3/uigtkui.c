/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <gtk/gtk.h>
#include <hb.h>
#include <hb-glib.h>

#include "bdjstring.h"
#include "colorutils.h"
#include "filedata.h"
#include "log.h"          // needed for glogwriteroutput
#include "oslocale.h"
#include "pathbld.h"
#include "mdebug.h"
#include "sysvars.h"
#include "tmutil.h"
#include "uiclass.h"

#include "ui/uiwcont-int.h"

#include "ui/uiui.h"

static char **cssdata = NULL;
static int  csscount = 0;
static bool initialized = false;

static GLogWriterOutput uiGtkLogger (GLogLevelFlags logLevel,
    const GLogField* fields, gsize n_fields, gpointer udata);
static void uiAddScreenCSS (const char *css);

int uiBaseMarginSz = UIUTILS_BASE_MARGIN_SZ;
int uiTextDirection = TEXT_DIR_DEFAULT;
static bool inprocess = false;

const char *
uiBackend (void)
{
  return "gtk3";
}

void
uiUIInitialize (int direction)
{
  int   argc = 0;
  int   gtdir = GTK_TEXT_DIR_LTR;

  if (initialized) {
    return;
  }

  uiInitUILog ();

  /* gtk's gtk_get_locale_direction() gets very confused if the locale */
  /* settings are changed before gtk_init() is called. */

  /* the locale has already been set */
  gtk_disable_setlocale ();
  gtk_init (&argc, NULL);

  uiTextDirection = direction;
  if (direction == TEXT_DIR_RTL) {
    gtdir = GTK_TEXT_DIR_RTL;
  }

  gtk_widget_set_default_direction (gtdir);

  initialized = true;
}

void
uiUIProcessEvents (void)
{
  if (inprocess) {
    return;
  }
  inprocess = true;
  gtk_main_iteration_do (FALSE);
  while (gtk_events_pending ()) {
    gtk_main_iteration_do (FALSE);
  }
  inprocess = false;
}

void
uiUIProcessWaitEvents (void)
{
  for (int i = 0; i < 4; ++i) {
    uiUIProcessEvents ();
    mssleep (5);
  }
}

void
uiCleanup (void)
{
  if (cssdata != NULL) {
    for (int i = 0; i < csscount; ++i) {
      dataFree (cssdata [i]);
    }
    mdfree (cssdata);
    csscount = 0;
    cssdata = NULL;
  }
}

void
uiSetUICSS (const char *uifont, const char *accentColor,
    const char *errorColor)
{
  char            tbuff [8192];
  char            wbuff [400];
  char            *p;
  int             sz = 0;

  pathbldMakePath (tbuff, sizeof (tbuff),
      "gtk-static", BDJ4_CSS_EXT, PATHBLD_MP_DREL_DATA);
  p = filedataReadAll (tbuff, NULL);
  if (p == NULL) {
    pathbldMakePath (tbuff, sizeof (tbuff),
        "gtk-static", BDJ4_CSS_EXT, PATHBLD_MP_DIR_TEMPLATE);
    p = filedataReadAll (tbuff, NULL);
  }

  *tbuff = '\0';
  if (p != NULL) {
    strlcat (tbuff, p, sizeof (tbuff));
    mdfree (p);
  }

  if (uifont != NULL && *uifont) {
    char  tmp [100];

    strlcpy (tmp, uifont, sizeof (tmp));
    if (uifont != NULL && *uifont) {
      p = strrchr (tmp, ' ');
      if (p != NULL) {
        ++p;
        if (isdigit (*p)) {
          --p;
          *p = '\0';
          ++p;
          sz = atoi (p);
        }
      }

      snprintf (wbuff, sizeof (wbuff), "* { font-family: '%s'; } ", tmp);
      strlcat (tbuff, wbuff, sizeof (tbuff));
    }
  }

  if (sz > 0) {
    snprintf (wbuff, sizeof (wbuff), " * { font-size: %dpt; } ", sz);
    strlcat (tbuff, wbuff, sizeof (tbuff));
    sz -= 2;
    snprintf (wbuff, sizeof (wbuff), " menuitem label { font-size: %dpt; } ", sz);
    strlcat (tbuff, wbuff, sizeof (tbuff));
    ++sz;
    snprintf (wbuff, sizeof (wbuff), " .leftnotebook tab label { font-size: %dpt; } ", sz);
    strlcat (tbuff, wbuff, sizeof (tbuff));
  }

  if (accentColor != NULL) {
    snprintf (wbuff, sizeof (wbuff),
        "label." ACCENT_CLASS " { color: %s; } ", accentColor);
    strlcat (tbuff, wbuff, sizeof (tbuff));
    snprintf (wbuff, sizeof (wbuff),
        "label." DARKACCENT_CLASS " { color: shade(%s,0.7); } ", accentColor);
    strlcat (tbuff, wbuff, sizeof (tbuff));
    snprintf (wbuff, sizeof (wbuff),
        "entry." ACCENT_CLASS " { color: %s; } ", accentColor);
    strlcat (tbuff, wbuff, sizeof (tbuff));

    snprintf (wbuff, sizeof (wbuff),
        "progressbar." ACCENT_CLASS " > trough > progress { background-color: %s; }",
        accentColor);
    strlcat (tbuff, wbuff, sizeof (tbuff));

    snprintf (wbuff, sizeof (wbuff),
        "menu separator { background-color: shade(%s,0.5); margin-right: 12px; margin-left: 8px; }",
        accentColor);
    strlcat (tbuff, wbuff, sizeof (tbuff));

    snprintf (wbuff, sizeof (wbuff),
        "paned." ACCENT_CLASS " > separator { background-color: %s; padding-bottom: 0px; } ", accentColor);
    strlcat (tbuff, wbuff, sizeof (tbuff));
  }
  if (errorColor != NULL) {
    snprintf (wbuff, sizeof (wbuff),
        "label." ERROR_CLASS " { color: %s; } ", errorColor);
    strlcat (tbuff, wbuff, sizeof (tbuff));
  }

  /* as of 2023-1-29, the length is 2600+ */
  if (strlen (tbuff) >= sizeof (tbuff)) {
    fprintf (stderr, "possible css overflow: %zd\n", strlen (tbuff));
  }
  uiAddScreenCSS (tbuff);
}

void
uiAddColorClass (const char *classnm, const char *color)
{
  char  tbuff [100];

  snprintf (tbuff, sizeof (tbuff), "%s { color: %s; } ", classnm, color);
  uiAddScreenCSS (tbuff);
}

void
uiAddBGColorClass (const char *classnm, const char *color)
{
  char  tbuff [100];

  snprintf (tbuff, sizeof (tbuff), "%s { background-color: %s; } ", classnm, color);
  uiAddScreenCSS (tbuff);
}

void
uiAddProgressbarClass (const char *classnm, const char *color)
{
  char  tbuff [100];

  snprintf (tbuff, sizeof (tbuff),
      "progressbar.%s > trough > progress { background-color: %s; }",
      classnm, color);
  uiAddScreenCSS (tbuff);
}

void
uiInitUILog (void)
{
  g_log_set_writer_func (uiGtkLogger, NULL, NULL);
}

/* internal routines */

static GLogWriterOutput
uiGtkLogger (GLogLevelFlags logLevel,
    const GLogField* fields,
    gsize n_fields,
    gpointer udata)
{
  char  *msg;

  if (logLevel != G_LOG_LEVEL_DEBUG) {
    msg = g_log_writer_format_fields (logLevel, fields, n_fields, FALSE);
    mdextalloc (msg);
    logMsg (LOG_GTK, LOG_IMPORTANT, "%s", msg);
    if (strcmp (sysvarsGetStr (SV_BDJ4_DEVELOPMENT), "dev") == 0) {
      fprintf (stderr, "%s\n", msg);
    }
    mdfree (msg); // allocated by glib
  }

  return G_LOG_WRITER_HANDLED;
}

static void
uiAddScreenCSS (const char *css)
{
  GtkCssProvider  *tcss;
  GdkScreen       *screen;
  char            *p;

  p = mdstrdup (css);
  ++csscount;
  cssdata = mdrealloc (cssdata, sizeof (char *) * csscount);
  cssdata [csscount-1] = p;

  tcss = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (tcss, p, -1, NULL);
  screen = gdk_screen_get_default ();
  if (screen != NULL) {
    gtk_style_context_add_provider_for_screen (screen,
        GTK_STYLE_PROVIDER (tcss),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
}

