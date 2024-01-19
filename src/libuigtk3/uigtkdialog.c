/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

#include <gtk/gtk.h>

#include "bdj4intl.h"
#include "callback.h"
#include "fileop.h"
#include "mdebug.h"
#include "uiwcont.h"

#include "ui/uiwcont-int.h"

#include "ui/uidialog.h"
#include "ui/uiwidget.h"
#include "ui/uiwindow.h"

typedef struct uiselect {
  uiwcont_t   *window;
  const char  *label;
  const char  *startpath;
  const char  *dfltname;
  const char  *mimefiltername;
  const char  *mimetype;
} uiselect_t;

static void uiDialogResponseHandler (GtkDialog *d, gint responseid, gpointer udata);
static gboolean uiDialogIsDirectory (const GtkFileFilterInfo* filterInfo, gpointer udata);
static void uiDialogAddButtonsInternal (uiwcont_t *uiwidget, va_list valist);

char *
uiSelectDirDialog (uiselect_t *selectdata)
{
  GtkFileChooserNative *widget = NULL;
  gint      res;
  char      *fn = NULL;
  GtkFileFilter   *ff;

  widget = gtk_file_chooser_native_new (
      selectdata->label,
      GTK_WINDOW (selectdata->window->widget),
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      /* CONTEXT: select folder dialog: select folder */
      _("Select"),
      /* CONTEXT: select folder dialog: close */
      _("Close"));

  if (selectdata->startpath != NULL) {
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget),
        selectdata->startpath);
  }

  /* gtk does not appear to filter on folders when select-folder is in use */
  ff = gtk_file_filter_new ();
  gtk_file_filter_add_custom (ff, GTK_FILE_FILTER_FILENAME,
      uiDialogIsDirectory, NULL, NULL);
  /* CONTEXT: select folder dialog: filter on folders. */
  gtk_file_filter_set_name (ff, _("Folder"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), ff);

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (widget));
  if (res == GTK_RESPONSE_ACCEPT) {
    fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
    mdextalloc (fn);
  }

  g_object_unref (widget);
  return fn;
}

char *
uiSelectFileDialog (uiselect_t *selectdata)
{
  GtkFileChooserNative *widget = NULL;
  gint      res;
  char      *fn = NULL;

  widget = gtk_file_chooser_native_new (
      selectdata->label,
      GTK_WINDOW (selectdata->window->widget),
      GTK_FILE_CHOOSER_ACTION_OPEN,
      /* CONTEXT: select file dialog: select file */
      _("Select"),
      /* CONTEXT: select file dialog: close */
      _("Close"));

  if (selectdata->startpath != NULL) {
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget),
        selectdata->startpath);
  }
  if (selectdata->mimetype != NULL) {
    GtkFileFilter   *ff;

    ff = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (ff, selectdata->mimetype);
    if (selectdata->mimefiltername != NULL) {
      gtk_file_filter_set_name (ff, selectdata->mimefiltername);
    }
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), ff);
  }

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (widget));
  if (res == GTK_RESPONSE_ACCEPT) {
    fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
    mdextalloc (fn);
  }

  g_object_unref (widget);
  return fn;
}

char *
uiSaveFileDialog (uiselect_t *selectdata)
{
  GtkFileChooserNative *widget = NULL;
  gint      res;
  char      *fn = NULL;

  widget = gtk_file_chooser_native_new (
      selectdata->label,
      GTK_WINDOW (selectdata->window->widget),
      GTK_FILE_CHOOSER_ACTION_SAVE,
      /* CONTEXT: save file dialog: save */
      _("Save"),
      /* CONTEXT: save file dialog: close */
      _("Close"));

  if (selectdata->startpath != NULL) {
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget),
        selectdata->startpath);
  }
  if (selectdata->dfltname != NULL) {
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (widget),
        selectdata->dfltname);
  }
  if (selectdata->mimetype != NULL) {
    GtkFileFilter   *ff;

    ff = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (ff, selectdata->mimetype);
    if (selectdata->mimefiltername != NULL) {
      gtk_file_filter_set_name (ff, selectdata->mimefiltername);
    }
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), ff);
  }

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (widget));
  if (res == GTK_RESPONSE_ACCEPT) {
    fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
    mdextalloc (fn);
  }

  g_object_unref (widget);
  return fn;
}

uiwcont_t *
uiCreateDialog (uiwcont_t *window,
    callback_t *uicb, const char *title, ...)
{
  uiwcont_t *uiwidget;
  GtkWidget *dialog;
  va_list   valist;

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), title);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window->widget));

  uiwidget = uiwcontAlloc ();
  uiwidget->wbasetype = WCONT_T_WINDOW;
  uiwidget->wtype = WCONT_T_DIALOG_WINDOW;
  uiwidget->widget = dialog;

  va_start (valist, title);
  uiDialogAddButtonsInternal (uiwidget, valist);
  va_end (valist);

  if (uicb != NULL) {
    g_signal_connect (dialog, "response",
        G_CALLBACK (uiDialogResponseHandler), uicb);
  }

  return uiwidget;
}

void
uiDialogShow (uiwcont_t *uiwidgetp)
{
  if (! uiwcontValid (uiwidgetp, WCONT_T_DIALOG_WINDOW, "dialog-show")) {
    return;
  }

  uiWidgetShowAll (uiwidgetp);
  uiWindowPresent (uiwidgetp);
}

void
uiDialogAddButtons (uiwcont_t *uidialog, ...)
{
  va_list   valist;

  if (! uiwcontValid (uidialog, WCONT_T_DIALOG_WINDOW, "dialog-show")) {
    return;
  }

  va_start (valist, uidialog);
  uiDialogAddButtonsInternal (uidialog, valist);
  va_end (valist);
}

void
uiDialogPackInDialog (uiwcont_t *uidialog, uiwcont_t *boxp)
{
  GtkWidget *content;

  if (! uiwcontValid (uidialog, WCONT_T_DIALOG_WINDOW, "dialog-pack-in-dialog")) {
    return;
  }

  content = gtk_dialog_get_content_area (GTK_DIALOG (uidialog->widget));
  gtk_container_add (GTK_CONTAINER (content), boxp->widget);
}

void
uiDialogDestroy (uiwcont_t *uidialog)
{
  if (! uiwcontValid (uidialog, WCONT_T_DIALOG_WINDOW, "dialog-destroy")) {
    return;
  }

  if (GTK_IS_WIDGET (uidialog->widget)) {
    gtk_widget_destroy (GTK_WIDGET (uidialog->widget));
  }
  uidialog->widget = NULL;
}

uiselect_t *
uiDialogCreateSelect (uiwcont_t *window, const char *label,
    const char *startpath, const char *dfltname,
    const char *mimefiltername, const char *mimetype)
{
  uiselect_t  *selectdata;

  if (! uiwcontValid (window, WCONT_T_WINDOW, "dialog-create-sel")) {
    return NULL;
  }

  selectdata = mdmalloc (sizeof (uiselect_t));
  selectdata->window = window;
  selectdata->label = label;
  selectdata->startpath = startpath;
  selectdata->dfltname = dfltname;
  selectdata->mimefiltername = mimefiltername;
  selectdata->mimetype = mimetype;
  return selectdata;
}

/* internal routines */

static void
uiDialogResponseHandler (GtkDialog *d, gint responseid, gpointer udata)
{
  callback_t  *uicb = udata;
  if (responseid == GTK_RESPONSE_DELETE_EVENT) {
    responseid = RESPONSE_DELETE_WIN;
  }
  callbackHandlerLong (uicb, responseid);
}

static gboolean
uiDialogIsDirectory (const GtkFileFilterInfo* filterInfo, gpointer udata)
{
  bool  rc = false;

  if (fileopIsDirectory (filterInfo->filename)) {
    rc = true;
  }

  return rc;
}

static void
uiDialogAddButtonsInternal (uiwcont_t *uiwidget, va_list valist)
{
  GtkWidget *dialog;
  char      *label;
  int       resp;

  if (uiwidget == NULL) {
    return;
  }

  dialog = uiwidget->widget;
  while ((label = va_arg (valist, char *)) != NULL) {
    resp = va_arg (valist, int);
    gtk_dialog_add_button (GTK_DIALOG (dialog), label, resp);
  }
}
