/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <stdarg.h>

#include "audiosrc.h"
#include "bdj4.h"
#include "bdj4intl.h"
#include "bdjstring.h"
#include "bdjopt.h"
#include "bdjvarsdf.h"
#include "configui.h"
#include "dance.h"
#include "datafile.h"
#include "dnctypes.h"
#include "fileop.h"
#include "ilist.h"
#include "log.h"
#include "mdebug.h"
#include "nlist.h"
#include "pathutil.h"
#include "slist.h"
#include "ui.h"
#include "uiselectfile.h"
#include "uivirtlist.h"

/* dance table */
static void confuiCreateDanceTable (confuigui_t *gui);
static int  confuiDanceEntryDanceChg (uiwcont_t *entry, const char *label, void *udata);
static int  confuiDanceEntryTagsChg (uiwcont_t *entry, const char *label, void *udata);
static int  confuiDanceEntryAnnouncementChg (uiwcont_t *entry, const char *label, void *udata);
static int  confuiDanceEntryChg (uiwcont_t *e, void *udata, int widx);
static bool confuiDanceSpinboxTypeChg (void *udata);
static bool confuiDanceSpinboxSpeedChg (void *udata);
static bool confuiDanceSpinboxLowMPMChg (void *udata);
static bool confuiDanceSpinboxHighMPMChg (void *udata);
static bool confuiDanceSpinboxTimeSigChg (void *udata);
static void confuiDanceSpinboxChg (void *udata, int widx);
static int  confuiDanceValidateAnnouncement (uiwcont_t *entry, confuigui_t *gui);
static void confuiDanceSave (confuigui_t *gui);
static void confuiLoadDanceTypeList (confuigui_t *gui);
static void confuiDanceFillRow (void *udata, uivirtlist_t *vl, int32_t rownum);
static void confuiDanceSelect (void *udata, uivirtlist_t *vl, int32_t rownum, int colidx);

void
confuiInitEditDances (confuigui_t *gui)
{
  confuiLoadDanceTypeList (gui);

  confuiSpinboxTextInitDataNum (gui, "cu-dance-speed",
      CONFUI_SPINBOX_DANCE_SPEED,
      /* CONTEXT: configuration: dance speed */
      DANCE_SPEED_SLOW, _("slow"),
      /* CONTEXT: configuration: dance speed */
      DANCE_SPEED_NORMAL, _("normal"),
      /* CONTEXT: configuration: dance speed */
      DANCE_SPEED_FAST, _("fast"),
      -1);

  confuiSpinboxTextInitDataNum (gui, "cu-dance-time-sig",
      CONFUI_SPINBOX_DANCE_TIME_SIG,
      /* CONTEXT: configuration: dance time signature */
      DANCE_TIMESIG_24, _("2/4"),
      /* CONTEXT: configuration: dance time signature */
      DANCE_TIMESIG_34, _("3/4"),
      /* CONTEXT: configuration: dance time signature */
      DANCE_TIMESIG_44, _("4/4"),
      -1);
}


void
confuiBuildUIEditDances (confuigui_t *gui)
{
  uiwcont_t     *vbox;
  uiwcont_t     *hbox;
  uiwcont_t     *dvbox;
  uiwcont_t     *szgrp;
  uiwcont_t     *szgrpB;
  uiwcont_t     *szgrpC;
  const char    *bpmstr;
  char          tbuff [MAXPATHLEN];
  uivirtlist_t  *uivl;

  logProcBegin ();
  gui->inchange = true;
  vbox = uiCreateVertBox ();

  szgrp = uiCreateSizeGroupHoriz ();
  szgrpB = uiCreateSizeGroupHoriz ();
  szgrpC = uiCreateSizeGroupHoriz ();

  /* edit dances */
  confuiMakeNotebookTab (vbox, gui,
      /* CONTEXT: configuration: edit the dance table */
      _("Edit Dances"), CONFUI_ID_DANCE);

  hbox = uiCreateHorizBox ();
  uiBoxPackStartExpand (vbox, hbox);
  uiWidgetAlignHorizStart (hbox);

  confuiMakeItemTable (gui, hbox, CONFUI_ID_DANCE, CONFUI_TABLE_NO_UP_DOWN);
  gui->tables [CONFUI_ID_DANCE].savefunc = confuiDanceSave;

  confuiCreateDanceTable (gui);

  dvbox = uiCreateVertBox ();
  uiWidgetSetMarginStart (dvbox, 8);
  uiBoxPackStart (hbox, dvbox);

  confuiMakeItemEntry (gui, dvbox, szgrp, tagdefs [TAG_DANCE].displayname,
      CONFUI_ENTRY_DANCE_DANCE, -1, "", CONFUI_NO_INDENT);
  uiEntrySetValidate (gui->uiitem [CONFUI_ENTRY_DANCE_DANCE].uiwidgetp,
      "", confuiDanceEntryDanceChg, gui, UIENTRY_IMMEDIATE);
  gui->uiitem [CONFUI_ENTRY_DANCE_DANCE].danceidx = DANCE_DANCE;

  /* CONTEXT: configuration: dances: the type of the dance (club/latin/standard) */
  confuiMakeItemSpinboxText (gui, dvbox, szgrp, szgrpB, _("Type"),
      CONFUI_SPINBOX_DANCE_TYPE, -1, CONFUI_OUT_NUM, 0,
      confuiDanceSpinboxTypeChg);
  gui->uiitem [CONFUI_SPINBOX_DANCE_TYPE].danceidx = DANCE_TYPE;

  /* CONTEXT: configuration: dances: the speed of the dance (fast/normal/slow) */
  confuiMakeItemSpinboxText (gui, dvbox, szgrp, szgrpB, _("Speed"),
      CONFUI_SPINBOX_DANCE_SPEED, -1, CONFUI_OUT_NUM, 0,
      confuiDanceSpinboxSpeedChg);
  gui->uiitem [CONFUI_SPINBOX_DANCE_SPEED].danceidx = DANCE_SPEED;

  /* CONTEXT: configuration: dances: tags associated with the dance */
  confuiMakeItemEntry (gui, dvbox, szgrp, _("Tags"),
      CONFUI_ENTRY_DANCE_TAGS, -1, "", CONFUI_NO_INDENT);
  uiEntrySetValidate (gui->uiitem [CONFUI_ENTRY_DANCE_TAGS].uiwidgetp,
      "", confuiDanceEntryTagsChg, gui, UIENTRY_IMMEDIATE);
  gui->uiitem [CONFUI_ENTRY_DANCE_TAGS].danceidx = DANCE_TAGS;

  /* CONTEXT: configuration: dances: play the selected announcement before the dance is played */
  confuiMakeItemEntryChooser (gui, dvbox, szgrp, _("Announcement"),
      CONFUI_ENTRY_CHOOSE_DANCE_ANNOUNCEMENT, -1, "",
      selectAudioFileCallback);
  uiEntrySetValidate (gui->uiitem [CONFUI_ENTRY_CHOOSE_DANCE_ANNOUNCEMENT].uiwidgetp,
      "", confuiDanceEntryAnnouncementChg, gui, UIENTRY_DELAYED);
  gui->uiitem [CONFUI_ENTRY_CHOOSE_DANCE_ANNOUNCEMENT].danceidx = DANCE_ANNOUNCE;

  bpmstr = tagdefs [TAG_BPM].displayname;
  /* CONTEXT: configuration: dances: low BPM (or MPM) setting */
  snprintf (tbuff, sizeof (tbuff), _("Low %s"), bpmstr);
  confuiMakeItemSpinboxNum (gui, dvbox, szgrp, szgrpC, tbuff,
      CONFUI_WIDGET_DANCE_MPM_LOW, -1, 10, 500, 0,
      confuiDanceSpinboxLowMPMChg);
  gui->uiitem [CONFUI_WIDGET_DANCE_MPM_LOW].danceidx = DANCE_MPM_LOW;

  /* CONTEXT: configuration: dances: high BPM (or MPM) setting */
  snprintf (tbuff, sizeof (tbuff), _("High %s"), bpmstr);
  confuiMakeItemSpinboxNum (gui, dvbox, szgrp, szgrpC, tbuff,
      CONFUI_WIDGET_DANCE_MPM_HIGH, -1, 10, 500, 0,
      confuiDanceSpinboxHighMPMChg);
  gui->uiitem [CONFUI_WIDGET_DANCE_MPM_HIGH].danceidx = DANCE_MPM_HIGH;

  /* CONTEXT: configuration: dances: time signature for the dance */
  confuiMakeItemSpinboxText (gui, dvbox, szgrp, szgrpC, _("Time Signature"),
      CONFUI_SPINBOX_DANCE_TIME_SIG, -1, CONFUI_OUT_NUM, 0,
      confuiDanceSpinboxTimeSigChg);
  gui->uiitem [CONFUI_SPINBOX_DANCE_TIME_SIG].danceidx = DANCE_TIMESIG;

  uivlSetSelectionCallback (gui->tables [CONFUI_ID_DANCE].uivl,
      confuiDanceSelect, gui);
//  uiTreeViewSetRowActivatedCallback (gui->tables [CONFUI_ID_DANCE].uitree,
//      gui->tables [CONFUI_ID_DANCE].callbacks [CONFUI_TABLE_CB_DANCE_SELECT]);

  gui->inchange = false;

  uiTreeViewSelectSet (gui->tables [CONFUI_ID_DANCE].uitree, 0);
  uivl = gui->tables [CONFUI_ID_DANCE].uivl;
  confuiDanceSelect (gui, uivl, 0, CONFUI_DANCE_COL_DANCE);

  uiwcontFree (dvbox);
  uiwcontFree (vbox);
  uiwcontFree (hbox);
  uiwcontFree (szgrp);
  uiwcontFree (szgrpB);
  uiwcontFree (szgrpC);

  logProcEnd ("");
}

void
confuiDanceSelectLoadValues (confuigui_t *gui, ilistidx_t danceIdx)
{
  dance_t         *dances;
  const char      *sval;
  slist_t         *slist;
  datafileconv_t  conv;
  int             widx;
  nlistidx_t      num;
  int             timesig;

  dances = bdjvarsdfGet (BDJVDF_DANCES);

  sval = danceGetStr (dances, danceIdx, DANCE_DANCE);
  widx = CONFUI_ENTRY_DANCE_DANCE;
  uiEntrySetValue (gui->uiitem [widx].uiwidgetp, sval);
  /* because the same entry field is used when switching dances, */
  /* and there is a validation timer running, */
  /* the validation timer must be cleared */
  /* the entry field does not need to be validated when being loaded */
  /* this applies to the dance, tags and announcement */
  uiEntryValidateClear (gui->uiitem [widx].uiwidgetp);

  slist = danceGetList (dances, danceIdx, DANCE_TAGS);
  conv.list = slist;
  conv.invt = VALUE_LIST;
  convTextList (&conv);
  sval = conv.strval;
  widx = CONFUI_ENTRY_DANCE_TAGS;
  uiEntrySetValue (gui->uiitem [widx].uiwidgetp, sval);
  dataFree (conv.strval);
  uiEntryValidateClear (gui->uiitem [widx].uiwidgetp);

  timesig = danceGetTimeSignature (danceIdx);

  sval = danceGetStr (dances, danceIdx, DANCE_ANNOUNCE);
  widx = CONFUI_ENTRY_CHOOSE_DANCE_ANNOUNCEMENT;
  uiEntrySetValue (gui->uiitem [widx].uiwidgetp, sval);
  uiEntryValidateClear (gui->uiitem [widx].uiwidgetp);

  num = danceGetNum (dances, danceIdx, DANCE_MPM_HIGH);
  widx = CONFUI_WIDGET_DANCE_MPM_HIGH;
  num = danceConvertMPMtoBPM (danceIdx, num);
  uiSpinboxSetValue (gui->uiitem [widx].uiwidgetp, num);

  num = danceGetNum (dances, danceIdx, DANCE_MPM_LOW);
  widx = CONFUI_WIDGET_DANCE_MPM_LOW;
  num = danceConvertMPMtoBPM (danceIdx, num);
  uiSpinboxSetValue (gui->uiitem [widx].uiwidgetp, num);

  num = danceGetNum (dances, danceIdx, DANCE_SPEED);
  widx = CONFUI_SPINBOX_DANCE_SPEED;
  uiSpinboxTextSetValue (gui->uiitem [widx].uiwidgetp, num);

  widx = CONFUI_SPINBOX_DANCE_TIME_SIG;
  uiSpinboxTextSetValue (gui->uiitem [widx].uiwidgetp, timesig);

  num = danceGetNum (dances, danceIdx, DANCE_TYPE);
  widx = CONFUI_SPINBOX_DANCE_TYPE;
  uiSpinboxTextSetValue (gui->uiitem [widx].uiwidgetp, num);
}

/* internal routines */

static void
confuiCreateDanceTable (confuigui_t *gui)
{
  dance_t           *dances;
  uivirtlist_t      *uivl;


  logProcBegin ();

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  uivl = gui->tables [CONFUI_ID_DANCE].uivl;
  uivlSetNumColumns (uivl, CONFUI_DANCE_COL_MAX);
  uivlMakeColumn (uivl, CONFUI_DANCE_COL_DANCE, VL_TYPE_LABEL);
  uivlMakeColumn (uivl, CONFUI_DANCE_COL_DANCE_IDX, VL_TYPE_INTERNAL_NUMERIC);

  uivlSetNumRows (uivl, danceGetCount (dances));
  gui->tables [CONFUI_ID_DANCE].currcount = danceGetCount (dances);
  uivlSetRowFillCallback (uivl, confuiDanceFillRow, gui);
  uivlDisplay (uivl);

  logProcEnd ("");
}

static int
confuiDanceEntryDanceChg (uiwcont_t *entry, const char *label, void *udata)
{
  return confuiDanceEntryChg (entry, udata, CONFUI_ENTRY_DANCE_DANCE);
}

static int
confuiDanceEntryTagsChg (uiwcont_t *entry, const char *label, void *udata)
{
  return confuiDanceEntryChg (entry, udata, CONFUI_ENTRY_DANCE_TAGS);
}

static int
confuiDanceEntryAnnouncementChg (uiwcont_t *entry, const char *label, void *udata)
{
  return confuiDanceEntryChg (entry, udata, CONFUI_ENTRY_CHOOSE_DANCE_ANNOUNCEMENT);
}

static int
confuiDanceEntryChg (uiwcont_t *entry, void *udata, int widx)
{
  confuigui_t     *gui = udata;
  const char      *str;
  uiwcont_t       *uitree;
  int             count;
  ilistidx_t      key;
  dance_t         *dances;
  int             didx;
  datafileconv_t  conv;
  int             entryrc = UIENTRY_ERROR;

  logProcBegin ();
  if (gui->inchange) {
    logProcEnd ("in-dance-select");
    return UIENTRY_OK;
  }

  str = uiEntryGetValue (entry);
  if (str == NULL) {
    logProcEnd ("null-string");
    return UIENTRY_OK;
  }

  didx = gui->uiitem [widx].danceidx;

  uitree = gui->tables [CONFUI_ID_DANCE].uitree;
  count = uiTreeViewSelectGetCount (uitree);
  if (count != 1) {
    logProcEnd ("no-selection");
    return UIENTRY_OK;
  }

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  key = uiTreeViewGetValue (uitree, CONFUI_DANCE_COL_DANCE_IDX);

  if (widx == CONFUI_ENTRY_DANCE_DANCE) {
    uiTreeViewSetValues (uitree,
        CONFUI_DANCE_COL_DANCE, str,
        TREE_VALUE_END);
    danceSetStr (dances, key, didx, str);
    entryrc = UIENTRY_OK;
  }
  if (widx == CONFUI_ENTRY_CHOOSE_DANCE_ANNOUNCEMENT) {
    entryrc = confuiDanceValidateAnnouncement (entry, gui);
    if (entryrc == UIENTRY_OK) {
      danceSetStr (dances, key, didx, str);
    }
  }
  if (widx == CONFUI_ENTRY_DANCE_TAGS) {
    slist_t *slist;

    conv.str = str;
    conv.invt = VALUE_STR;
    convTextList (&conv);
    slist = conv.list;
    danceSetList (dances, key, didx, slist);
    entryrc = UIENTRY_OK;
  }
  if (entryrc == UIENTRY_OK) {
    gui->tables [gui->tablecurr].changed = true;
  }
  logProcEnd ("");
  return entryrc;
}

static bool
confuiDanceSpinboxTypeChg (void *udata)
{
  confuiDanceSpinboxChg (udata, CONFUI_SPINBOX_DANCE_TYPE);
  return UICB_CONT;
}

static bool
confuiDanceSpinboxSpeedChg (void *udata)
{
  confuiDanceSpinboxChg (udata, CONFUI_SPINBOX_DANCE_SPEED);
  return UICB_CONT;
}

static bool
confuiDanceSpinboxLowMPMChg (void *udata)
{
  confuiDanceSpinboxChg (udata, CONFUI_WIDGET_DANCE_MPM_LOW);
  return UICB_CONT;
}

static bool
confuiDanceSpinboxHighMPMChg (void *udata)
{
  confuiDanceSpinboxChg (udata, CONFUI_WIDGET_DANCE_MPM_HIGH);
  return UICB_CONT;
}

static bool
confuiDanceSpinboxTimeSigChg (void *udata)
{
  confuiDanceSpinboxChg (udata, CONFUI_SPINBOX_DANCE_TIME_SIG);
  return UICB_CONT;
}

static void
confuiDanceSpinboxChg (void *udata, int widx)
{
  confuigui_t     *gui = udata;
  uiwcont_t       *uitree;
  int             count;
  double          value;
  long            nval = 0;
  ilistidx_t      key;
  dance_t         *dances;
  int             didx;

  logProcBegin ();
  if (gui->inchange) {
    logProcEnd ("in-dance-select");
    return;
  }

  didx = gui->uiitem [widx].danceidx;

  if (gui->uiitem [widx].basetype == CONFUI_SPINBOX_TEXT) {
    /* text spinbox */
    nval = uiSpinboxTextGetValue (gui->uiitem [widx].uiwidgetp);
  }
  if (gui->uiitem [widx].basetype == CONFUI_SPINBOX_NUM) {
    value = uiSpinboxGetValue (gui->uiitem [widx].uiwidgetp);
    nval = (long) value;
  }

  uitree = gui->tables [CONFUI_ID_DANCE].uitree;
  count = uiTreeViewSelectGetCount (uitree);
  if (count != 1) {
    logProcEnd ("no-selection");
    return;
  }

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  key = uiTreeViewGetValue (uitree, CONFUI_DANCE_COL_DANCE_IDX);
  if (key == DANCE_MPM_HIGH || key == DANCE_MPM_LOW) {
    nval = danceConvertBPMtoMPM (didx, nval, DANCE_NO_FORCE);
  }
  danceSetNum (dances, key, didx, nval);
  gui->tables [gui->tablecurr].changed = true;
  logProcEnd ("");
}

static int
confuiDanceValidateAnnouncement (uiwcont_t *entry, confuigui_t *gui)
{
  int         rc = UIENTRY_ERROR;
  const char  *fn;
  char        nfn [MAXPATHLEN];

  logProcBegin ();
  if (gui->inchange) {
    logProcEnd ("in-dance-select");
    return UIENTRY_OK;
  }

  gui->inchange = true;

  fn = uiEntryGetValue (entry);
  if (fn == NULL) {
    logProcEnd ("bad-fn");
    return UIENTRY_ERROR;
  }

  strlcpy (nfn, fn, sizeof (nfn));
  pathNormalizePath (nfn, sizeof (nfn));

  if (*nfn == '\0') {
    rc = UIENTRY_OK;
  } else {
    const char  *rfn;
    char        ffn [MAXPATHLEN];

    rfn = audiosrcRelativePath (nfn, 0);
    audiosrcFullPath (nfn, ffn, sizeof (ffn), 0, NULL);

    if (fileopFileExists (ffn)) {
      if (strcmp (rfn, nfn) != 0) {
        uiEntrySetValue (entry, rfn);
      }
      rc = UIENTRY_OK;
    }
  }

  /* sanitizeaddress indicates a buffer underflow error */
  /* if tablecurr is set to CONFUI_ID_NONE */
  /* also this validation routine gets called at most any time, but */
  /* the changed flag should only be set for the edit dance tab */
  if (rc == UIENTRY_OK && gui->tablecurr == CONFUI_ID_DANCE) {
    gui->tables [gui->tablecurr].changed = true;
  }

  gui->inchange = false;
  logProcEnd ("");
  return rc;
}

static void
confuiDanceSave (confuigui_t *gui)
{
  dance_t   *dances;

  logProcBegin ();

  if (gui->tables [CONFUI_ID_DANCE].changed == false) {
    logProcEnd ("not-changed");
    return;
  }

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  /* the data is already saved in the dance list; just re-use it */
  danceSave (dances, NULL, -1);
  logProcEnd ("");
}

static void
confuiLoadDanceTypeList (confuigui_t *gui)
{
  nlist_t       *tlist = NULL;
  nlist_t       *llist = NULL;
  dnctype_t     *dnctypes;
  slistidx_t    iteridx;
  const char    *key;
  int           count;

  logProcBegin ();

  tlist = nlistAlloc ("cu-dance-type", LIST_ORDERED, NULL);
  llist = nlistAlloc ("cu-dance-type-l", LIST_ORDERED, NULL);

  dnctypes = bdjvarsdfGet (BDJVDF_DANCE_TYPES);
  dnctypesStartIterator (dnctypes, &iteridx);
  count = 0;
  while ((key = dnctypesIterate (dnctypes, &iteridx)) != NULL) {
    nlistSetStr (tlist, count, key);
    nlistSetNum (llist, count, count);
    ++count;
  }

  gui->uiitem [CONFUI_SPINBOX_DANCE_TYPE].displist = tlist;
  gui->uiitem [CONFUI_SPINBOX_DANCE_TYPE].sbkeylist = llist;
  logProcEnd ("");
}

static void
confuiDanceFillRow (void *udata, uivirtlist_t *vl, int32_t rownum)
{
  confuigui_t *gui = udata;
  dance_t     *dances;
  slist_t     *dancelist;
  const char  *dancedisp;
  slistidx_t  didx;

  dances = bdjvarsdfGet (BDJVDF_DANCES);
  /* dancelist has the correct display order */
  dancelist = danceGetDanceList (dances);
  didx = slistGetNumByIdx (dancelist, rownum);
  dancedisp = danceGetStr (dances, didx, DANCE_DANCE);
  uivlSetRowColumnValue (gui->tables [CONFUI_ID_DANCE].uivl, rownum,
      CONFUI_DANCE_COL_DANCE, dancedisp);
  uivlSetRowColumnNum (gui->tables [CONFUI_ID_DANCE].uivl, rownum,
      CONFUI_DANCE_COL_DANCE_IDX, didx);
}

static void
confuiDanceSelect (void *udata, uivirtlist_t *vl, int32_t rownum, int colidx)
{
  confuigui_t   *gui = udata;
  uivirtlist_t  *uivl;
  ilistidx_t    didx;

  gui->inchange = true;
  uivl = gui->tables [CONFUI_ID_DANCE].uivl;
  if (uivl == NULL) {
    return;
  }

  didx = uivlGetRowColumnNum (uivl, rownum, CONFUI_DANCE_COL_DANCE_IDX);
  confuiDanceSelectLoadValues (gui, didx);
  gui->inchange = false;
}
