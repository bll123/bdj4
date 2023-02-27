/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>

#include <gtk/gtk.h>

#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjvarsdf.h"
#include "dance.h"
#include "manageui.h"
#include "mdebug.h"
#include "playlist.h"
#include "tagdef.h"
#include "tmutil.h"
#include "ui.h"
#include "callback.h"
#include "validate.h"

enum {
  MPLTREE_COL_DANCE_SELECT,
  MPLTREE_COL_DANCE,
  MPLTREE_COL_COUNT,
  MPLTREE_COL_MAXPLAYTIME,
  MPLTREE_COL_LOWBPM,
  MPLTREE_COL_HIGHBPM,
  MPLTREE_COL_SB_PAD,
  MPLTREE_COL_DANCE_IDX,
  MPLTREE_COL_EDITABLE,
  MPLTREE_COL_ADJUST,
  MPLTREE_COL_DIGITS,
  MPLTREE_COL_MAX,
};

typedef struct managepltree {
  uitree_t          *uitree;
  GtkTreeViewColumn *danceselcol;
  GtkTreeViewColumn *dancecol;
  GtkTreeViewColumn *countcol;
  GtkTreeViewColumn *lowbpmcol;
  GtkTreeViewColumn *highbpmcol;
  UIWidget          *statusMsg;
  UIWidget          uihideunsel;
  callback_t        *unselcb;
  playlist_t        *playlist;
  bool              changed : 1;
  bool              hideunselected : 1;
  bool              inprepop : 1;
} managepltree_t;

static void managePlaylistTreeSetColumnVisibility (managepltree_t *managepltree, int pltype);
static void managePlaylistTreeToggleDance (GtkCellRendererToggle *renderer, gchar *spath, gpointer udata);
static void managePlaylistTreeEditInt (GtkCellRendererText* r, const gchar* path, const gchar* ntext, gpointer udata);
static void managePlaylistTreeEditTime (GtkCellRendererText* r, const gchar* spath, const gchar* ntext, gpointer udata);
static void managePlaylistTreeCreate (managepltree_t *managepltree);
static bool managePlaylistTreeHideUnselectedCallback (void *udata);
static void managePlaylistTreeSelectCheck (GtkTreeView* tv, GtkTreePath* path,
    GtkTreeViewColumn* column, gpointer udata);

managepltree_t *
managePlaylistTreeAlloc (UIWidget *statusMsg)
{
  managepltree_t *managepltree;

  managepltree = mdmalloc (sizeof (managepltree_t));
  managepltree->uitree = NULL;
  managepltree->danceselcol = NULL;
  managepltree->dancecol = NULL;
  managepltree->countcol = NULL;
  managepltree->lowbpmcol = NULL;
  managepltree->highbpmcol = NULL;
  managepltree->statusMsg = statusMsg;
  managepltree->playlist = NULL;
  managepltree->changed = false;
  managepltree->hideunselected = false;
  managepltree->inprepop = false;
  managepltree->unselcb = NULL;

  return managepltree;
}

void
managePlaylistTreeFree (managepltree_t *managepltree)
{
  if (managepltree != NULL) {
    uiTreeViewFree (managepltree->uitree);
    callbackFree (managepltree->unselcb);
    mdfree (managepltree);
  }
}

void
manageBuildUIPlaylistTree (managepltree_t *managepltree, UIWidget *vboxp,
    UIWidget *tophbox)
{
  UIWidget    hbox;
  UIWidget    uiwidget;
  UIWidget    *uitreewidgetp;
  UIWidget    scwindow;
  GtkCellRenderer *renderer = NULL;
  GtkTreeViewColumn *column = NULL;
  const char  *bpmstr;
  char        tbuff [100];

  uiCreateHorizBox (&hbox);
  uiBoxPackEnd (tophbox, &hbox);

  /* CONTEXT: playlist management: hide unselected dances */
  uiCreateCheckButton (&uiwidget, _("Hide Unselected"), 0);
  managepltree->unselcb = callbackInit (
      managePlaylistTreeHideUnselectedCallback, managepltree, NULL);
  uiToggleButtonSetCallback (&uiwidget, managepltree->unselcb);
  uiBoxPackStart (&hbox, &uiwidget);
  uiutilsUIWidgetCopy (&managepltree->uihideunsel, &uiwidget);

  uiCreateScrolledWindow (&scwindow, 300);
  uiWidgetExpandVert (&scwindow);
  uiBoxPackStartExpand (vboxp, &scwindow);

  managepltree->uitree = uiCreateTreeView ();
  uitreewidgetp = uiTreeViewGetUIWidget (managepltree->uitree);
  uiWidgetExpandVert (uitreewidgetp);
  uiBoxPackInWindow (&scwindow, uitreewidgetp);

  uiTreeViewEnableHeaders (managepltree->uitree);
  g_signal_connect (uitreewidgetp->widget, "row-activated",
      G_CALLBACK (managePlaylistTreeSelectCheck), managepltree);

  renderer = gtk_cell_renderer_toggle_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "active", MPLTREE_COL_DANCE_SELECT,
      NULL);
  managepltree->danceselcol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (uitreewidgetp->widget), column);
  g_signal_connect (G_OBJECT(renderer), "toggled",
      G_CALLBACK (managePlaylistTreeToggleDance), managepltree);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_DANCE,
      NULL);
  managepltree->dancecol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_column_set_title (column, tagdefs [TAG_DANCE].displayname);
  gtk_tree_view_append_column (GTK_TREE_VIEW (uitreewidgetp->widget), column);

  renderer = gtk_cell_renderer_spin_new ();
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
  g_object_set_data (G_OBJECT (renderer), "mpltreecolumn",
      GUINT_TO_POINTER (MPLTREE_COL_COUNT));
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_COUNT,
      "editable", MPLTREE_COL_EDITABLE,
      "adjustment", MPLTREE_COL_ADJUST,
      "digits", MPLTREE_COL_DIGITS,
      NULL);
  managepltree->countcol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  /* CONTEXT: playlist management: count column header */
  gtk_tree_view_column_set_title (column, _("Count"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (uitreewidgetp->widget), column);
  g_signal_connect (renderer, "edited",
      G_CALLBACK (managePlaylistTreeEditInt), managepltree);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set_data (G_OBJECT (renderer), "mpltreecolumn",
      GUINT_TO_POINTER (MPLTREE_COL_MAXPLAYTIME));
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_MAXPLAYTIME,
      "editable", MPLTREE_COL_EDITABLE,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  /* CONTEXT: playlist management: max play time column header (keep short) */
  gtk_tree_view_column_set_title (column, _("Maximum\nPlay Time"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (uitreewidgetp->widget), column);
  g_signal_connect (renderer, "edited",
      G_CALLBACK (managePlaylistTreeEditTime), managepltree);

  renderer = gtk_cell_renderer_spin_new ();
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
  g_object_set_data (G_OBJECT (renderer), "mpltreecolumn",
      GUINT_TO_POINTER (MPLTREE_COL_LOWBPM));
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_LOWBPM,
      "editable", MPLTREE_COL_EDITABLE,
      "adjustment", MPLTREE_COL_ADJUST,
      "digits", MPLTREE_COL_DIGITS,
      NULL);
  managepltree->lowbpmcol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);

  bpmstr = tagdefs [TAG_BPM].displayname;

  /* CONTEXT: playlist management: low bpm/mpm column header */
  snprintf (tbuff, sizeof (tbuff), _("Low %s"), bpmstr);
  gtk_tree_view_column_set_title (column, tbuff);
  gtk_tree_view_append_column (GTK_TREE_VIEW (uitreewidgetp->widget), column);
  g_signal_connect (renderer, "edited",
      G_CALLBACK (managePlaylistTreeEditInt), managepltree);

  renderer = gtk_cell_renderer_spin_new ();
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
  g_object_set_data (G_OBJECT (renderer), "mpltreecolumn",
      GUINT_TO_POINTER (MPLTREE_COL_HIGHBPM));
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_HIGHBPM,
      "editable", MPLTREE_COL_EDITABLE,
      "adjustment", MPLTREE_COL_ADJUST,
      "digits", MPLTREE_COL_DIGITS,
      NULL);
  managepltree->highbpmcol = column;
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);

  /* CONTEXT: playlist management: high bpm/mpm column */
  snprintf (tbuff, sizeof (tbuff), _("High %s"), bpmstr);
  gtk_tree_view_column_set_title (column, tbuff);
  gtk_tree_view_append_column (GTK_TREE_VIEW (uitreewidgetp->widget), column);
  g_signal_connect (renderer, "edited",
      G_CALLBACK (managePlaylistTreeEditInt), managepltree);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
      "text", MPLTREE_COL_SB_PAD,
      NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW (uitreewidgetp->widget), column);

  managePlaylistTreeCreate (managepltree);
}

void
managePlaylistTreePrePopulate (managepltree_t *managepltree, playlist_t *pl)
{
  int     pltype;
  bool    hideunselstate = false;

  managepltree->playlist = pl;
  managepltree->inprepop = true;
  pltype = playlistGetConfigNum (pl, PLAYLIST_TYPE);

  if (pltype == PLTYPE_SONGLIST) {
    uiWidgetDisable (&managepltree->uihideunsel);
  }
  if (pltype == PLTYPE_SEQUENCE) {
    /* a sequence has no need to display the non-selected dances */
    hideunselstate = true;
    uiWidgetDisable (&managepltree->uihideunsel);
  }
  if (pltype == PLTYPE_AUTO) {
    uiWidgetEnable (&managepltree->uihideunsel);
  }
  uiToggleButtonSetState (&managepltree->uihideunsel, hideunselstate);
  managepltree->inprepop = false;
}

void
managePlaylistTreePopulate (managepltree_t *managepltree, playlist_t *pl)
{
  dance_t       *dances;
  int           pltype;
  ilistidx_t    iteridx;
  ilistidx_t    dkey;
  GtkTreeModel  *model;
  GtkTreeIter   iter;
  int           count;
  char          tbuff [40];
  UIWidget      *uiwidgetp;

  dances = bdjvarsdfGet (BDJVDF_DANCES);

  managepltree->playlist = pl;
  pltype = playlistGetConfigNum (pl, PLAYLIST_TYPE);

  managePlaylistTreeSetColumnVisibility (managepltree, pltype);

  uiwidgetp = uiTreeViewGetUIWidget (managepltree->uitree);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uiwidgetp->widget));

  danceStartIterator (dances, &iteridx);
  count = 0;
  while ((dkey = danceIterate (dances, &iteridx)) >= 0) {
    long  sel, dcount, bpmlow, bpmhigh, mpt;
    char  mptdisp [40];

    sel = playlistGetDanceNum (pl, dkey, PLDANCE_SELECTED);

    if (managepltree->hideunselected && pl != NULL) {
      if (! sel) {
        /* don't display this one */
        continue;
      }
    }

    dcount = playlistGetDanceNum (pl, dkey, PLDANCE_COUNT);
    if (dcount < 0) { dcount = 0; }
    mpt = playlistGetDanceNum (pl, dkey, PLDANCE_MAXPLAYTIME);
    if (mpt < 0) { mpt = 0; }
    tmutilToMS (mpt, mptdisp, sizeof (mptdisp));
    bpmlow = playlistGetDanceNum (pl, dkey, PLDANCE_BPM_LOW);
    if (bpmlow < 0) { bpmlow = 0; }
    bpmhigh = playlistGetDanceNum (pl, dkey, PLDANCE_BPM_HIGH);
    if (bpmhigh < 0) { bpmhigh = 0; }

    snprintf (tbuff, sizeof (tbuff), "%d", count);
    if (gtk_tree_model_get_iter_from_string (model, &iter, tbuff)) {
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
          MPLTREE_COL_DANCE_SELECT, (glong) sel,
          MPLTREE_COL_MAXPLAYTIME, mptdisp,
          MPLTREE_COL_COUNT, (glong) dcount,
          MPLTREE_COL_LOWBPM, (glong) bpmlow,
          MPLTREE_COL_HIGHBPM, (glong) bpmhigh,
          -1);
    }
    ++count;
  }

  managepltree->changed = false;
}


bool
managePlaylistTreeIsChanged (managepltree_t *managepltree)
{
  return managepltree->changed;
}

void
managePlaylistTreeUpdatePlaylist (managepltree_t *managepltree)
{
  GtkTreeModel    *model;
  GtkTreeIter     iter;
  glong            tval;
  char            *tstr;
  char            tbuff [40];
  ilistidx_t      dkey;
  playlist_t      *pl;
  int             count;
  UIWidget        *uiwidgetp;

  if (! managepltree->changed) {
    return;
  }

  pl = managepltree->playlist;

  /* hide unselected may be on, and only the displayed dances will */
  /* be updated */

  uiwidgetp = uiTreeViewGetUIWidget (managepltree->uitree);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uiwidgetp->widget));
  count = 0;
  while (1) {
    snprintf (tbuff, sizeof (tbuff), "%d", count);
    if (! gtk_tree_model_get_iter_from_string (model, &iter, tbuff)) {
      break;
    }

    gtk_tree_model_get (model, &iter, MPLTREE_COL_DANCE_IDX, &tval, -1);
    dkey = tval;
    gtk_tree_model_get (model, &iter, MPLTREE_COL_DANCE_SELECT, &tval, -1);
    playlistSetDanceNum (pl, dkey, PLDANCE_SELECTED, tval);
    gtk_tree_model_get (model, &iter, MPLTREE_COL_COUNT, &tval, -1);
    playlistSetDanceNum (pl, dkey, PLDANCE_COUNT, tval);
    gtk_tree_model_get (model, &iter, MPLTREE_COL_MAXPLAYTIME, &tstr, -1);
    tval = tmutilStrToMS (tstr);
    playlistSetDanceNum (pl, dkey, PLDANCE_MAXPLAYTIME, tval);
    gtk_tree_model_get (model, &iter, MPLTREE_COL_LOWBPM, &tval, -1);
    playlistSetDanceNum (pl, dkey, PLDANCE_BPM_LOW, tval);
    gtk_tree_model_get (model, &iter, MPLTREE_COL_HIGHBPM, &tval, -1);
    playlistSetDanceNum (pl, dkey, PLDANCE_BPM_HIGH, tval);

    ++count;
  }
}

/* internal routines */

static void
managePlaylistTreeSetColumnVisibility (managepltree_t *managepltree, int pltype)
{
  switch (pltype) {
    case PLTYPE_SONGLIST: {
      gtk_tree_view_column_set_visible (managepltree->danceselcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->countcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->lowbpmcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->highbpmcol, FALSE);
      break;
    }
    case PLTYPE_AUTO: {
      gtk_tree_view_column_set_visible (managepltree->danceselcol, TRUE);
      gtk_tree_view_column_set_visible (managepltree->countcol, TRUE);
      gtk_tree_view_column_set_visible (managepltree->lowbpmcol, TRUE);
      gtk_tree_view_column_set_visible (managepltree->highbpmcol, TRUE);
      break;
    }
    case PLTYPE_SEQUENCE: {
      gtk_tree_view_column_set_visible (managepltree->danceselcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->countcol, FALSE);
      gtk_tree_view_column_set_visible (managepltree->lowbpmcol, TRUE);
      gtk_tree_view_column_set_visible (managepltree->highbpmcol, TRUE);
      break;
    }
  }
}

static void
managePlaylistTreeToggleDance (GtkCellRendererToggle *renderer, gchar *spath, gpointer udata)
{
  managepltree_t  *managepltree = udata;
  GtkTreeModel    *model;
  GtkTreeIter   iter;
  gint          val;
  int           col = MPLTREE_COL_DANCE_SELECT;
  glong         dkey;
  UIWidget        *uiwidgetp;

  uiwidgetp = uiTreeViewGetUIWidget (managepltree->uitree);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uiwidgetp->widget));
  gtk_tree_model_get_iter_from_string (model, &iter, spath);

  gtk_tree_model_get (model, &iter, col, &val, -1);
  val = ! val;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, val, -1);

  /* must update the playlist for hide-unselected */
  gtk_tree_model_get (model, &iter, MPLTREE_COL_DANCE_IDX, &dkey, -1);
  playlistSetDanceNum (managepltree->playlist, dkey, PLDANCE_SELECTED, val);

  managepltree->changed = true;
}

static void
managePlaylistTreeEditInt (GtkCellRendererText* r, const gchar* path,
    const gchar* ntext, gpointer udata)
{
  managepltree_t  *managepltree = udata;
  GtkTreeModel    *model;
  GtkTreeIter     iter;
  int             col;
  long            val;
  UIWidget        *uiwidgetp;

  uiwidgetp = uiTreeViewGetUIWidget (managepltree->uitree);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uiwidgetp->widget));
  gtk_tree_model_get_iter_from_string (model, &iter, path);
  col = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (r), "mpltreecolumn"));
  val = atol (ntext);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, val, -1);
  managepltree->changed = true;
}


static void
managePlaylistTreeEditTime (GtkCellRendererText* r, const gchar* spath,
    const gchar* ntext, gpointer udata)
{
  managepltree_t  *managepltree = udata;
  GtkTreeModel    *model;
  GtkTreeIter     iter;
  const char      *valstr;
  char            tbuff [200];
  int             col;
  UIWidget        *uiwidgetp;

  uiwidgetp = uiTreeViewGetUIWidget (managepltree->uitree);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uiwidgetp->widget));
  gtk_tree_model_get_iter_from_string (model, &iter, spath);
  col = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (r), "mpltreecolumn"));

  valstr = validate (ntext, VAL_MIN_SEC);
  if (valstr == NULL) {
    /* do not need to convert it at this time */
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, ntext, -1);
    managepltree->changed = true;
  } else {
    snprintf (tbuff, sizeof (tbuff), valstr, ntext);
    uiLabelSetText (managepltree->statusMsg, tbuff);
  }
}

static void
managePlaylistTreeCreate (managepltree_t *managepltree)
{
  dance_t       *dances;
  GtkTreeIter   iter;
  slist_t       *dancelist;
  slistidx_t    iteridx;
  ilistidx_t    key;
  uitree_t      *uitree;
  UIWidget      *uiwidgetp;
  GtkListStore  *store;
  GtkAdjustment *adjustment;

  uitree = managepltree->uitree;

  store = gtk_list_store_new (MPLTREE_COL_MAX,
      TREE_TYPE_BOOLEAN,  // dance select
      TREE_TYPE_STRING,   // dance
      TREE_TYPE_NUM,      // count
      TREE_TYPE_STRING,   // max play time
      TREE_TYPE_NUM,      // low bpm
      TREE_TYPE_NUM,      // high bpm
      TREE_TYPE_STRING,   // pad
      TREE_TYPE_NUM,      // dance idx
      TREE_TYPE_NUM,      // editable
      TREE_TYPE_WIDGET,   // adjust
      TREE_TYPE_NUM);     // digits

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  dancelist = danceGetDanceList (dances);

  slistStartIterator (dancelist, &iteridx);
  while ((key = slistIterateValueNum (dancelist, &iteridx)) >= 0) {
    char        *dancedisp;

    dancedisp = danceGetStr (dances, key, DANCE_DANCE);

    if (managepltree->hideunselected &&
        managepltree->playlist != NULL) {
      int     sel;

      sel = playlistGetDanceNum (managepltree->playlist, key, PLDANCE_SELECTED);
      if (! sel) {
        /* don't display this one */
        continue;
      }
    }

    gtk_list_store_append (store, &iter);
    adjustment = gtk_adjustment_new (0, 0.0, 200.0, 1.0, 5.0, 0.0);
    gtk_list_store_set (store, &iter,
        MPLTREE_COL_DANCE_SELECT, 0,
        MPLTREE_COL_DANCE, dancedisp,
        MPLTREE_COL_MAXPLAYTIME, "0:00",
        MPLTREE_COL_LOWBPM, 0,
        MPLTREE_COL_HIGHBPM, 0,
        MPLTREE_COL_SB_PAD, "  ",
        MPLTREE_COL_DANCE_IDX, key,
        MPLTREE_COL_EDITABLE, 1,
        MPLTREE_COL_ADJUST, adjustment,
        MPLTREE_COL_DIGITS, 0,
        -1);
  }

  uiwidgetp = uiTreeViewGetUIWidget (uitree);
  gtk_tree_view_set_model (GTK_TREE_VIEW (uiwidgetp->widget), GTK_TREE_MODEL (store));
  g_object_unref (store);
}

static bool
managePlaylistTreeHideUnselectedCallback (void *udata)
{
  managepltree_t  *managepltree = udata;
  bool            tchg;

  tchg = managepltree->changed;
  if (managepltree->inprepop) {
    managepltree->hideunselected =
        uiToggleButtonIsActive (&managepltree->uihideunsel);
  }
  if (! managepltree->inprepop) {
    managepltree->hideunselected = ! managepltree->hideunselected;
  }
  managePlaylistTreeCreate (managepltree);
  managePlaylistTreePopulate (managepltree, managepltree->playlist);
  managepltree->changed = tchg;
  return UICB_CONT;
}

static void
managePlaylistTreeSelectCheck (GtkTreeView* tv, GtkTreePath* path,
    GtkTreeViewColumn* column, gpointer udata)
{
  managepltree_t  *managepltree = udata;
  GtkTreeModel    *model;
  GtkTreeIter     iter;
  gint            val;
  int             col = MPLTREE_COL_DANCE_SELECT;
  UIWidget        *uiwidgetp;

  if (column != managepltree->dancecol) {
    return;
  }

  uiwidgetp = uiTreeViewGetUIWidget (managepltree->uitree);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (uiwidgetp->widget));
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, col, &val, -1);
  val = ! val;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, col, val, -1);
}
