/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include <gtk/gtk.h>

#include "callback.h"
#include "mdebug.h"
#include "tagdef.h"
#include "uiclass.h"

#include "ui/uiinternal.h"
#include "ui/uibox.h"
#include "ui/uigeneral.h"
#include "ui/uitreeview.h"
#include "ui/uiui.h"
#include "ui/uiwidget.h"

static void uiTreeViewEditedHandler (GtkCellRendererText* r, const gchar* path, const gchar* ntext, gpointer udata);
static void uiTreeViewCheckboxHandler (GtkCellRendererToggle *renderer, gchar *spath, gpointer udata);
static void uiTreeViewRadioHandler (GtkCellRendererToggle *renderer, gchar *spath, gpointer udata);
static void uiTreeViewRowActiveHandler (GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* column, gpointer udata);
static gboolean uiTreeViewForeachHandler (GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer udata);
static GType uiTreeViewConvertTreeType (int type);

typedef struct uitree {
  UIWidget          uitree;
  GtkTreeSelection  *sel;
  GtkTreeIter       selectiter;
  GtkTreeIter       savedselectiter;
  GtkTreeModel      *model;
  callback_t        *rowactivecb;
  callback_t        *foreachcb;
  callback_t        *editedcb;
  callback_t        *radiocb;
  int               selmode;
  bool              selectset;
  bool              savedselectset;
} uitree_t;

uitree_t *
uiCreateTreeView (void)
{
  uitree_t          *uitree;
  GtkWidget         *tree;
  GtkTreeSelection  *sel;

  uitree = mdmalloc (sizeof (uitree_t));

  tree = gtk_tree_view_new ();
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (tree), FALSE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (tree), TRUE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), FALSE);
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
  uitree->selmode = SELECT_SINGLE;

  gtk_widget_set_halign (tree, GTK_ALIGN_START);
  gtk_widget_set_hexpand (tree, FALSE);
  gtk_widget_set_vexpand (tree, FALSE);
  uitree->uitree.widget = tree;
  uitree->sel = sel;
  uitree->selectset = false;
  uitree->savedselectset = false;
  uitree->model = NULL;
  uitree->rowactivecb = NULL;
  uitree->foreachcb = NULL;
  uitree->editedcb = NULL;
  uitree->radiocb = NULL;
  uiWidgetSetAllMargins (&uitree->uitree, 2);
  return uitree;
}

void
uiTreeViewFree (uitree_t *uitree)
{
  if (uitree == NULL) {
    return;
  }

  mdfree (uitree);
}


void
uiTreeViewEnableHeaders (uitree_t *uitree)
{
  if (uitree == NULL) {
    return;
  }

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (uitree->uitree.widget), TRUE);
}

void
uiTreeViewDisableHeaders (uitree_t *uitree)
{
  if (uitree == NULL) {
    return;
  }

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (uitree->uitree.widget), FALSE);
}

void
uiTreeViewDarkBackground (uitree_t *uitree)
{
  if (uitree == NULL) {
    return;
  }

  uiWidgetSetClass (&uitree->uitree, TREEVIEW_DARK_CLASS);
}

void
uiTreeViewDisableSingleClick (uitree_t *uitree)
{
  if (uitree == NULL) {
    return;
  }

  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (uitree->uitree.widget), FALSE);
}

void
uiTreeViewSelectSetMode (uitree_t *uitree, int mode)
{
  int     gtkmode = GTK_SELECTION_SINGLE;

  if (uitree == NULL) {
    return;
  }

  if (mode == SELECT_SINGLE) {
    gtkmode = GTK_SELECTION_SINGLE;
  }
  if (mode == SELECT_MULTIPLE) {
    gtkmode = GTK_SELECTION_MULTIPLE;
  }

  gtk_tree_selection_set_mode (uitree->sel, gtkmode);
  uitree->selmode = mode;
}

void
uiTreeViewSetRowActivatedCallback (uitree_t *uitree, callback_t *cb)
{
  if (uitree == NULL) {
    return;
  }
  uitree->rowactivecb = cb;
  g_signal_connect (uitree->uitree.widget, "row-activated",
      G_CALLBACK (uiTreeViewRowActiveHandler), uitree);
}

void
uiTreeViewSetEditedCallback (uitree_t *uitree, callback_t *cb)
{
  if (uitree == NULL) {
    return;
  }
  uitree->editedcb = cb;
}

void
uiTreeViewSetRadioCallback (uitree_t *uitree, callback_t *cb)
{
  if (uitree == NULL) {
    return;
  }
  uitree->radiocb = cb;
}

UIWidget *
uiTreeViewGetUIWidget (uitree_t *uitree)
{
  if (uitree == NULL) {
    return NULL;
  }

  return &uitree->uitree;
}

void
uiTreeViewAppendColumn (uitree_t *uitree, int widgettype,
    int coldisp, const char *title, ...)
{
  GtkCellRenderer   *renderer = NULL;
  GtkTreeViewColumn *column = NULL;
  va_list           args;
  int               coltype;
  int               col;
  char              *gtkcoltype = "text";
  int               gtkcoldisp;

  if (uitree == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }

  column = gtk_tree_view_column_new ();
  switch (widgettype) {
    case TREE_WIDGET_TEXT: {
      renderer = gtk_cell_renderer_text_new ();
      break;
    }
    case TREE_WIDGET_SPINBOX: {
      renderer = gtk_cell_renderer_spin_new ();
      gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
      break;
    }
    case TREE_WIDGET_IMAGE: {
      renderer = gtk_cell_renderer_pixbuf_new ();
      break;
    }
    case TREE_WIDGET_CHECKBOX: {
      renderer = gtk_cell_renderer_toggle_new ();
      break;
    }
    case TREE_WIDGET_RADIO: {
      renderer = gtk_cell_renderer_toggle_new ();
      gtk_cell_renderer_toggle_set_radio (GTK_CELL_RENDERER_TOGGLE (renderer), TRUE);
      break;
    }
  }
  gtk_tree_view_column_pack_start (column, renderer, TRUE);

  va_start (args, title);
  coltype = va_arg (args, int);
  while (coltype != -1) {
    col = va_arg (args, int);

    switch (coltype) {
      case TREE_COL_MODE_TEXT: {
        /* the stupid "edited" signal has no way to retrieve the column number */
        /* so set some data in the renderer to save the column number */
        g_object_set_data (G_OBJECT (renderer), "uicolumn", GUINT_TO_POINTER (col));
        gtkcoltype = "text";
        break;
      }
      case TREE_COL_MODE_EDITABLE: {
        g_signal_connect (renderer, "edited",
            G_CALLBACK (uiTreeViewEditedHandler), uitree);
        gtkcoltype = "editable";
        break;
      }
      case TREE_COL_MODE_FONT: {
        gtkcoltype = "font";
        break;
      }
      case TREE_COL_MODE_MARKUP: {
        gtkcoltype = "markup";
        break;
      }
      case TREE_COL_MODE_ADJUSTMENT: {
        gtkcoltype = "adjustment";
        break;
      }
      case TREE_COL_MODE_DIGITS: {
        gtkcoltype = "digits";
        break;
      }
      case TREE_COL_MODE_ACTIVE: {
        /* the stupid "toggled" signal has no way to retrieve the column number */
        /* so set some data in the renderer to save the column number */
        g_object_set_data (G_OBJECT (renderer), "uicolumn", GUINT_TO_POINTER (col));
        if (widgettype == TREE_WIDGET_CHECKBOX) {
          g_signal_connect (G_OBJECT(renderer), "toggled",
              G_CALLBACK (uiTreeViewCheckboxHandler), uitree);
        }
        if (widgettype == TREE_WIDGET_RADIO) {
          g_signal_connect (G_OBJECT(renderer), "toggled",
              G_CALLBACK (uiTreeViewRadioHandler), uitree);
        }
        gtkcoltype = "active";
        break;
      }
      case TREE_COL_MODE_FOREGROUND: {
        gtkcoltype = "foreground";
        break;
      }
      case TREE_COL_MODE_FOREGROUND_SET: {
        gtkcoltype = "foreground-set";
        break;
      }
    }

    gtk_tree_view_column_add_attribute (column, renderer, gtkcoltype, col);

    coltype = va_arg (args, int);
  }
  va_end (args);

  gtkcoldisp = GTK_TREE_VIEW_COLUMN_AUTOSIZE;
  switch (coldisp) {
    case TREE_COL_DISP_NORM: {
      gtkcoldisp = GTK_TREE_VIEW_COLUMN_AUTOSIZE;
      break;
    }
    case TREE_COL_DISP_GROW: {
      gtkcoldisp = GTK_TREE_VIEW_COLUMN_GROW_ONLY;
      break;
    }
  }
  gtk_tree_view_column_set_sizing (column, gtkcoldisp);
  if (title != NULL) {
    gtk_tree_view_column_set_title (column, title);
  }
  gtk_tree_view_append_column (GTK_TREE_VIEW (uitree->uitree.widget), column);
}

void
uiTreeViewCreateValueStore (uitree_t *uitree, int colmax, ...)
{
  GtkListStore  *store = NULL;
  GType         *types;
  va_list       args;
  int           tval;

  if (uitree == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }
  if (colmax <= 0 || colmax > 90) {
    return;
  }

  types = mdmalloc (sizeof (GType) * colmax);

  if (types != NULL) {
    va_start (args, colmax);
    for (int i = 0; i < colmax; ++i) {
      tval = va_arg (args, int);
      if (tval == TREE_TYPE_END) {
        fprintf (stderr, "ERR: uiTreeViewCreateValueStore: not enough values\n");
        break;
      }
      types [i] = uiTreeViewConvertTreeType (tval);
    }
    tval = va_arg (args, int);
    if (tval != TREE_TYPE_END) {
      fprintf (stderr, "ERR: uiTreeViewCreateValueStore: too many values\n");
    }
    va_end (args);

    store = gtk_list_store_newv (colmax, types);
    gtk_tree_view_set_model (GTK_TREE_VIEW (uitree->uitree.widget),
        GTK_TREE_MODEL (store));
    g_object_unref (store);
    mdfree (types);
    uitree->model = gtk_tree_view_get_model (GTK_TREE_VIEW (uitree->uitree.widget));
  }
}

void
uiTreeViewCreateValueStoreFromList (uitree_t *uitree, int colmax, int *typelist)
{
  GtkListStore  *store = NULL;
  GType         *types;

  if (uitree == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }
  if (colmax <= 0 || colmax > 90) {
    return;
  }

  types = mdmalloc (sizeof (GType) * colmax);

  if (types != NULL) {
    for (int i = 0; i < colmax; ++i) {
      types [i] = uiTreeViewConvertTreeType (typelist [i]);
    }

    store = gtk_list_store_newv (colmax, types);
    gtk_tree_view_set_model (GTK_TREE_VIEW (uitree->uitree.widget),
        GTK_TREE_MODEL (store));
    g_object_unref (store);
    mdfree (types);
    uitree->model = gtk_tree_view_get_model (GTK_TREE_VIEW (uitree->uitree.widget));
  }
}

void
uiTreeViewValueAppend (uitree_t *uitree)
{
  GtkListStore  *store = NULL;

  if (uitree == NULL) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }

  store = GTK_LIST_STORE (uitree->model);
  gtk_list_store_append (store, &uitree->selectiter);
  uitree->selectset = true;
}

void
uiTreeViewValueInsertBefore (uitree_t *uitree)
{
  GtkListStore  *store = NULL;
  GtkTreeIter   titer;
  GtkTreeIter   *titerp;
  int           count;

  if (uitree == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }

  store = GTK_LIST_STORE (uitree->model);
  count = gtk_tree_selection_count_selected_rows (uitree->sel);
  if (count == 0 || ! uitree->selectset) {
    titerp = NULL;
  } else {
    memcpy (&titer, &uitree->selectiter, sizeof (GtkTreeIter));
    titerp = &titer;
  }
  gtk_list_store_insert_before (store, &uitree->selectiter, titerp);
  gtk_tree_selection_select_iter (uitree->sel, &uitree->selectiter);
  uitree->selectset = true;
}

void
uiTreeViewValueInsertAfter (uitree_t *uitree)
{
  GtkListStore  *store = NULL;
  GtkTreeIter   titer;
  GtkTreeIter   *titerp;
  int           count;

  if (uitree == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }

  store = GTK_LIST_STORE (uitree->model);
  count = gtk_tree_selection_count_selected_rows (uitree->sel);
  if (count == 0 || ! uitree->selectset) {
    titerp = NULL;
  } else {
    memcpy (&titer, &uitree->selectiter, sizeof (GtkTreeIter));
    titerp = &titer;
  }
  gtk_list_store_insert_after (store, &uitree->selectiter, titerp);
  gtk_tree_selection_select_iter (uitree->sel, &uitree->selectiter);
  uitree->selectset = true;
}

void
uiTreeViewValueRemove (uitree_t *uitree)
{
  int     count = 0;
  int     idx;
  bool    valid = false;


  if (uitree == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }
  if (! uitree->selectset) {
    return;
  }

  idx = uiTreeViewSelectGetIndex (uitree);
  gtk_list_store_remove (GTK_LIST_STORE (uitree->model), &uitree->selectiter);
  uitree->selectset = false;
  count = gtk_tree_selection_count_selected_rows (uitree->sel);
  if (count == 1 && uitree->selmode == SELECT_SINGLE) {
    valid = gtk_tree_selection_get_selected (uitree->sel, &uitree->model, &uitree->selectiter);
    if (valid) {
      uitree->selectset = true;
    }
  }

  /* count == 0 will default to ! valid */
  if (! valid && idx >= 0) {
    --idx;
    if (idx < 0) { idx = 0; }
    uiTreeViewSelectSet (uitree, idx);
  }
}

void
uiTreeViewSetValues (uitree_t *uitree, ...)
{
  GtkListStore  *store = NULL;
  va_list       args;

  if (uitree == NULL) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }

  store = GTK_LIST_STORE (uitree->model);
  va_start (args, uitree);
  gtk_list_store_set_valist (store, &uitree->selectiter, args);
  va_end (args);
  uitree->selectset = true;
}

int
uiTreeViewSelectGetCount (uitree_t *uitree)
{
  int               count;

  uitree->selectset = false;
  if (uitree == NULL) {
    return 0;
  }
  if (uitree->model == NULL) {
    return 0;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return 0;
  }

  count = gtk_tree_selection_count_selected_rows (uitree->sel);
  uitree->selectset = false;
  if (count == 1 && uitree->selmode == SELECT_SINGLE) {
    bool    valid;

    /* this only works if the treeview is in single-selection mode */
    valid = gtk_tree_selection_get_selected (uitree->sel, &uitree->model, &uitree->selectiter);
    if (valid) {
      uitree->selectset = true;
    }
  }
  return count;
}

int
uiTreeViewSelectGetIndex (uitree_t *uitree)
{
  GtkTreePath       *path;
  char              *pathstr;
  int               idx;

  if (uitree == NULL) {
    return -1;
  }
  if (uitree->model == NULL) {
    return -1;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return -1;
  }
  if (! uitree->selectset) {
    return -1;
  }

  idx = -1;
  path = gtk_tree_model_get_path (uitree->model, &uitree->selectiter);
  mdextalloc (path);
  if (path != NULL) {
    pathstr = gtk_tree_path_to_string (path);
    mdextalloc (pathstr);
    sscanf (pathstr, "%d", &idx);
    mdextfree (path);
    gtk_tree_path_free (path);
    mdfree (pathstr);       // allocated by gtk
  }

  return idx;
}

void
uiTreeViewSelectCurrent (uitree_t *uitree)
{
  GtkTreePath       *path;

  if (uitree == NULL) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return;
  }

  path = gtk_tree_model_get_path (uitree->model, &uitree->selectiter);
  mdextalloc (path);
  if (path != NULL) {
    gtk_tree_selection_select_path (uitree->sel, path);
    mdextfree (path);
    gtk_tree_path_free (path);
  }
}

bool
uiTreeViewSelectFirst (uitree_t *uitree)
{
  int   valid = false;

  if (uitree == NULL) {
    return valid;
  }
  if (uitree->model == NULL) {
    return valid;
  }

  valid = gtk_tree_model_get_iter_first (uitree->model, &uitree->selectiter);
  if (valid) {
    uitree->selectset = true;
  }
  return valid;
}

bool
uiTreeViewSelectNext (uitree_t *uitree)
{
  int   valid = false;

  if (uitree == NULL) {
    return valid;
  }
  if (uitree->model == NULL) {
    return valid;
  }

  valid = gtk_tree_model_iter_next (uitree->model, &uitree->selectiter);
  if (valid) {
    uitree->selectset = true;
  }
  return valid;
}

bool
uiTreeViewSelectPrevious (uitree_t *uitree)
{
  int   valid = false;

  if (uitree == NULL) {
    return valid;
  }
  if (uitree->model == NULL) {
    return valid;
  }

  valid = gtk_tree_model_iter_previous (uitree->model, &uitree->selectiter);
  if (valid) {
    uitree->selectset = true;
  }
  return valid;
}

int
uiTreeViewSelectDefault (uitree_t *uitree)
{
  int               count = 0;
  GtkTreeIter       iter;
  GtkTreeModel      *model;


  count = uiTreeViewGetSelection (uitree, &model, &iter);
  if (count != 1) {
    uiTreeViewSelectSet (uitree, 0);
  }

  return count;
}

void
uiTreeViewSelectSave (uitree_t *uitree)
{
  if (uitree == NULL) {
    return;
  }
  if (! uitree->selectset) {
    return;
  }

  memcpy (&uitree->savedselectiter, &uitree->selectiter, sizeof (GtkTreeIter));
  uitree->savedselectset = uitree->selectset;
}

void
uiTreeViewSelectRestore (uitree_t *uitree)
{
  if (uitree == NULL) {
    return;
  }
  if (! uitree->selectset) {
    return;
  }

  memcpy (&uitree->selectiter, &uitree->savedselectiter, sizeof (GtkTreeIter));
  uitree->selectset = uitree->savedselectset;
}

void
uiTreeViewMoveBefore (uitree_t *uitree)
{
  GtkTreeIter   citer;
  bool          valid = false;

  if (uitree == NULL) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }

  memcpy (&citer, &uitree->selectiter, sizeof (citer));
  valid = gtk_tree_model_iter_previous (uitree->model, &uitree->selectiter);
  if (valid) {
    gtk_list_store_move_before (GTK_LIST_STORE (uitree->model),
        &citer, &uitree->selectiter);
    uitree->selectset = true;
  }
}

void
uiTreeViewMoveAfter (uitree_t *uitree)
{
  GtkTreeIter   citer;
  bool          valid = false;

  if (uitree == NULL) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }

  memcpy (&citer, &uitree->selectiter, sizeof (citer));
  valid = gtk_tree_model_iter_next (uitree->model, &uitree->selectiter);
  if (valid) {
    gtk_list_store_move_after (GTK_LIST_STORE (uitree->model),
        &citer, &uitree->selectiter);
    uitree->selectset = true;
  }
}

long
uiTreeViewGetValue (uitree_t *uitree, int col)
{
  glong     idx;

  if (uitree == NULL) {
    return -1;
  }
  if (uitree->model == NULL) {
    return -1;
  }
  if (! uitree->selectset) {
    return -1;
  }

  gtk_tree_model_get (uitree->model, &uitree->selectiter, col, &idx, -1);
  return idx;
}

char *
uiTreeViewGetValueStr (uitree_t *uitree, int col)
{
  char    *str;

  if (uitree == NULL) {
    return NULL;
  }
  if (uitree->model == NULL) {
    return NULL;
  }
  if (! uitree->selectset) {
    return NULL;
  }

  gtk_tree_model_get (uitree->model, &uitree->selectiter, col, &str, -1);
  mdextalloc (str);
  return str;
}

void
uiTreeViewForeach (uitree_t *uitree, callback_t *cb)
{
  if (uitree == NULL) {
    return;
  }
  if (uitree->model == NULL) {
    return;
  }

  /* save the selection iter */
  /* the foreach handler sets it so that data can be retrieved */
  uiTreeViewSelectSave (uitree);
  uitree->foreachcb = cb;
  gtk_tree_model_foreach (uitree->model, uiTreeViewForeachHandler, uitree);
  uiTreeViewSelectRestore (uitree);
}

void
uiTreeViewSelectSet (uitree_t *uitree, int row)
{
  char        tbuff [40];
  GtkTreePath *path = NULL;

  if (row < 0) {
    uitree->selectset = false;
    return;
  }

  snprintf (tbuff, sizeof (tbuff), "%d", row);
  path = gtk_tree_path_new_from_string (tbuff);
  mdextalloc (path);
  uitree->selectset = true;
  if (path != NULL) {
    bool    valid;

    gtk_tree_selection_select_path (uitree->sel, path);
    mdextfree (path);
    gtk_tree_path_free (path);
    if (uitree->selmode == SELECT_SINGLE) {
      valid = gtk_tree_selection_get_selected (uitree->sel, &uitree->model, &uitree->selectiter);
      if (valid) {
        uitree->selectset = true;
      }
    }
  }
}

/* the display routines are used by song selection and the music queue */
/* to create the display listings */

GtkTreeViewColumn *
uiTreeViewAddDisplayColumns (uitree_t *uitree, slist_t *sellist, int col,
    int fontcol, int ellipsizeCol)
{
  slistidx_t  seliteridx;
  int         tagidx;
  GtkCellRenderer       *renderer = NULL;
  GtkTreeViewColumn     *column = NULL;
  GtkTreeViewColumn     *favColumn = NULL;


  if (uitree == NULL) {
    return NULL;
  }

  slistStartIterator (sellist, &seliteridx);
  while ((tagidx = slistIterateValueNum (sellist, &seliteridx)) >= 0) {
    renderer = gtk_cell_renderer_text_new ();
    if (tagidx == TAG_FAVORITE) {
      /* use the normal UI font here */
      column = gtk_tree_view_column_new_with_attributes ("", renderer,
          "markup", col,
          NULL);
      gtk_cell_renderer_set_alignment (renderer, 0.5, 0.5);
      favColumn = column;
    } else {
      column = gtk_tree_view_column_new_with_attributes ("", renderer,
          "text", col,
          "font", fontcol,
          NULL);
    }
    if (tagdefs [tagidx].alignRight) {
      gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
    }
    if (tagdefs [tagidx].ellipsize) {
      if (tagidx == TAG_TITLE) {
        gtk_tree_view_column_set_min_width (column, 200);
      }
      if (tagidx == TAG_ARTIST) {
        gtk_tree_view_column_set_min_width (column, 100);
      }
      gtk_tree_view_column_add_attribute (column, renderer,
          "ellipsize", ellipsizeCol);
      gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
      gtk_tree_view_column_set_expand (column, TRUE);
    } else {
      gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    }
    gtk_tree_view_append_column (GTK_TREE_VIEW (uitree->uitree.widget), column);
    if (tagidx == TAG_FAVORITE) {
      gtk_tree_view_column_set_title (column, "\xE2\x98\x86");
    } else {
      if (tagdefs [tagidx].shortdisplayname != NULL) {
        gtk_tree_view_column_set_title (column, tagdefs [tagidx].shortdisplayname);
      } else {
        gtk_tree_view_column_set_title (column, tagdefs [tagidx].displayname);
      }
    }
    col++;
  }

  return favColumn;
}

void
uiTreeViewSetDisplayColumn (GtkTreeModel *model, GtkTreeIter *iter,
    int col, long num, const char *str)
{
  if (str != NULL) {
    gtk_list_store_set (GTK_LIST_STORE (model), iter, col, str, -1);
  } else {
    char    tstr [40];

    *tstr = '\0';
    if (num >= 0) {
      snprintf (tstr, sizeof (tstr), "%ld", num);
    }
    gtk_list_store_set (GTK_LIST_STORE (model), iter, col, tstr, -1);
  }
}


/* these routines will be removed at a later date */

int
uiTreeViewGetSelection (uitree_t *uitree, GtkTreeModel **model, GtkTreeIter *iter)
{
  GtkTreeSelection  *sel;
  int               count;

  if (uitree == NULL) {
    return 0;
  }
  if (! uiutilsUIWidgetSet (&uitree->uitree)) {
    return 0;
  }

  sel = uitree->sel;
  count = gtk_tree_selection_count_selected_rows (sel);
  uitree->selectset = false;
  if (count == 1 && uitree->selmode == SELECT_SINGLE) {
    bool    valid;

    /* this only works if the treeview is in single-selection mode */
    valid = gtk_tree_selection_get_selected (sel, model, iter);
    if (valid) {
      uitree->selectset = true;
    }
  }
  return count;
}

/* internal routines */

/* used by the editable column routines */
static void
uiTreeViewEditedHandler (GtkCellRendererText* r, const gchar* pathstr,
    const gchar* ntext, gpointer udata)
{
  uitree_t      *uitree = udata;
  GtkWidget     *tree;
  GtkTreeModel  *model;
  GtkTreeIter   iter;
  GType         coltype;
  int           col;

  if (uitree == NULL) {
    return;
  }

  tree = uitree->uitree.widget;

  /* retrieve the column number from the 'uicolumn' value set when */
  /* the column was created */
  col = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (r), "uicolumn"));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
  gtk_tree_model_get_iter_from_string (model, &iter, pathstr);
  coltype = gtk_tree_model_get_column_type (model, col);
  if (coltype == G_TYPE_STRING) {
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, ntext, -1);
  }
  if (coltype == G_TYPE_LONG) {
    long val = atol (ntext);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, val, -1);
  }

  if (uitree->editedcb != NULL) {
    callbackHandlerLong (uitree->editedcb, col);
  }
}

static void
uiTreeViewCheckboxHandler (GtkCellRendererToggle *r,
    gchar *pathstr, gpointer udata)
{
  uitree_t      *uitree = udata;
  GtkWidget     *tree;
  GtkTreeModel  *model;
  GtkTreeIter   iter;
  int           col;
  gint          val;

  if (uitree == NULL) {
    return;
  }

  tree = uitree->uitree.widget;

  /* retrieve the column number from the 'uicolumn' value set when */
  /* the column was created */
  col = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (r), "uicolumn"));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
  gtk_tree_model_get_iter_from_string (model, &iter, pathstr);
  gtk_tree_model_get (model, &iter, col, &val, -1);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, !val, -1);

  if (uitree->editedcb != NULL) {
    callbackHandlerLong (uitree->editedcb, col);
  }
}

static void
uiTreeViewRadioHandler (GtkCellRendererToggle *r,
    gchar *pathstr, gpointer udata)
{
  uitree_t      *uitree = udata;
  GtkWidget     *tree;
  GtkTreeModel  *model;
  GtkTreeIter   iter;
  int           col;

  if (uitree == NULL) {
    return;
  }

  tree = uitree->uitree.widget;

  /* retrieve the column number from the 'uicolumn' value set when */
  /* the column was created */
  col = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (r), "uicolumn"));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
  gtk_tree_model_get_iter_from_string (model, &iter, pathstr);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, 1, -1);

  if (uitree->radiocb != NULL) {
    /* the callback must handle clearing the old value */
    callbackHandlerIntInt (uitree->radiocb, atoi (pathstr), col);
  }
}

static void
uiTreeViewRowActiveHandler (GtkTreeView* tv, GtkTreePath* path,
    GtkTreeViewColumn* column, gpointer udata)
{
  uitree_t    *uitree = udata;
  int         count;

  if (uitree == NULL) {
    return;
  }

  count = uiTreeViewSelectGetCount (uitree);
  uitree->selectset = false;
  if (count == 1 && uitree->selmode == SELECT_SINGLE) {
    bool  valid;

    valid = gtk_tree_selection_get_selected (uitree->sel, &uitree->model, &uitree->selectiter);
    if (valid) {
      uitree->selectset = true;
    }
  }
  if (uitree->rowactivecb != NULL) {
    callbackHandler (uitree->rowactivecb);
  }
}

static gboolean
uiTreeViewForeachHandler (GtkTreeModel* model, GtkTreePath* path,
    GtkTreeIter* iter, gpointer udata)
{
  uitree_t      *uitree = udata;

  memcpy (&uitree->selectiter, iter, sizeof (GtkTreeIter));
  uitree->selectset = true;
  return callbackHandler (uitree->foreachcb);
}

static GType
uiTreeViewConvertTreeType (int type)
{
  GType   gtktype = G_TYPE_LONG;

  switch (type) {
    case TREE_TYPE_STRING: {
      gtktype = G_TYPE_STRING;
      break;
    }
    case TREE_TYPE_NUM: {
      gtktype = G_TYPE_LONG;
      break;
    }
    case TREE_TYPE_WIDGET: {
      gtktype = G_TYPE_OBJECT;
      break;
    }
    case TREE_TYPE_BOOLEAN: {
      gtktype = G_TYPE_BOOLEAN;
      break;
    }
    case TREE_TYPE_ELLIPSIZE: {
      gtktype = G_TYPE_INT;
      break;
    }
    case TREE_TYPE_IMAGE: {
      gtktype = GDK_TYPE_PIXBUF;
      break;
    }
    case TREE_TYPE_END: {
      fprintf (stderr, "ERR: tree-type-end found in conversion\n");
      break;
    }
    default: {
      fprintf (stderr, "ERR: unknown tree type: %d\n", type);
      break;
    }
  }

  return gtktype;
}
