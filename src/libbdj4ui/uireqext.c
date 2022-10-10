#include "config.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "audiotag.h"
#include "bdj4.h"
#include "bdj4intl.h"
#include "bdj4ui.h"
#include "bdjopt.h"
#include "fileop.h"
#include "log.h"
#include "musicdb.h"
#include "nlist.h"
#include "slist.h"
#include "song.h"
#include "tagdef.h"
#include "ui.h"
#include "uidance.h"
#include "uireqext.h"

enum {
  UIREQEXT_CB_DIALOG,
  UIREQEXT_CB_DANCE,
  UIREQEXT_CB_AUDIO_FILE,
  UIREQEXT_CB_MAX,
};

typedef struct uireqext {
  UIWidget        *parentwin;
  nlist_t         *options;
  UIWidget        reqextDialog;
  uientry_t       *afEntry;
  UIWidget        artistDisp;
  UIWidget        titleDisp;
  uidance_t       *uidance;
  UICallback      callbacks [UIREQEXT_CB_MAX];
  song_t          *song;
} uireqext_t;

/* request external */
static void   uireqextCreateDialog (uireqext_t *uireqext);
static bool   uireqextAudioFileDialog (void *udata);
static bool   uireqextDanceSelectHandler (void *udata, long idx);
static void   uireqextInitDisplay (uireqext_t *uireqext);
static void   uireqextClearSong (uireqext_t *uireqext);
static bool   uireqextResponseHandler (void *udata, long responseid);
static void   uireqextProcessAudioFile (uireqext_t *uireqext);

uireqext_t *
uireqextInit (UIWidget *windowp, nlist_t *opts)
{
  uireqext_t  *uireqext;

  uireqext = malloc (sizeof (uireqext_t));
  uiutilsUIWidgetInit (&uireqext->reqextDialog);
  uiutilsUIWidgetInit (&uireqext->artistDisp);
  uiutilsUIWidgetInit (&uireqext->titleDisp);
  uireqext->afEntry = uiEntryInit (50, MAXPATHLEN);
  uireqext->uidance = NULL;
  uireqext->parentwin = windowp;
  uireqext->options = opts;
  uireqext->song = NULL;

  return uireqext;
}

void
uireqextFree (uireqext_t *uireqext)
{
  if (uireqext != NULL) {
    if (uireqext->song != NULL) {
      songFree (uireqext->song);
    }
    if (uireqext->afEntry != NULL) {
      uiEntryFree (uireqext->afEntry);
    }
    uiDialogDestroy (&uireqext->reqextDialog);
    uidanceFree (uireqext->uidance);
    free (uireqext);
  }
}

bool
uireqextDialog (uireqext_t *uireqext)
{
  int         x, y;

  if (uireqext == NULL) {
    return UICB_STOP;
  }

  logProcBegin (LOG_PROC, "uireqextDialog");
  uireqextCreateDialog (uireqext);
  uireqextInitDisplay (uireqext);
  uiWidgetShowAll (&uireqext->reqextDialog);

  x = nlistGetNum (uireqext->options, MQ_REQ_EXT_POSITION_X);
  y = nlistGetNum (uireqext->options, MQ_REQ_EXT_POSITION_Y);
  uiWindowMove (&uireqext->reqextDialog, x, y, -1);
  logProcEnd (LOG_PROC, "uireqextDialog", "");
  return UICB_CONT;
}

song_t *
uireqextGetSong (uireqext_t *uireqext)
{
  if (uireqext == NULL) {
    return NULL;
  }

  return uireqext->song;
}

/* internal routines */

static void
uireqextCreateDialog (uireqext_t *uireqext)
{
  UIWidget      vbox;
  UIWidget      hbox;
  UIWidget      uiwidget;
  UIWidget      *uiwidgetp;
  UIWidget      sg;  // labels
  UIWidget      sgA; // title, artist

  logProcBegin (LOG_PROC, "uireqextCreateDialog");

  if (uireqext == NULL) {
    return;
  }

  if (uiutilsUIWidgetSet (&uireqext->reqextDialog)) {
    return;
  }

  uiCreateSizeGroupHoriz (&sg);
  uiCreateSizeGroupHoriz (&sgA);

  uiutilsUICallbackLongInit (&uireqext->callbacks [UIREQEXT_CB_DIALOG],
      uireqextResponseHandler, uireqext);
  uiCreateDialog (&uireqext->reqextDialog, uireqext->parentwin,
      &uireqext->callbacks [UIREQEXT_CB_DIALOG],
      /* CONTEXT: request external dialog: title for the dialog */
      _("Select Audio File"),
      /* CONTEXT: request external dialog: closes the dialog */
      _("Close"),
      RESPONSE_CLOSE,
      /* CONTEXT: request external dialog: queue the selected file */
      _("Queue"),
      RESPONSE_APPLY,
      NULL
      );

  uiCreateVertBox (&vbox);
  uiWidgetSetAllMargins (&vbox, 10);
  uiWidgetSetMarginTop (&vbox, 20);
  uiWidgetExpandHoriz (&vbox);
  uiWidgetExpandVert (&vbox);
  uiDialogPackInDialog (&uireqext->reqextDialog, &vbox);

  uiCreateHorizBox (&hbox);
  uiWidgetExpandHoriz (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  uiCreateColonLabel (&uiwidget,
      /* CONTEXT: request external: enter the audio file location */
      _("Audio File"));
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  uiEntryCreate (uireqext->afEntry);
  uiEntrySetValue (uireqext->afEntry, "");
  uiwidgetp = uiEntryGetUIWidget (uireqext->afEntry);
  uiWidgetAlignHorizFill (uiwidgetp);
  uiWidgetExpandHoriz (uiwidgetp);
  uiBoxPackStartExpand (&hbox, uiwidgetp);
//  uiEntrySetValidate (uireqext->afEntry,
//      uireqextValidateAudioFile, uireqext, UIENTRY_DELAYED);

  uiutilsUICallbackInit (&uireqext->callbacks [UIREQEXT_CB_AUDIO_FILE],
      uireqextAudioFileDialog, uireqext, NULL);
  uiCreateButton (&uiwidget,
      &uireqext->callbacks [UIREQEXT_CB_AUDIO_FILE],
      "", NULL);
  uiButtonSetImageIcon (&uiwidget, "folder");
  uiWidgetSetMarginStart (&uiwidget, 0);
  uiBoxPackStart (&hbox, &uiwidget);

  /* artist display */
  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  uiCreateColonLabel (&uiwidget, tagdefs [TAG_ARTIST].displayname);
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  uiCreateLabel (&uireqext->artistDisp, "");
  uiBoxPackStart (&hbox, &uireqext->artistDisp);
  uiSizeGroupAdd (&sgA, &uireqext->artistDisp);

  /* title display */
  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  uiCreateColonLabel (&uiwidget, tagdefs [TAG_TITLE].displayname);
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  uiCreateLabel (&uireqext->titleDisp, "");
  uiBoxPackStart (&hbox, &uireqext->titleDisp);
  uiSizeGroupAdd (&sgA, &uireqext->titleDisp);

  /* dance : always available */
  uiCreateHorizBox (&hbox);
  uiBoxPackStart (&vbox, &hbox);

  uiCreateColonLabel (&uiwidget, tagdefs [TAG_DANCE].displayname);
  uiBoxPackStart (&hbox, &uiwidget);
  uiSizeGroupAdd (&sg, &uiwidget);

  uiutilsUICallbackLongInit (&uireqext->callbacks [UIREQEXT_CB_DANCE],
      uireqextDanceSelectHandler, uireqext);
  uireqext->uidance = uidanceDropDownCreate (&hbox, &uireqext->reqextDialog,
      /* CONTEXT: request external: dance drop-down */
      UIDANCE_EMPTY_DANCE, _("Select Dance"), UIDANCE_PACK_START);
  uidanceSetCallback (uireqext->uidance, &uireqext->callbacks [UIREQEXT_CB_DANCE]);

  logProcEnd (LOG_PROC, "uireqextCreateDialog", "");
}

static bool
uireqextAudioFileDialog (void *udata)
{
  uireqext_t  *uireqext = udata;
  char        *fn = NULL;
  uiselect_t  *selectdata;

  if (uireqext == NULL) {
    return UICB_STOP;
  }

  selectdata = uiDialogCreateSelect (uireqext->parentwin,
      /* CONTEXT: request external: file selection dialog: window title */
      _("Select File"),
      "",   // ### FIX start path
      NULL,
      /* CONTEXT: request external: file selection dialog: audio file filter */
      _("Audio Files"), "audio/*");
  fn = uiSelectFileDialog (selectdata);
  if (fn != NULL) {
    uiEntrySetValue (uireqext->afEntry, fn);
    logMsg (LOG_INSTALL, LOG_IMPORTANT, "selected loc: %s", fn);
    uireqextProcessAudioFile (uireqext);
    free (fn);
  }
  free (selectdata);

  return UICB_CONT;
}

static bool
uireqextDanceSelectHandler (void *udata, long idx)
{
  return UICB_CONT;
}

static void
uireqextInitDisplay (uireqext_t *uireqext)
{
  if (uireqext == NULL) {
    return;
  }

  uireqextClearSong (uireqext);
  uiEntrySetValue (uireqext->afEntry, "");
  uiLabelSetText (&uireqext->artistDisp, "");
  uiLabelSetText (&uireqext->titleDisp, "");
  uidanceSetValue (uireqext->uidance, -1);
}

static void
uireqextClearSong (uireqext_t *uireqext)
{
  if (uireqext == NULL) {
    return;
  }

  if (uireqext->song != NULL) {
    songFree (uireqext->song);
    uireqext->song = NULL;
  }
}

static bool
uireqextResponseHandler (void *udata, long responseid)
{
  uireqext_t  *uireqext = udata;
  int         x, y, ws;

  uiWindowGetPosition (&uireqext->reqextDialog, &x, &y, &ws);
  nlistSetNum (uireqext->options, MQ_REQ_EXT_POSITION_X, x);
  nlistSetNum (uireqext->options, MQ_REQ_EXT_POSITION_Y, y);

  switch (responseid) {
    case RESPONSE_DELETE_WIN: {
      logMsg (LOG_DBG, LOG_ACTIONS, "= action: reqext: del window");
      uiutilsUIWidgetInit (&uireqext->reqextDialog);
      uireqextClearSong (uireqext);
      break;
    }
    case RESPONSE_CLOSE: {
      logMsg (LOG_DBG, LOG_ACTIONS, "= action: reqext: close window");
      uiWidgetHide (&uireqext->reqextDialog);
      uireqextClearSong (uireqext);
      break;
    }
    case RESPONSE_APPLY: {
      logMsg (LOG_DBG, LOG_ACTIONS, "= action: reqext: apply");
      uiWidgetHide (&uireqext->reqextDialog);
      break;
    }
  }

  return UICB_CONT;
}

static void
uireqextProcessAudioFile (uireqext_t *uireqext)
{
  const char  *ffn;

  if (uireqext == NULL) {
fprintf (stderr, "null-req\n");
    return;
  }

  uireqextClearSong (uireqext);
  ffn = uiEntryGetValue (uireqext->afEntry);
fprintf (stderr, "ffn: %s\n", ffn);
  if (*ffn) {
    if (fileopFileExists (ffn)) {
      char    *data;
      slist_t *tagdata;
      int     rewrite;

      // ### determine if this is a song in the db, if so use the db entry

      data = audiotagReadTags (ffn);
      if (data == NULL) {
        return;
      }

      tagdata = audiotagParseData (ffn, data, &rewrite);
      free (data);
      if (slistGetCount (tagdata) == 0) {
        slistFree (tagdata);
fprintf (stderr, "no tag data\n");
        return;
      }

      if (bdjoptGetNum (OPT_G_LOADDANCEFROMGENRE)) {
        const char  *val;

        val = slistGetStr (tagdata, tagdefs [TAG_DANCE].tag);
        if (val == NULL || ! *val) {
          val = slistGetStr (tagdata, tagdefs [TAG_GENRE].tag);
          if (val != NULL && *val) {
            slistSetStr (tagdata, tagdefs [TAG_DANCE].tag, val);
          }
        }
      }

      uireqext->song = songAlloc ();
fprintf (stderr, "created song\n");
      songSetStr (uireqext->song, TAG_FILE, ffn);
      songSetStr (uireqext->song, TAG_DURATION,
          slistGetStr (tagdata, tagdefs [TAG_DURATION].tag));
      songSetStr (uireqext->song, TAG_DANCE,
          slistGetStr (tagdata, tagdefs [TAG_DANCE].tag));
      songSetStr (uireqext->song, TAG_ARTIST,
          slistGetStr (tagdata, tagdefs [TAG_ARTIST].tag));
      songSetStr (uireqext->song, TAG_TITLE,
          slistGetStr (tagdata, tagdefs [TAG_TITLE].tag));
      songSetNum (uireqext->song, TAG_TEMPORARY, true);

      uiLabelSetText (&uireqext->artistDisp,
          songGetStr (uireqext->song, TAG_ARTIST));
      uiLabelSetText (&uireqext->titleDisp,
          songGetStr (uireqext->song, TAG_TITLE));
    } else {
fprintf (stderr, "no file %s\n", ffn);
    }
  }
}
