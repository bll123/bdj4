/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>

#include "bdj4.h"
#include "bdjstring.h"
#include "bdj4init.h"
#include "bdj4intl.h"
#include "bdjmsg.h"
#include "bdjopt.h"
#include "bdjvars.h"
#include "colorutils.h"
#include "conn.h"
#include "dirlist.h"
#include "dirop.h"
#include "fileop.h"
#include "instutil.h"
#include "lock.h"
#include "log.h"
#include "mdebug.h"
#include "nlist.h"
#include "osprocess.h"
#include "ossignal.h"
#include "osuiutils.h"
#include "pathbld.h"
#include "procutil.h"
#include "progstate.h"
#include "sock.h"
#include "sockh.h"
#include "startutil.h"
#include "support.h"
#include "sysvars.h"
#include "templateutil.h"
#include "ui.h"
#include "callback.h"
#include "uiutils.h"
#include "volreg.h"
#include "webclient.h"

typedef enum {
  START_STATE_NONE,
  START_STATE_DELAY,
  START_STATE_SUPPORT_INIT,
  START_STATE_SUPPORT_SEND_MSG,
  START_STATE_SUPPORT_SEND_INFO,
  START_STATE_SUPPORT_SEND_FILES_DATA,
  START_STATE_SUPPORT_SEND_FILES_PROFILE,
  START_STATE_SUPPORT_SEND_FILES_MACHINE,
  START_STATE_SUPPORT_SEND_FILES_MACH_PROF,
  START_STATE_SUPPORT_SEND_DIAG_INIT,
  START_STATE_SUPPORT_SEND_DIAG,
  START_STATE_SUPPORT_SEND_DB_PRE,
  START_STATE_SUPPORT_SEND_DB,
  START_STATE_SUPPORT_FINISH,
  START_STATE_SUPPORT_SEND_FILE,
} startstate_t;

enum {
  SF_ALL,
  SF_CONF_ONLY,
  SF_MAC_DIAG,
  CLOSE_REQUEST,
  CLOSE_CRASH,
};

enum {
  START_CB_PLAYER,
  START_CB_MANAGE,
  START_CB_CONFIG,
  START_CB_SUPPORT,
  START_CB_EXIT,
  START_CB_SEND_SUPPORT,
  START_CB_MENU_STOP_ALL,
  START_CB_MENU_DEL_PROFILE,
  START_CB_MENU_PROFILE_SHORTCUT,
  START_CB_MENU_ALT_SETUP,
  START_CB_SUPPORT_RESP,
  START_CB_SUPPORT_MSG_RESP,
  START_CB_MAX,
};

enum {
  START_LINK_CB_WIKI,
  START_LINK_CB_FORUM,
  START_LINK_CB_TICKETS,
  START_LINK_CB_DOWNLOAD,
  START_LINK_CB_MAX,
};

enum {
  START_BUTTON_PLAYER,
  START_BUTTON_MANAGE,
  START_BUTTON_CONFIG,
  START_BUTTON_SUPPORT,
  START_BUTTON_EXIT,
  START_BUTTON_SEND_SUPPORT,
  START_BUTTON_MAX,
};

enum {
  START_W_WINDOW,
  START_W_STATUS_DISP,
  START_W_STATUS_MSG,
  START_W_STATUS_DISP_MSG,
  START_W_PROFILE_ACCENT,
  START_W_SUPPORT_DIALOG,
  START_W_SUPPORT_MSG_DIALOG,
  START_W_SUPPORT_SEND_FILES,
  START_W_SUPPORT_SEND_DB,
  START_W_MAX,
};

typedef struct {
  uiwcont_t     *uiwidgetp;
  callback_t    *macoscb;
  char          *uri;
} startlinkinfo_t;

typedef struct {
  progstate_t     *progstate;
  procutil_t      *processes [ROUTE_MAX];
  conn_t          *conn;
  int             currprofile;
  int             newprofile;
  loglevel_t      loglevel;
  int             maxProfileWidth;
  startstate_t    startState;
  startstate_t    nextState;
  startstate_t    delayState;
  int             delayCount;
  time_t          lastPluiStart;
  char            *supportDir;
  slist_t         *supportFileList;
  int             sendType;
  slistidx_t      supportFileIterIdx;
  char            *supportInFname;
  webclient_t     *webclient;
  char            ident [80];
  char            latestversion [60];
  support_t       *support;
  int             mainstart [ROUTE_MAX];
  int             started [ROUTE_MAX];
  int             stopwaitcount;
  mstime_t        pluiCheckTime;
  nlist_t         *proflist;
  nlist_t         *profidxlist;
  callback_t      *callbacks [START_CB_MAX];
  startlinkinfo_t linkinfo [START_LINK_CB_MAX];
  uispinbox_t     *profilesel;
  uibutton_t      *buttons [START_BUTTON_MAX];
  uiwcont_t       *wcont [START_W_MAX];
  uitextbox_t     *supporttb;
  uientry_t       *supportsubject;
  uientry_t       *supportemail;
  /* options */
  datafile_t      *optiondf;
  nlist_t         *options;
  bool            supportactive : 1;
  bool            supportmsgactive : 1;
  bool            optionsalloc : 1;
} startui_t;

enum {
  STARTERUI_POSITION_X,
  STARTERUI_POSITION_Y,
  STARTERUI_SIZE_X,
  STARTERUI_SIZE_Y,
  STARTERUI_KEY_MAX,
};

static datafilekey_t starteruidfkeys [STARTERUI_KEY_MAX] = {
  { "STARTERUI_POS_X",  STARTERUI_POSITION_X,    VALUE_NUM, NULL, DF_NORM },
  { "STARTERUI_POS_Y",  STARTERUI_POSITION_Y,    VALUE_NUM, NULL, DF_NORM },
  { "STARTERUI_SIZE_X", STARTERUI_SIZE_X,        VALUE_NUM, NULL, DF_NORM },
  { "STARTERUI_SIZE_Y", STARTERUI_SIZE_Y,        VALUE_NUM, NULL, DF_NORM },
};

enum {
  LOOP_DELAY = 5,
};

static bool     starterInitDataCallback (void *udata, programstate_t programState);
static bool     starterStoppingCallback (void *udata, programstate_t programState);
static bool     starterStopWaitCallback (void *udata, programstate_t programState);
static bool     starterClosingCallback (void *udata, programstate_t programState);
static void     starterBuildUI (startui_t *starter);
static int      starterMainLoop  (void *tstarter);
static int      starterProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
                    bdjmsgmsg_t msg, char *args, void *udata);
static void     starterCloseProcess (startui_t *starter, bdjmsgroute_t routefrom, int reason);
static void     starterStartMain (startui_t *starter, bdjmsgroute_t routefrom, char *args);
static void     starterStopMain (startui_t *starter, bdjmsgroute_t routefrom);
static bool     starterCloseCallback (void *udata);
static void     starterSigHandler (int sig);

static bool     starterStartPlayerui (void *udata);
static bool     starterStartManageui (void *udata);
static bool     starterStartConfig (void *udata);
static bool     starterStartProcess (startui_t *starter, const char *procname, bdjmsgroute_t route);

static int      starterGetProfiles (startui_t *starter);
static void     starterResetProfile (startui_t *starter, int profidx);
static const char * starterSetProfile (void *udata, int idx);
static int      starterCheckProfile (startui_t *starter);
static bool     starterDeleteProfile (void *udata);
static void     starterRebuildProfileList (startui_t *starter);
static bool     starterCreateProfileShortcut (void *udata);

static void     starterSupportInit (startui_t *starter);
static bool     starterProcessSupport (void *udata);
static void     starterSupportDialogClear (startui_t *starter);
static bool     starterSupportResponseHandler (void *udata, long responseid);
static bool     starterCreateSupportMsgDialog (void *udata);
static void     starterSupportMsgDialogClear (startui_t *starter);
static bool     starterSupportMsgHandler (void *udata, long responseid);
static void     starterSendFilesInit (startui_t *starter, char *dir, int type);
static void     starterSendFiles (startui_t *starter);

static bool     starterStopAllProcessCallback (void *udata);

static bool     starterDownloadLinkHandler (void *udata);
static bool     starterWikiLinkHandler (void *udata);
static bool     starterForumLinkHandler (void *udata);
static bool     starterTicketLinkHandler (void *udata);
static void     starterLinkHandler (void *udata, int cbidx);

static void     starterSaveWindowPosition (startui_t *starter);
static void     starterSetWindowPosition (startui_t *starter);
static void     starterLoadOptions (startui_t *starter);
static bool     starterSetUpAlternate (void *udata);
static void     starterSendPlayerActive (startui_t *starter);

static bool gKillReceived = false;
static bool gNewProfile = false;
static bool gStopProgram = false;

int
main (int argc, char *argv[])
{
  int         status = 0;
  startui_t   starter;
  uint16_t    listenPort;
  long        flags;
  char        uri [MAXPATHLEN];

#if BDJ4_MEM_DEBUG
  mdebugInit ("strt");
#endif

  starter.progstate = progstateInit ("starterui");
  progstateSetCallback (starter.progstate, STATE_INITIALIZE_DATA,
      starterInitDataCallback, &starter);
  progstateSetCallback (starter.progstate, STATE_STOPPING,
      starterStoppingCallback, &starter);
  progstateSetCallback (starter.progstate, STATE_STOP_WAIT,
      starterStopWaitCallback, &starter);
  progstateSetCallback (starter.progstate, STATE_CLOSING,
      starterClosingCallback, &starter);
  starter.conn = NULL;
  starter.maxProfileWidth = 0;
  starter.startState = START_STATE_NONE;
  starter.nextState = START_STATE_NONE;
  starter.delayState = START_STATE_NONE;
  starter.delayCount = 0;
  starter.supportDir = NULL;
  starter.supportFileList = NULL;
  starter.sendType = SF_CONF_ONLY;
  starter.supportInFname = NULL;
  starter.webclient = NULL;
  starter.supporttb = NULL;
  starter.lastPluiStart = mstime ();
  strcpy (starter.ident, "");
  strcpy (starter.latestversion, "");
  for (int i = 0; i < ROUTE_MAX; ++i) {
    starter.mainstart [i] = 0;
    starter.started [i] = 0;
  }
  starter.stopwaitcount = 0;
  mstimeset (&starter.pluiCheckTime, 0);
  starter.proflist = NULL;
  starter.profidxlist = NULL;
  for (int i = 0; i < START_CB_MAX; ++i) {
    starter.callbacks [i] = NULL;
  }
  for (int i = 0; i < START_LINK_CB_MAX; ++i) {
    starter.linkinfo [i].uiwidgetp = NULL;
    starter.linkinfo [i].macoscb = NULL;
    starter.linkinfo [i].uri = NULL;
  }
  for (int i = 0; i < START_BUTTON_MAX; ++i) {
    starter.buttons [i] = NULL;
  }
  for (int i = 0; i < START_W_MAX; ++i) {
    starter.wcont [i] = NULL;
  }
  starter.support = NULL;
  starter.optiondf = NULL;
  starter.options = NULL;
  starter.optionsalloc = false;
  starter.supportsubject = NULL;
  starter.supportemail = NULL;
  starter.supportactive = false;
  starter.supportmsgactive = false;

  starter.callbacks [START_CB_SEND_SUPPORT] = callbackInit (
      starterCreateSupportMsgDialog, &starter, NULL);
  starter.callbacks [START_CB_SUPPORT_MSG_RESP] = callbackInitLong (
      starterSupportMsgHandler, &starter);
  starter.callbacks [START_CB_SUPPORT_RESP] = callbackInitLong (
      starterSupportResponseHandler, &starter);
  starter.callbacks [START_CB_MENU_PROFILE_SHORTCUT] = callbackInit (
      starterCreateProfileShortcut, &starter, NULL);

  snprintf (uri, sizeof (uri), "%s%s",
      sysvarsGetStr (SV_HOST_DOWNLOAD), sysvarsGetStr (SV_URI_DOWNLOAD));
  starter.linkinfo [START_LINK_CB_DOWNLOAD].uri = mdstrdup (uri);
  if (isMacOS ()) {
    starter.linkinfo [START_LINK_CB_DOWNLOAD].macoscb = callbackInit (
        starterDownloadLinkHandler, &starter, NULL);
  }

  snprintf (uri, sizeof (uri), "%s%s",
      sysvarsGetStr (SV_HOST_WIKI), sysvarsGetStr (SV_URI_WIKI));
  starter.linkinfo [START_LINK_CB_WIKI].uri = mdstrdup (uri);
  if (isMacOS ()) {
    starter.linkinfo [START_LINK_CB_WIKI].macoscb = callbackInit (
        starterWikiLinkHandler, &starter, NULL);
  }

  snprintf (uri, sizeof (uri), "%s%s",
      sysvarsGetStr (SV_HOST_FORUM), sysvarsGetStr (SV_URI_FORUM));
  starter.linkinfo [START_LINK_CB_FORUM].uri = mdstrdup (uri);
  if (isMacOS ()) {
    starter.linkinfo [START_LINK_CB_FORUM].macoscb = callbackInit (
        starterForumLinkHandler, &starter, NULL);
  }

  snprintf (uri, sizeof (uri), "%s%s",
      sysvarsGetStr (SV_HOST_TICKET), sysvarsGetStr (SV_URI_TICKET));
  starter.linkinfo [START_LINK_CB_TICKETS].uri = mdstrdup (uri);
  if (isMacOS ()) {
    starter.linkinfo [START_LINK_CB_TICKETS].macoscb = callbackInit (
        starterTicketLinkHandler, &starter, NULL);
  }

  procutilInitProcesses (starter.processes);

  osSetStandardSignals (starterSigHandler);

  flags = BDJ4_INIT_NO_DB_LOAD | BDJ4_INIT_NO_DATAFILE_LOAD |
      BDJ4_INIT_NO_LOCK;
  starter.loglevel = bdj4startup (argc, argv, NULL, "strt",
      ROUTE_STARTERUI, &flags);
  logProcBegin (LOG_PROC, "starterui");

  starter.profilesel = uiSpinboxInit ();

  starterLoadOptions (&starter);

  uiUIInitialize ();
  uiSetUICSS (uiutilsGetCurrentFont (),
      bdjoptGetStr (OPT_P_UI_ACCENT_COL),
      bdjoptGetStr (OPT_P_UI_ERROR_COL));

  starterBuildUI (&starter);
  osuiFinalize ();

  while (! gStopProgram) {
    long loglevel = 0;

    gNewProfile = false;
    listenPort = bdjvarsGetNum (BDJVL_STARTERUI_PORT);
    starter.conn = connInit (ROUTE_STARTERUI);

    sockhMainLoop (listenPort, starterProcessMsg, starterMainLoop, &starter);

    if (gNewProfile) {
      connDisconnectAll (starter.conn);
      connFree (starter.conn);
      logEnd ();
      loglevel = bdjoptGetNum (OPT_G_DEBUGLVL);
      logStart (lockName (ROUTE_STARTERUI), "strt", loglevel);
    }
  }

  connFree (starter.conn);
  progstateFree (starter.progstate);
  logProcEnd (LOG_PROC, "starterui", "");
  logEnd ();
#if BDJ4_MEM_DEBUG
  mdebugReport ();
  mdebugCleanup ();
#endif
  return status;
}

/* internal routines */

static bool
starterInitDataCallback (void *udata, programstate_t programState)
{
  startui_t   *starter = udata;
  char        tbuff [MAXPATHLEN];

  pathbldMakePath (tbuff, sizeof (tbuff),
      NEWINSTALL_FN, BDJ4_CONFIG_EXT, PATHBLD_MP_DREL_DATA);
  if (fileopFileExists (tbuff)) {
    const char  *targv [5];
    int         targc = 0;

    targv [targc++] = NULL;
    starter->processes [ROUTE_HELPERUI] = procutilStartProcess (
        ROUTE_HELPERUI, "bdj4helperui", PROCUTIL_DETACH, targv);
    fileopDelete (tbuff);
  }

  return STATE_FINISHED;
}

static bool
starterStoppingCallback (void *udata, programstate_t programState)
{
  startui_t   *starter = udata;

  logProcBegin (LOG_PROC, "starterStoppingCallback");

  if (starter->started [ROUTE_PLAYERUI]) {
    starterPlayerShutdown ();
  }

  for (int route = 0; route < ROUTE_MAX; ++route) {
    if (starter->started [route] > 0) {
      procutilStopProcess (starter->processes [route],
          starter->conn, route, PROCUTIL_NORM_TERM);
      starter->started [route] = 0;
    }
  }

  starterSaveWindowPosition (starter);

  procutilStopAllProcess (starter->processes, starter->conn, PROCUTIL_NORM_TERM);

  logProcEnd (LOG_PROC, "starterStoppingCallback", "");
  return STATE_FINISHED;
}

static bool
starterStopWaitCallback (void *udata, programstate_t programState)
{
  startui_t   *starter = udata;
  bool        rc;

  rc = connWaitClosed (starter->conn, &starter->stopwaitcount);
  return rc;
}

static bool
starterClosingCallback (void *udata, programstate_t programState)
{
  startui_t   *starter = udata;

  logProcBegin (LOG_PROC, "starterClosingCallback");

  uiCloseWindow (starter->wcont [START_W_WINDOW]);
  uiCleanup ();

  starterSupportMsgDialogClear (starter);
  starterSupportDialogClear (starter);
  for (int i = 0; i < START_W_MAX; ++i) {
    uiwcontFree (starter->wcont [i]);
  }
  for (int i = 0; i < START_BUTTON_MAX; ++i) {
    uiButtonFree (starter->buttons [i]);
  }
  for (int i = 0; i < START_CB_MAX; ++i) {
    callbackFree (starter->callbacks [i]);
  }

  procutilStopAllProcess (starter->processes, starter->conn, PROCUTIL_FORCE_TERM);
  procutilFreeAll (starter->processes);

  bdj4shutdown (ROUTE_STARTERUI, NULL);

  supportFree (starter->support);
  webclientClose (starter->webclient);
  uiTextBoxFree (starter->supporttb);
  uiSpinboxFree (starter->profilesel);

  datafileSave (starter->optiondf, NULL, starter->options, DF_NO_OFFSET, 1);

  for (int i = 0; i < START_LINK_CB_MAX; ++i) {
    uiwcontFree (starter->linkinfo [i].uiwidgetp);
    callbackFree (starter->linkinfo [i].macoscb);
    dataFree (starter->linkinfo [i].uri);
  }
  if (starter->optionsalloc) {
    nlistFree (starter->options);
  }
  datafileFree (starter->optiondf);
  dataFree (starter->supportDir);
  slistFree (starter->supportFileList);
  nlistFree (starter->proflist);
  nlistFree (starter->profidxlist);

  logProcEnd (LOG_PROC, "starterClosingCallback", "");
  return STATE_FINISHED;
}

static void
starterBuildUI (startui_t  *starter)
{
  uiwcont_t   *uiwidgetp;
  uibutton_t  *uibutton;
  uiwcont_t   *menubar;
  uiwcont_t   *menu;
  uiwcont_t   *menuitem;
  uiwcont_t   *vbox;
  uiwcont_t   *bvbox;
  uiwcont_t   *hbox;
  uiwcont_t   *szgrp;
  char        imgbuff [MAXPATHLEN];
  char        tbuff [MAXPATHLEN];
  int         dispidx;
  uiutilsaccent_t accent;

  logProcBegin (LOG_PROC, "starterBuildUI");

  szgrp = uiCreateSizeGroupHoriz ();

  pathbldMakePath (imgbuff, sizeof (imgbuff),
      "bdj4_icon", BDJ4_IMG_SVG_EXT, PATHBLD_MP_DIR_IMG);
  starter->callbacks [START_CB_EXIT] = callbackInit (
      starterCloseCallback, starter, NULL);
  starter->wcont [START_W_WINDOW] = uiCreateMainWindow (
      starter->callbacks [START_CB_EXIT],
      bdjoptGetStr (OPT_P_PROFILENAME), imgbuff);

  vbox = uiCreateVertBox ();
  uiWidgetSetAllMargins (vbox, 2);
  uiBoxPackInWindow (starter->wcont [START_W_WINDOW], vbox);

  uiutilsAddAccentColorDisplay (vbox, &accent);
  hbox = accent.hbox;
  starter->wcont [START_W_PROFILE_ACCENT] = accent.label;

  uiwidgetp = uiCreateLabel ("");
  uiWidgetSetClass (uiwidgetp, ERROR_CLASS);
  uiBoxPackEnd (hbox, uiwidgetp);
  starter->wcont [START_W_STATUS_MSG] = uiwidgetp;

  menubar = uiCreateMenubar ();
  uiBoxPackStart (hbox, menubar);
  uiwcontFree (hbox);

  /* CONTEXT: starterui: action menu for the starter user interface */
  menuitem = uiMenuCreateItem (menubar, _("Actions"), NULL);
  menu = uiCreateSubMenu (menuitem);
  uiwcontFree (menuitem);

  /* CONTEXT: starterui: menu item: stop all BDJ4 processes */
  snprintf (tbuff, sizeof (tbuff), _("Stop All %s Processes"), BDJ4_NAME);
  starter->callbacks [START_CB_MENU_STOP_ALL] = callbackInit (
      starterStopAllProcessCallback, starter, NULL);
  menuitem = uiMenuCreateItem (menu, tbuff,
      starter->callbacks [START_CB_MENU_STOP_ALL]);
  uiwcontFree (menuitem);

  starter->callbacks [START_CB_MENU_DEL_PROFILE] = callbackInit (
      starterDeleteProfile, starter, NULL);
  /* CONTEXT: starterui: menu item: delete profile */
  menuitem = uiMenuCreateItem (menu, _("Delete Profile"),
      starter->callbacks [START_CB_MENU_DEL_PROFILE]);
  uiwcontFree (menuitem);

  if (! isMacOS ()) {
    /* CONTEXT: starterui: menu item: create shortcut for profile */
    menuitem = uiMenuCreateItem (menu, _("Create Shortcut for Profile"),
        starter->callbacks [START_CB_MENU_PROFILE_SHORTCUT]);
    uiwcontFree (menuitem);
  }

  pathbldMakePath (tbuff, sizeof (tbuff),
      ALT_COUNT_FN, BDJ4_CONFIG_EXT, PATHBLD_MP_DREL_DATA);
  if (fileopFileExists (tbuff)) {
    /* CONTEXT: starterui: menu item: install in alternate folder */
    snprintf (tbuff, sizeof (tbuff), _("Set Up Alternate Folder"));
    starter->callbacks [START_CB_MENU_ALT_SETUP] = callbackInit (
        starterSetUpAlternate, starter, NULL);
    menuitem = uiMenuCreateItem (menu, tbuff,
        starter->callbacks [START_CB_MENU_ALT_SETUP]);
    uiwcontFree (menuitem);
  }

  /* main display */
  hbox = uiCreateHorizBox ();
  uiWidgetSetMarginTop (hbox, 4);
  uiBoxPackStart (vbox, hbox);

  /* CONTEXT: starterui: profile to be used when starting BDJ4 */
  uiwidgetp = uiCreateColonLabel (_("Profile"));
  uiBoxPackStart (hbox, uiwidgetp);
  uiwcontFree (uiwidgetp);

  /* get the profile list after bdjopt has been initialized */
  dispidx = starterGetProfiles (starter);
  uiSpinboxTextCreate (starter->profilesel, starter);
  uiSpinboxTextSet (starter->profilesel, 0,
      nlistGetCount (starter->proflist), starter->maxProfileWidth,
      starter->proflist, NULL, starterSetProfile);
  uiSpinboxTextSetValue (starter->profilesel, dispidx);
  uiwidgetp = uiSpinboxGetWidgetContainer (starter->profilesel);
  uiWidgetSetMarginStart (uiwidgetp, 4);
  uiWidgetAlignHorizFill (uiwidgetp);
  uiBoxPackStart (hbox, uiwidgetp);

  uiwcontFree (hbox);

  hbox = uiCreateHorizBox ();
  uiWidgetExpandHoriz (hbox);
  uiBoxPackStart (vbox, hbox);

  bvbox = uiCreateVertBox ();
  uiBoxPackStart (hbox, bvbox);

  pathbldMakePath (tbuff, sizeof (tbuff),
     "bdj4_icon", BDJ4_IMG_SVG_EXT, PATHBLD_MP_DIR_IMG);
  uiwidgetp = uiImageScaledFromFile (tbuff, 128);
  uiWidgetExpandHoriz (uiwidgetp);
  uiWidgetSetAllMargins (uiwidgetp, 10);
  uiBoxPackStart (hbox, uiwidgetp);
  uiwcontFree (uiwidgetp);

  uiwcontFree (hbox);

  starter->callbacks [START_CB_PLAYER] = callbackInit (
      starterStartPlayerui, starter, NULL);
  uibutton = uiCreateButton (
      starter->callbacks [START_CB_PLAYER],
      /* CONTEXT: starterui: button: starts the player user interface */
      _("Player"), NULL);
  starter->buttons [START_BUTTON_PLAYER] = uibutton;
  uiwidgetp = uiButtonGetWidgetContainer (uibutton);
  uiWidgetSetMarginTop (uiwidgetp, 2);
  uiWidgetAlignHorizStart (uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiBoxPackStart (bvbox, uiwidgetp);
  uiButtonAlignLeft (uibutton);

  starter->callbacks [START_CB_MANAGE] = callbackInit (
      starterStartManageui, starter, NULL);
  uibutton = uiCreateButton (
      starter->callbacks [START_CB_MANAGE],
      /* CONTEXT: starterui: button: starts the management user interface */
      _("Manage"), NULL);
  starter->buttons [START_BUTTON_MANAGE] = uibutton;
  uiwidgetp = uiButtonGetWidgetContainer (uibutton);
  uiWidgetSetMarginTop (uiwidgetp, 2);
  uiWidgetAlignHorizStart (uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiBoxPackStart (bvbox, uiwidgetp);
  uiButtonAlignLeft (uibutton);

  starter->callbacks [START_CB_CONFIG] = callbackInit (
      starterStartConfig, starter, NULL);
  uibutton = uiCreateButton (
      starter->callbacks [START_CB_CONFIG],
      /* CONTEXT: starterui: button: starts the configuration user interface */
      _("Configure"), NULL);
  starter->buttons [START_BUTTON_CONFIG] = uibutton;
  uiwidgetp = uiButtonGetWidgetContainer (uibutton);
  uiWidgetSetMarginTop (uiwidgetp, 2);
  uiWidgetAlignHorizStart (uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiBoxPackStart (bvbox, uiwidgetp);
  uiButtonAlignLeft (uibutton);

  starter->callbacks [START_CB_SUPPORT] = callbackInit (
      starterProcessSupport, starter, NULL);
  uibutton = uiCreateButton (
      starter->callbacks [START_CB_SUPPORT],
      /* CONTEXT: starterui: button: support : support information */
      _("Support"), NULL);
  starter->buttons [START_BUTTON_SUPPORT] = uibutton;
  uiwidgetp = uiButtonGetWidgetContainer (uibutton);
  uiWidgetSetMarginTop (uiwidgetp, 2);
  uiWidgetAlignHorizStart (uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiBoxPackStart (bvbox, uiwidgetp);
  uiButtonAlignLeft (uibutton);

  uibutton = uiCreateButton (
      starter->callbacks [START_CB_EXIT],
      /* CONTEXT: starterui: button: exits BDJ4 (exits everything) */
      _("Exit"), NULL);
  starter->buttons [START_BUTTON_EXIT] = uibutton;
  uiwidgetp = uiButtonGetWidgetContainer (uibutton);
  uiWidgetSetMarginTop (uiwidgetp, 2);
  uiWidgetAlignHorizStart (uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiBoxPackStart (bvbox, uiwidgetp);
  uiButtonAlignLeft (uibutton);

  starterSetWindowPosition (starter);

  pathbldMakePath (imgbuff, sizeof (imgbuff),
      "bdj4_icon", BDJ4_IMG_PNG_EXT, PATHBLD_MP_DIR_IMG);
  osuiSetIcon (imgbuff);

  uiWidgetShowAll (starter->wcont [START_W_WINDOW]);

  uiwcontFree (vbox);
  uiwcontFree (bvbox);
  uiwcontFree (szgrp);
  uiwcontFree (menu);
  uiwcontFree (menubar);

  logProcEnd (LOG_PROC, "starterBuildUI", "");
}

int
starterMainLoop (void *tstarter)
{
  startui_t   *starter = tstarter;
  int         stop = false;
  /* support message handling */
  char        tbuff [MAXPATHLEN];

  uiUIProcessEvents ();

  if (gNewProfile) {
    return true;
  }

  if (! progstateIsRunning (starter->progstate)) {
    progstateProcess (starter->progstate);
    if (progstateCurrState (starter->progstate) == STATE_CLOSED) {
      gStopProgram = true;
      stop = true;
    }
    if (gKillReceived) {
      logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
      progstateShutdownProcess (starter->progstate);
      gKillReceived = false;
    }
    return stop;
  }

  /* will connect to any process from which handshakes have been received */
  connProcessUnconnected (starter->conn);

  /* try to connect if not connected */
  for (int route = 0; route < ROUTE_MAX; ++route) {
    if (starter->started [route]) {
      if (! connIsConnected (starter->conn, route)) {
        connConnect (starter->conn, route);
      }
    }
  }

  if (starter->started [ROUTE_PLAYERUI] &&
      mstimeCheck (&starter->pluiCheckTime) &&
      connIsConnected (starter->conn, ROUTE_PLAYERUI)) {
    /* check and see if the playerui is still connected */
    connSendMessage (starter->conn, ROUTE_PLAYERUI, MSG_NULL, NULL);
    if (! connIsConnected (starter->conn, ROUTE_PLAYERUI)) {
      starterCloseProcess (starter, ROUTE_PLAYERUI, CLOSE_CRASH);
    }
    mstimeset (&starter->pluiCheckTime, 500);
  }

  switch (starter->startState) {
    case START_STATE_NONE: {
      break;
    }
    case START_STATE_DELAY: {
      ++starter->delayCount;
      if (starter->delayCount > LOOP_DELAY) {
        starter->delayCount = 0;
        starter->startState = starter->delayState;
      }
      break;
    }
    case START_STATE_SUPPORT_INIT: {
      char        datestr [40];
      char        tmstr [40];

      tmutilDstamp (datestr, sizeof (datestr));
      tmutilShortTstamp (tmstr, sizeof (tmstr));
      snprintf (starter->ident, sizeof (starter->ident), "%s-%s-%s",
          sysvarsGetStr (SV_USER_MUNGE), datestr, tmstr);

      starterSupportInit (starter);
      /* CONTEXT: starterui: support: status message */
      snprintf (tbuff, sizeof (tbuff), _("Sending Support Message"));
      uiLabelSetText (starter->wcont [START_W_STATUS_DISP], tbuff);
      starter->delayCount = 0;
      starter->delayState = START_STATE_SUPPORT_SEND_MSG;
      starter->startState = START_STATE_DELAY;
      break;
    }
    case START_STATE_SUPPORT_SEND_MSG: {
      const char  *email;
      const char  *subj;
      char        *msg;
      FILE        *fh;

      email = uiEntryGetValue (starter->supportemail);
      subj = uiEntryGetValue (starter->supportsubject);
      msg = uiTextBoxGetValue (starter->supporttb);

      strlcpy (tbuff, "support.txt", sizeof (tbuff));
      fh = fileopOpen (tbuff, "w");
      if (fh != NULL) {
        fprintf (fh, "Ident  : %s\n", starter->ident);
        fprintf (fh, "E-Mail : %s\n", email);
        fprintf (fh, "Subject: %s\n", subj);
        fprintf (fh, "Message:\n\n%s\n", msg);
        mdextfclose (fh);
        fclose (fh);
      }
      mdfree (msg);  // allocated by gtk

      supportSendFile (starter->support, starter->ident, tbuff, SUPPORT_NO_COMPRESSION);
      fileopDelete (tbuff);

      /* CONTEXT: starterui: support: status message */
      snprintf (tbuff, sizeof (tbuff), _("Sending %s Information"), BDJ4_NAME);
      uiLabelSetText (starter->wcont [START_W_STATUS_DISP], tbuff);
      starter->delayCount = 0;
      starter->delayState = START_STATE_SUPPORT_SEND_INFO;
      starter->startState = START_STATE_DELAY;
      break;
    }
    case START_STATE_SUPPORT_SEND_INFO: {
      char        prog [MAXPATHLEN];
      char        arg [40];
      const char  *targv [4];
      int         targc = 0;

      pathbldMakePath (prog, sizeof (prog),
          "bdj4info", sysvarsGetStr (SV_OS_EXEC_EXT), PATHBLD_MP_DIR_EXEC);
      strlcpy (arg, "--bdj4", sizeof (arg));
      strlcpy (tbuff, "bdj4info.txt", sizeof (tbuff));
      targv [targc++] = prog;
      targv [targc++] = arg;
      targv [targc++] = NULL;
      osProcessStart (targv, OS_PROC_WAIT, NULL, tbuff);
      supportSendFile (starter->support, starter->ident, tbuff, SUPPORT_COMPRESSED);
      fileopDelete (tbuff);

      strlcpy (tbuff, "VERSION.txt", sizeof (tbuff));
      supportSendFile (starter->support, starter->ident, tbuff, SUPPORT_NO_COMPRESSION);

      starter->startState = START_STATE_SUPPORT_SEND_FILES_DATA;
      break;
    }
    case START_STATE_SUPPORT_SEND_FILES_DATA: {
      bool        sendfiles;

      sendfiles = uiToggleButtonIsActive (starter->wcont [START_W_SUPPORT_SEND_FILES]);
      if (! sendfiles) {
        starter->startState = START_STATE_SUPPORT_SEND_DIAG_INIT;
        break;
      }

      pathbldMakePath (tbuff, sizeof (tbuff),
          "", "", PATHBLD_MP_DREL_DATA);
      starterSendFilesInit (starter, tbuff, SF_CONF_ONLY);
      starter->startState = START_STATE_SUPPORT_SEND_FILE;
      starter->nextState = START_STATE_SUPPORT_SEND_FILES_PROFILE;
      break;
    }
    case START_STATE_SUPPORT_SEND_FILES_PROFILE: {
      pathbldMakePath (tbuff, sizeof (tbuff),
          "", "", PATHBLD_MP_DREL_DATA | PATHBLD_MP_USEIDX);
      starterSendFilesInit (starter, tbuff, SF_CONF_ONLY);
      starter->startState = START_STATE_SUPPORT_SEND_FILE;
      starter->nextState = START_STATE_SUPPORT_SEND_FILES_MACHINE;
      break;
    }
    case START_STATE_SUPPORT_SEND_FILES_MACHINE: {
      pathbldMakePath (tbuff, sizeof (tbuff),
          "", "", PATHBLD_MP_DREL_DATA | PATHBLD_MP_HOSTNAME);
      starterSendFilesInit (starter, tbuff, SF_CONF_ONLY);
      starter->startState = START_STATE_SUPPORT_SEND_FILE;
      starter->nextState = START_STATE_SUPPORT_SEND_FILES_MACH_PROF;
      break;
    }
    case START_STATE_SUPPORT_SEND_FILES_MACH_PROF: {
      pathbldMakePath (tbuff, sizeof (tbuff),
          "", "", PATHBLD_MP_DREL_DATA | PATHBLD_MP_HOSTNAME | PATHBLD_MP_USEIDX);
      /* will end up with the backup bdjconfig.txt file also, but that's ok */
      /* need all of the log files */
      starterSendFilesInit (starter, tbuff, SF_ALL);
      starter->startState = START_STATE_SUPPORT_SEND_FILE;
      starter->nextState = START_STATE_SUPPORT_SEND_DIAG_INIT;
      break;
    }
    case START_STATE_SUPPORT_SEND_DIAG_INIT: {
      snprintf (tbuff, sizeof (tbuff), "%s/Library/Logs/DiagnosticReports",
          sysvarsGetStr (SV_HOME));
      starterSendFilesInit (starter, tbuff, SF_MAC_DIAG);

      if (fileopFileExists ("core") ||
          slistGetCount (starter->supportFileList) > 0) {
        /* CONTEXT: starterui: support: status message */
        snprintf (tbuff, sizeof (tbuff), _("Sending Diagnostics"));
        uiLabelSetText (starter->wcont [START_W_STATUS_DISP], tbuff);
        starter->startState = START_STATE_SUPPORT_SEND_DIAG;
      } else {
        starter->startState = START_STATE_SUPPORT_SEND_DB_PRE;
      }
      break;
    }
    case START_STATE_SUPPORT_SEND_DIAG: {
      strlcpy (tbuff, "core", sizeof (tbuff));
      if (fileopFileExists (tbuff)) {
        supportSendFile (starter->support, starter->ident, tbuff, SUPPORT_COMPRESSED);
        fileopDelete (tbuff);
      }

      starter->startState = START_STATE_SUPPORT_SEND_FILE;
      starter->nextState = START_STATE_SUPPORT_SEND_DB_PRE;
      break;
    }
    case START_STATE_SUPPORT_SEND_DB_PRE: {
      bool        senddb;

      senddb = uiToggleButtonIsActive (starter->wcont [START_W_SUPPORT_SEND_DB]);
      if (! senddb) {
        starter->startState = START_STATE_SUPPORT_FINISH;
        break;
      }
      /* CONTEXT: starterui: support: status message */
      snprintf (tbuff, sizeof (tbuff), _("Sending %s"), "data/musicdb.dat");
      uiLabelSetText (starter->wcont [START_W_STATUS_DISP], tbuff);
      starterSendFilesInit (starter, tbuff, SF_CONF_ONLY);
      starter->delayCount = 0;
      starter->delayState = START_STATE_SUPPORT_SEND_DB;
      starter->startState = START_STATE_DELAY;
      break;
    }
    case START_STATE_SUPPORT_SEND_DB: {
      bool        senddb;

      senddb = uiToggleButtonIsActive (starter->wcont [START_W_SUPPORT_SEND_DB]);
      if (senddb) {
        strlcpy (tbuff, "data/musicdb.dat", sizeof (tbuff));
        supportSendFile (starter->support, starter->ident, tbuff, SUPPORT_COMPRESSED);
      }
      starter->startState = START_STATE_SUPPORT_FINISH;
      break;
    }
    case START_STATE_SUPPORT_FINISH: {
      webclientClose (starter->webclient);
      starter->webclient = NULL;
      uiDialogDestroy (starter->wcont [START_W_SUPPORT_MSG_DIALOG]);
      starterSupportMsgDialogClear (starter);
      starter->startState = START_STATE_NONE;
      starter->supportmsgactive = false;
      break;
    }
    case START_STATE_SUPPORT_SEND_FILE: {
      starterSendFiles (starter);
      starter->delayCount = 0;
      starter->delayState = starter->startState;
      starter->startState = START_STATE_DELAY;
      break;
    }
  }

  if (gKillReceived) {
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
    progstateShutdownProcess (starter->progstate);
    gKillReceived = false;
  }
  return stop;
}

static int
starterProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, void *udata)
{
  startui_t       *starter = udata;

  logProcBegin (LOG_PROC, "starterProcessMsg");

  logMsg (LOG_DBG, LOG_MSGS, "got: from:%d/%s route:%d/%s msg:%d/%s args:%s",
      routefrom, msgRouteDebugText (routefrom),
      route, msgRouteDebugText (route), msg, msgDebugText (msg), args);

  switch (route) {
    case ROUTE_NONE:
    case ROUTE_STARTERUI: {
      switch (msg) {
        case MSG_HANDSHAKE: {
          if (! connIsConnected (starter->conn, routefrom)) {
            connConnect (starter->conn, routefrom);
          }
          connProcessHandshake (starter->conn, routefrom);
          break;
        }
        case MSG_SOCKET_CLOSE: {
          starterCloseProcess (starter, routefrom, CLOSE_REQUEST);
          break;
        }
        case MSG_EXIT_REQUEST: {
          logMsg (LOG_SESS, LOG_IMPORTANT, "got exit request");
          progstateShutdownProcess (starter->progstate);
          gKillReceived = false;
          break;
        }
        case MSG_REQ_PLAYERUI_ACTIVE: {
          if (routefrom == ROUTE_MANAGEUI) {
            starterSendPlayerActive (starter);
          }
          break;
        }
        case MSG_START_MAIN: {
          starterStartMain (starter, routefrom, args);
          break;
        }
        case MSG_STOP_MAIN: {
          starterStopMain (starter, routefrom);
          break;
        }
        case MSG_DB_ENTRY_UPDATE: {
          connSendMessage (starter->conn, ROUTE_MAIN, msg, args);
          if (routefrom != ROUTE_PLAYERUI) {
            connSendMessage (starter->conn, ROUTE_PLAYERUI, msg, args);
          }
          if (routefrom != ROUTE_MANAGEUI) {
            connSendMessage (starter->conn, ROUTE_MANAGEUI, msg, args);
          }
          break;
        }
        case MSG_DATABASE_UPDATE: {
          /* only comes from manage ui */
          connSendMessage (starter->conn, ROUTE_MAIN, msg, args);
          connSendMessage (starter->conn, ROUTE_PLAYERUI, msg, args);
          break;
        }
        case MSG_DEBUG_LEVEL: {
          starter->loglevel = atol (args);
          bdjoptSetNum (OPT_G_DEBUGLVL, starter->loglevel);
          logSetLevelAll (starter->loglevel);
          break;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }

  logProcEnd (LOG_PROC, "starterProcessMsg", "");

  if (gKillReceived) {
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
  }
  return gKillReceived;
}

static void
starterCloseProcess (startui_t *starter, bdjmsgroute_t routefrom, int request)
{
  int   wasstarted;

  if (request == CLOSE_CRASH) {
    logMsg (LOG_ERR, LOG_IMPORTANT, "ERR: %d/%s crashed", routefrom,
        msgRouteDebugText (routefrom));
  }

  wasstarted = starter->started [routefrom];

  procutilCloseProcess (starter->processes [routefrom],
      starter->conn, routefrom);
  procutilFreeRoute (starter->processes, routefrom);
  starter->processes [routefrom] = NULL;
  connDisconnect (starter->conn, routefrom);
  starter->started [routefrom] = 0;
  if (routefrom == ROUTE_MAIN) {
    starter->started [ROUTE_MAIN] = 0;
    starter->mainstart [ROUTE_PLAYERUI] = 0;
    starter->mainstart [ROUTE_MANAGEUI] = 0;
  }
  if (routefrom == ROUTE_PLAYERUI) {
    starterSendPlayerActive (starter);
    starterPlayerShutdown ();
  }

  if (request == CLOSE_CRASH && wasstarted && routefrom == ROUTE_PLAYERUI) {
    time_t  curr;

    curr = mstime ();
    if (curr > starter->lastPluiStart + 60000) {
      starterStartPlayerui (starter);
    }
  }
}

static void
starterStartMain (startui_t *starter, bdjmsgroute_t routefrom, char *args)
{
  int         flags;
  const char  *targv [5];
  int         targc = 0;

  if (starter->started [ROUTE_MAIN] == 0) {

    flags = PROCUTIL_DETACH;
    if (atoi (args)) {
      targv [targc++] = "--nomarquee";
    }
    targv [targc++] = NULL;

    starter->processes [ROUTE_MAIN] = procutilStartProcess (
        ROUTE_MAIN, "bdj4main", flags, targv);
  }

  /* if this ui already requested a start, let the ui know */
  if ( starter->mainstart [routefrom] ) {
    connSendMessage (starter->conn, routefrom, MSG_MAIN_ALREADY, NULL);
  }
  /* prevent multiple starts from the same ui */
  if ( ! starter->mainstart [routefrom] ) {
    ++starter->started [ROUTE_MAIN];
  }
  ++starter->mainstart [routefrom];
}


static void
starterStopMain (startui_t *starter, bdjmsgroute_t routefrom)
{
  if (starter->started [ROUTE_MAIN]) {
    --starter->started [ROUTE_MAIN];
    if (starter->started [ROUTE_MAIN] <= 0) {
      procutilStopProcess (starter->processes [ROUTE_MAIN],
          starter->conn, ROUTE_MAIN, PROCUTIL_NORM_TERM);
      starter->started [ROUTE_MAIN] = 0;
    }
    starter->mainstart [routefrom] = 0;
  }
}


static bool
starterCloseCallback (void *udata)
{
  startui_t   *starter = udata;

  if (progstateCurrState (starter->progstate) <= STATE_RUNNING) {
    progstateShutdownProcess (starter->progstate);
    logMsg (LOG_DBG, LOG_MSGS, "got: close win request");
  }

  return UICB_STOP;
}

static void
starterSigHandler (int sig)
{
  gKillReceived = true;
}

static bool
starterStartPlayerui (void *udata)
{
  startui_t *starter = udata;
  bool      rc;

  rc = starterStartProcess (starter, "bdj4playerui", ROUTE_PLAYERUI);
  starter->lastPluiStart = mstime ();
  mstimeset (&starter->pluiCheckTime, 500);
  starterSendPlayerActive (starter);
  starterPlayerStartup ();
  return rc;
}

static bool
starterStartManageui (void *udata)
{
  startui_t      *starter = udata;

  return starterStartProcess (starter, "bdj4manageui", ROUTE_MANAGEUI);
}

static bool
starterStartConfig (void *udata)
{
  startui_t      *starter = udata;

  return starterStartProcess (starter, "bdj4configui", ROUTE_CONFIGUI);
}

static bool
starterStartProcess (startui_t *starter, const char *procname,
    bdjmsgroute_t route)
{
  const char  *targv [5];
  int         targc = 0;

  if (starterCheckProfile (starter) < 0) {
    return UICB_STOP;
  }

  if (starter->started [route]) {
    if (procutilExists (starter->processes [route]) == 0) {
      connSendMessage (starter->conn, route, MSG_WINDOW_FIND, NULL);
      return UICB_CONT;
    }
  }

  targv [targc++] = NULL;

  starter->processes [route] = procutilStartProcess (
      route, procname, PROCUTIL_DETACH, targv);
  starter->started [route] = true;
  starterSendPlayerActive (starter);
  return UICB_CONT;
}

static void
starterSupportInit (startui_t *starter)
{
  if (starter->support == NULL) {
    starter->support = supportAlloc ();
  }
}

static bool
starterProcessSupport (void *udata)
{
  startui_t     *starter = udata;
  uiwcont_t     *vbox = NULL;
  uiwcont_t     *hbox = NULL;
  uiwcont_t     *uiwidgetp = NULL;
  uiwcont_t     *uidialog = NULL;
  uiwcont_t     *szgrp = NULL;
  uibutton_t    *uibutton;
  char          tbuff [MAXPATHLEN];
  const char    *builddate;
  const char    *rlslvl;
  const char    *devmode;
  uiutilsaccent_t accent;

  if (starter->supportactive) {
    return UICB_STOP;
  }

  if (starterCheckProfile (starter) < 0) {
    return UICB_STOP;
  }

  starter->supportactive = true;

  uidialog = uiCreateDialog (starter->wcont [START_W_WINDOW],
      starter->callbacks [START_CB_SUPPORT_RESP],
      /* CONTEXT: starterui: title for the support dialog */
      _("Support"),
      /* CONTEXT: starterui: support dialog: closes the dialog */
      _("Close"),
      RESPONSE_CLOSE,
      NULL
      );

  szgrp = uiCreateSizeGroupHoriz ();

  vbox = uiCreateVertBox ();
  uiWidgetSetAllMargins (vbox, 2);
  uiDialogPackInDialog (uidialog, vbox);

  /* status message line */
  uiutilsAddAccentColorDisplay (vbox, &accent);
  hbox = accent.hbox;
  uiwcontFree (accent.label);

  uiwidgetp = uiCreateLabel ("");
  uiWidgetSetClass (uiwidgetp, ERROR_CLASS);
  uiBoxPackEnd (hbox, uiwidgetp);
  starter->wcont [START_W_STATUS_DISP_MSG] = uiwidgetp;

  uiwcontFree (hbox);

  /* begin line */
  hbox = uiCreateHorizBox ();
  uiBoxPackStart (vbox, hbox);

  /* CONTEXT: starterui: basic support dialog, version display */
  snprintf (tbuff, sizeof (tbuff), _("%s Version"), BDJ4_NAME);
  uiwidgetp = uiCreateColonLabel (tbuff);
  uiBoxPackStart (hbox, uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiwcontFree (uiwidgetp);

  builddate = sysvarsGetStr (SV_BDJ4_BUILDDATE);
  rlslvl = sysvarsGetStr (SV_BDJ4_RELEASELEVEL);
  if (strcmp (rlslvl, "production") == 0) {
    rlslvl="";
  }
  devmode = sysvarsGetStr (SV_BDJ4_DEVELOPMENT);
  snprintf (tbuff, sizeof (tbuff), "%s %s (%s) %s",
      sysvarsGetStr (SV_BDJ4_VERSION), rlslvl, builddate, devmode);

  uiwidgetp = uiCreateLabel (tbuff);
  uiBoxPackStart (hbox, uiwidgetp);
  uiwcontFree (uiwidgetp);

  uiwcontFree (hbox);

  /* begin line */
  hbox = uiCreateHorizBox ();
  uiBoxPackStart (vbox, hbox);

  /* CONTEXT: starterui: basic support dialog, latest version display */
  uiwidgetp = uiCreateColonLabel (_("Latest Version"));
  uiBoxPackStart (hbox, uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiwcontFree (uiwidgetp);

  uiwidgetp = uiCreateLabel ("");
  uiBoxPackStart (hbox, uiwidgetp);

  if (*starter->latestversion == '\0') {
    starterSupportInit (starter);
    supportGetLatestVersion (starter->support, starter->latestversion, sizeof (starter->latestversion));
    if (*starter->latestversion == '\0') {
      /* CONTEXT: starterui: no internet connection */
      uiLabelSetText (starter->wcont [START_W_STATUS_DISP_MSG], _("No internet connection"));
    }
  }
  uiLabelSetText (uiwidgetp, starter->latestversion);
  uiwcontFree (uiwidgetp);

  /* begin line */
  /* CONTEXT: starterui: basic support dialog, list of support options */
  uiwidgetp = uiCreateColonLabel (_("Support options"));
  uiBoxPackStart (vbox, uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiwcontFree (uiwidgetp);

  /* begin line */
  /* CONTEXT: starterui: basic support dialog: support option (bdj4 download) */
  snprintf (tbuff, sizeof (tbuff), _("%s Download"), BDJ4_NAME);
  uiwidgetp = uiCreateLink (tbuff,
      starter->linkinfo [START_LINK_CB_DOWNLOAD].uri);
  if (isMacOS ()) {
    uiLinkSetActivateCallback (uiwidgetp,
        starter->linkinfo [START_LINK_CB_DOWNLOAD].macoscb);
  }
  uiBoxPackStart (vbox, uiwidgetp);
  starter->linkinfo [START_LINK_CB_DOWNLOAD].uiwidgetp = uiwidgetp;

  /* begin line */
  /* CONTEXT: starterui: basic support dialog: support option (bdj4 wiki) */
  snprintf (tbuff, sizeof (tbuff), _("%s Wiki"), BDJ4_NAME);
  uiwidgetp = uiCreateLink (tbuff,
      starter->linkinfo [START_LINK_CB_WIKI].uri);
  if (isMacOS ()) {
    uiLinkSetActivateCallback (uiwidgetp,
        starter->linkinfo [START_LINK_CB_WIKI].macoscb);
  }
  uiBoxPackStart (vbox, uiwidgetp);
  starter->linkinfo [START_LINK_CB_WIKI].uiwidgetp = uiwidgetp;

  /* begin line */
  /* CONTEXT: starterui: basic support dialog: support option (bdj4 forums) */
  snprintf (tbuff, sizeof (tbuff), _("%s Forums"), BDJ4_NAME);
  uiwidgetp = uiCreateLink (tbuff,
      starter->linkinfo [START_LINK_CB_FORUM].uri);
  if (isMacOS ()) {
    uiLinkSetActivateCallback (uiwidgetp,
        starter->linkinfo [START_LINK_CB_FORUM].macoscb);
  }
  uiBoxPackStart (vbox, uiwidgetp);
  starter->linkinfo [START_LINK_CB_FORUM].uiwidgetp = uiwidgetp;

  /* begin line */
  /* CONTEXT: starterui: basic support dialog: support option (bdj4 support tickets) */
  snprintf (tbuff, sizeof (tbuff), _("%s Support Tickets"), BDJ4_NAME);
  uiwidgetp = uiCreateLink (tbuff,
      starter->linkinfo [START_LINK_CB_TICKETS].uri);
  if (isMacOS ()) {
    uiLinkSetActivateCallback (uiwidgetp,
        starter->linkinfo [START_LINK_CB_TICKETS].macoscb);
  }
  uiBoxPackStart (vbox, uiwidgetp);
  starter->linkinfo [START_LINK_CB_TICKETS].uiwidgetp = uiwidgetp;

  uiwcontFree (hbox);

  /* begin line */
  hbox = uiCreateHorizBox ();
  uiBoxPackStart (vbox, hbox);

  uibutton = uiCreateButton (
      starter->callbacks [START_CB_SEND_SUPPORT],
      /* CONTEXT: starterui: basic support dialog: button: support option */
      _("Send Support Message"), NULL);
  starter->buttons [START_BUTTON_SEND_SUPPORT] = uibutton;
  uiwidgetp = uiButtonGetWidgetContainer (uibutton);
  uiBoxPackStart (hbox, uiwidgetp);

  uiwcontFree (hbox);

  starter->wcont [START_W_SUPPORT_DIALOG] = uidialog;
  uiDialogShow (uidialog);

  uiwcontFree (vbox);
  uiwcontFree (szgrp);

  return UICB_CONT;
}

static void
starterSupportDialogClear (startui_t *starter)
{
  for (int i = 0; i < START_LINK_CB_MAX; ++i) {
    uiwcontFree (starter->linkinfo [i].uiwidgetp);
    starter->linkinfo [i].uiwidgetp = NULL;
  }

  uiwcontFree (starter->wcont [START_W_STATUS_DISP_MSG]);
  starter->wcont [START_W_STATUS_DISP_MSG] = NULL;

  uiButtonFree (starter->buttons [START_BUTTON_SEND_SUPPORT]);
  starter->buttons [START_BUTTON_SEND_SUPPORT] = NULL;

  uiwcontFree (starter->wcont [START_W_SUPPORT_DIALOG]);
  starter->wcont [START_W_SUPPORT_DIALOG] = NULL;

  starter->supportactive = false;
}


static bool
starterSupportResponseHandler (void *udata, long responseid)
{
  startui_t *starter = udata;

  uiDialogDestroy (starter->wcont [START_W_SUPPORT_MSG_DIALOG]);
  if (responseid == RESPONSE_CLOSE) {
    uiDialogDestroy (starter->wcont [START_W_SUPPORT_DIALOG]);
  }
  starterSupportMsgDialogClear (starter);
  starterSupportDialogClear (starter);
  return UICB_CONT;
}

static int
starterGetProfiles (startui_t *starter)
{
  int         count;
  size_t      max;
  size_t      len;
  char        *pname = NULL;
  int         availprof = -1;
  nlist_t     *proflist = NULL;
  nlist_t     *profidxlist = NULL;
  bool        profileinuse = false;
  int         dispidx = -1;

  starter->newprofile = -1;
  starter->currprofile = sysvarsGetNum (SVL_BDJIDX);

  /* want the 'new profile' selection to always be last */
  /* and its index may not be last */
  /* use two lists, one with the display ordering, and an index list */
  proflist = nlistAlloc ("profile-list", LIST_ORDERED, NULL);
  profidxlist = nlistAlloc ("profile-idx-list", LIST_ORDERED, NULL);
  max = 0;

  count = 0;
  for (int i = 0; i < BDJOPT_MAX_PROFILES; ++i) {
    sysvarsSetNum (SVL_BDJIDX, i);

    if (bdjoptProfileExists ()) {
      pid_t   pid;

      pid = lockExists (lockName (ROUTE_STARTERUI), PATHBLD_MP_USEIDX);
      if (pid > 0) {
        /* do not add this selection to the profile list */
        if (starter->currprofile == i) {
          profileinuse = true;
        }
        continue;
      }

      if (profileinuse) {
        starter->currprofile = i;
        profileinuse = false;
      }

      if (i == starter->currprofile) {
        dispidx = count;
      }

      pname = bdjoptGetProfileName ();
      if (pname != NULL) {
        len = strlen (pname);
        max = len > max ? len : max;
        nlistSetStr (proflist, count, pname);
        nlistSetNum (profidxlist, count, i);
        mdfree (pname);
      }
      ++count;
    } else if (availprof == -1) {
      if (i == starter->currprofile) {
        profileinuse = true;
      }
      availprof = i;
    }
  }

  /* CONTEXT: starterui: selection to create a new profile */
  nlistSetStr (proflist, count, _("Create Profile"));
  nlistSetNum (profidxlist, count, availprof);
  starter->newprofile = availprof;
  len = strlen (nlistGetStr (proflist, count));
  max = len > max ? len : max;
  starter->maxProfileWidth = (int) max;
  if (profileinuse) {
    dispidx = count;
    starter->currprofile = availprof;
  }

  nlistFree (starter->proflist);
  starter->proflist = proflist;
  nlistFree (starter->profidxlist);
  starter->profidxlist = profidxlist;

  sysvarsSetNum (SVL_BDJIDX, starter->currprofile);

  if (starter->currprofile != starter->newprofile) {
    starterResetProfile (starter, starter->currprofile);
  }

  bdjvarsAdjustPorts ();

  if (dispidx == 0) {
    logMsg (LOG_DBG, LOG_IMPORTANT, "clean old locks");
    starterRemoveAllLocks ();
  }

  return dispidx;
}

static void
starterResetProfile (startui_t *starter, int profidx)
{
  starter->currprofile = profidx;
  sysvarsSetNum (SVL_BDJIDX, profidx);

  /* if the profile is the new profile, there's no option data to load */
  /* the check-profile function will do the actual creation of a new profile */
  /* if a button is pressed */
  if (profidx != starter->newprofile) {
    bdjoptInit ();
    uiWindowSetTitle (starter->wcont [START_W_WINDOW], bdjoptGetStr (OPT_P_PROFILENAME));
    uiutilsSetAccentColor (starter->wcont [START_W_PROFILE_ACCENT]);
    starterLoadOptions (starter);
    bdjvarsAdjustPorts ();
  }
}


static const char *
starterSetProfile (void *udata, int idx)
{
  startui_t   *starter = udata;
  const char  *disp;
  int         dispidx;
  int         profidx;
  int         chg;

  dispidx = uiSpinboxTextGetValue (starter->profilesel);
  disp = nlistGetStr (starter->proflist, dispidx);
  profidx = nlistGetNum (starter->profidxlist, dispidx);

  chg = profidx != starter->currprofile;

  if (chg) {
    uiLabelSetText (starter->wcont [START_W_STATUS_MSG], "");
    starterResetProfile (starter, profidx);
    gNewProfile = true;
  }

  return disp;
}

static int
starterCheckProfile (startui_t *starter)
{
  int   rc;

  uiLabelSetText (starter->wcont [START_W_STATUS_MSG], "");

  if (sysvarsGetNum (SVL_BDJIDX) == starter->newprofile) {
    char  tbuff [100];
    int   profidx;

    bdjoptInit ();
    profidx = sysvarsGetNum (SVL_BDJIDX);

    /* CONTEXT: starterui: name of the new profile (New profile 9) */
    snprintf (tbuff, sizeof (tbuff), _("New Profile %d"), profidx);
    bdjoptSetStr (OPT_P_PROFILENAME, tbuff);
    uiWindowSetTitle (starter->wcont [START_W_WINDOW], tbuff);

    /* select a completely random color */
    createRandomColor (tbuff, sizeof (tbuff));
    bdjoptSetStr (OPT_P_UI_PROFILE_COL, tbuff);
    uiutilsSetAccentColor (starter->wcont [START_W_PROFILE_ACCENT]);

    bdjoptSave ();

    templateImageCopy (bdjoptGetStr (OPT_P_UI_ACCENT_COL));
    templateDisplaySettingsCopy ();
    /* do not re-run the new profile init */
    starter->newprofile = -1;

    starterRebuildProfileList (starter);
  }

  rc = lockAcquire (lockName (ROUTE_STARTERUI), PATHBLD_MP_USEIDX);
  if (rc < 0) {
    /* CONTEXT: starterui: profile is already in use */
    uiLabelSetText (starter->wcont [START_W_STATUS_MSG], _("Profile in use"));
  } else {
    uiSpinboxSetState (starter->profilesel, UIWIDGET_DISABLE);
  }

  return rc;
}

static bool
starterDeleteProfile (void *udata)
{
  startui_t *starter = udata;
  char      tbuff [MAXPATHLEN];

  if (starter->currprofile == 0 ||
      starter->currprofile == starter->newprofile) {
    /* CONTEXT: starter: status message */
    uiLabelSetText (starter->wcont [START_W_STATUS_MSG], _("Profile may not be deleted."));
    return UICB_STOP;
  }

  logEnd ();

  /* data/profileNN */
  pathbldMakePath (tbuff, sizeof (tbuff), "", "",
      PATHBLD_MP_DREL_DATA | PATHBLD_MP_USEIDX);
  if (fileopIsDirectory (tbuff)) {
    diropDeleteDir (tbuff);
  }
  /* data/<hostname>/profileNN */
  pathbldMakePath (tbuff, sizeof (tbuff), "", "",
      PATHBLD_MP_DREL_DATA | PATHBLD_MP_HOSTNAME | PATHBLD_MP_USEIDX);
  if (fileopIsDirectory (tbuff)) {
    diropDeleteDir (tbuff);
  }
  /* img/profileNN */
  pathbldMakePath (tbuff, sizeof (tbuff), "", "",
      PATHBLD_MP_DIR_IMG | PATHBLD_MP_USEIDX);
  if (fileopIsDirectory (tbuff)) {
    diropDeleteDir (tbuff);
  }

  starterResetProfile (starter, 0);
  starterRebuildProfileList (starter);

  return UICB_CONT;
}

static void
starterRebuildProfileList (startui_t *starter)
{
  int       dispidx;

  dispidx = starterGetProfiles (starter);
  uiSpinboxTextSet (starter->profilesel, 0,
      nlistGetCount (starter->proflist), starter->maxProfileWidth,
      starter->proflist, NULL, starterSetProfile);
  uiSpinboxTextSetValue (starter->profilesel, dispidx);
}

static bool
starterCreateProfileShortcut (void *udata)
{
  startui_t   *starter = udata;
  char        *pname;

  pname = bdjoptGetProfileName ();
  instutilCreateShortcut (pname, sysvarsGetStr (SV_BDJ4_DIR_MAIN),
      sysvarsGetStr (SV_BDJ4_DIR_MAIN), starter->currprofile);
  mdfree (pname);
  return UICB_CONT;
}

static bool
starterCreateSupportMsgDialog (void *udata)
{
  startui_t     *starter = udata;
  uiwcont_t     *uiwidgetp;
  uiwcont_t     *vbox;
  uiwcont_t     *hbox;
  uiwcont_t     *uidialog;
  uiwcont_t     *szgrp;
  uitextbox_t   *tb;
  uiutilsaccent_t accent;

  if (starter->supportmsgactive) {
    return UICB_STOP;
  }

  starter->supportmsgactive = true;

  uidialog = uiCreateDialog (starter->wcont [START_W_WINDOW],
      starter->callbacks [START_CB_SUPPORT_MSG_RESP],
      /* CONTEXT: starterui: title for the support message dialog */
      _("Support Message"),
      /* CONTEXT: starterui: support message dialog: closes the dialog */
      _("Close"),
      RESPONSE_CLOSE,
      /* CONTEXT: starterui: support message dialog: sends the support message */
      _("Send Support Message"),
      RESPONSE_APPLY,
      NULL
      );
  uiWindowSetDefaultSize (uidialog, -1, 400);

  szgrp = uiCreateSizeGroupHoriz ();

  vbox = uiCreateVertBox ();
  uiWidgetSetAllMargins (vbox, 2);
  uiDialogPackInDialog (uidialog, vbox);

  /* profile color line */
  uiutilsAddAccentColorDisplay (vbox, &accent);
  hbox = accent.hbox;
  uiwcontFree (accent.label);

  /* line 1 */
  uiwcontFree (hbox);
  hbox = uiCreateHorizBox ();
  uiBoxPackStart (vbox, hbox);

  /* CONTEXT: starterui: sending support message: user's e-mail address */
  uiwidgetp = uiCreateColonLabel (_("E-Mail Address"));
  uiBoxPackStart (hbox, uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiwcontFree (uiwidgetp);

  starter->supportemail = uiEntryInit (50, 100);
  uiEntryCreate (starter->supportemail);
  uiBoxPackStart (hbox, uiEntryGetWidgetContainer (starter->supportemail));

  /* line 2 */
  uiwcontFree (hbox);
  hbox = uiCreateHorizBox ();
  uiBoxPackStart (vbox, hbox);

  /* CONTEXT: starterui: sending support message: subject of message */
  uiwidgetp = uiCreateColonLabel (_("Subject"));
  uiBoxPackStart (hbox, uiwidgetp);
  uiSizeGroupAdd (szgrp, uiwidgetp);
  uiwcontFree (uiwidgetp);

  starter->supportsubject = uiEntryInit (50, 100);
  uiEntryCreate (starter->supportsubject);
  uiBoxPackStart (hbox, uiEntryGetWidgetContainer (starter->supportsubject));

  /* line 3 */
  /* CONTEXT: starterui: sending support message: message text */
  uiwidgetp = uiCreateColonLabel (_("Message"));
  uiBoxPackStart (vbox, uiwidgetp);
  uiwcontFree (uiwidgetp);

  /* line 4 */
  tb = uiTextBoxCreate (200, NULL);
  uiTextBoxHorizExpand (tb);
  uiTextBoxVertExpand (tb);
  uiBoxPackStartExpand (vbox, uiTextBoxGetScrolledWindow (tb));
  starter->supporttb = tb;

  /* line 5 */
  /* CONTEXT: starterui: sending support message: checkbox: option to send data files */
  starter->wcont [START_W_SUPPORT_SEND_FILES] = uiCreateCheckButton (_("Attach Data Files"), 0);
  uiBoxPackStart (vbox, starter->wcont [START_W_SUPPORT_SEND_FILES]);

  /* line 6 */
  /* CONTEXT: starterui: sending support message: checkbox: option to send database */
  starter->wcont [START_W_SUPPORT_SEND_DB] = uiCreateCheckButton (_("Attach Database"), 0);
  uiBoxPackStart (vbox, starter->wcont [START_W_SUPPORT_SEND_DB]);

  /* line 7 */
  uiwidgetp = uiCreateLabel ("");
  uiBoxPackStart (vbox, uiwidgetp);
  uiLabelEllipsizeOn (uiwidgetp);
  uiWidgetSetClass (uiwidgetp, ACCENT_CLASS);
  starter->wcont [START_W_STATUS_DISP] = uiwidgetp;

  starter->wcont [START_W_SUPPORT_MSG_DIALOG] = uidialog;
  uiDialogShow (uidialog);

  uiwcontFree (vbox);
  uiwcontFree (hbox);
  uiwcontFree (szgrp);

  return UICB_CONT;
}

static void
starterSupportMsgDialogClear (startui_t *starter)
{
  uiwcontFree (starter->wcont [START_W_SUPPORT_SEND_FILES]);
  starter->wcont [START_W_SUPPORT_SEND_FILES] = NULL;

  uiwcontFree (starter->wcont [START_W_SUPPORT_SEND_DB]);
  starter->wcont [START_W_SUPPORT_SEND_DB] = NULL;

  uiwcontFree (starter->wcont [START_W_SUPPORT_MSG_DIALOG]);
  starter->wcont [START_W_SUPPORT_MSG_DIALOG] = NULL;

  uiwcontFree (starter->wcont [START_W_STATUS_DISP]);
  starter->wcont [START_W_STATUS_DISP] = NULL;

  uiTextBoxFree (starter->supporttb);
  starter->supporttb = NULL;

  uiEntryFree (starter->supportsubject);
  starter->supportsubject = NULL;

  uiEntryFree (starter->supportemail);
  starter->supportemail = NULL;

  starter->supportmsgactive = false;
}


static bool
starterSupportMsgHandler (void *udata, long responseid)
{
  startui_t   *starter = udata;

  switch (responseid) {
    case RESPONSE_DELETE_WIN: {
      starterSupportMsgDialogClear (starter);
      break;
    }
    case RESPONSE_CLOSE: {
      uiDialogDestroy (starter->wcont [START_W_SUPPORT_MSG_DIALOG]);
      starterSupportMsgDialogClear (starter);
      break;
    }
    case RESPONSE_APPLY: {
      starter->startState = START_STATE_SUPPORT_INIT;
      break;
    }
  }
  return UICB_CONT;
}

static void
starterSendFilesInit (startui_t *starter, char *dir, int sendType)
{
  slist_t     *list;
  char        *ext = BDJ4_CONFIG_EXT;

  if (sendType == SF_ALL) {
    ext = NULL;
  }
  if (sendType == SF_MAC_DIAG) {
    ext = ".crash";
  }
  starter->sendType = sendType;
  list = dirlistBasicDirList (dir, ext);
  starter->supportFileList = list;
  slistStartIterator (list, &starter->supportFileIterIdx);
  dataFree (starter->supportDir);
  starter->supportDir = mdstrdup (dir);
}

static void
starterSendFiles (startui_t *starter)
{
  const char  *fn;
  const char  *origfn;
  char        ifn [MAXPATHLEN];
  char        tbuff [100];

  if (starter->supportInFname != NULL) {
    origfn = starter->supportInFname;
    supportSendFile (starter->support, starter->ident, origfn, SUPPORT_COMPRESSED);
    if (starter->sendType == SF_MAC_DIAG) {
      fileopDelete (starter->supportInFname);
    }
    dataFree (starter->supportInFname);
    starter->supportInFname = NULL;
    return;
  }

  if ((fn = slistIterateKey (starter->supportFileList,
      &starter->supportFileIterIdx)) == NULL) {
    slistFree (starter->supportFileList);
    starter->supportFileList = NULL;
    dataFree (starter->supportDir);
    starter->supportDir = NULL;
    starter->startState = starter->nextState;
    return;
  }

  if (starter->sendType == SF_MAC_DIAG) {
    if (strncmp (fn, "bdj4", 4) != 0) {
      /* skip any .crash file that is not a bdj4 crash */
      return;
    }
  }

  strlcpy (ifn, starter->supportDir, sizeof (ifn));
  strlcat (ifn, "/", sizeof (ifn));
  strlcat (ifn, fn, sizeof (ifn));
  starter->supportInFname = mdstrdup (ifn);
  /* CONTEXT: starterui: support: status message */
  snprintf (tbuff, sizeof (tbuff), _("Sending %s"), ifn);
  uiLabelSetText (starter->wcont [START_W_STATUS_DISP], tbuff);
}

static bool
starterStopAllProcessCallback (void *udata)
{
  startui_t   *starter = udata;

  starterStopAllProcesses (starter->conn);
  return UICB_CONT;
}

static inline bool
starterDownloadLinkHandler (void *udata)
{
  starterLinkHandler (udata, START_LINK_CB_DOWNLOAD);
  return UICB_STOP;
}

static inline bool
starterWikiLinkHandler (void *udata)
{
  starterLinkHandler (udata, START_LINK_CB_WIKI);
  return UICB_STOP;
}

static inline bool
starterForumLinkHandler (void *udata)
{
  starterLinkHandler (udata, START_LINK_CB_FORUM);
  return UICB_STOP;
}

static inline bool
starterTicketLinkHandler (void *udata)
{
  starterLinkHandler (udata, START_LINK_CB_TICKETS);
  return UICB_STOP;
}

static void
starterLinkHandler (void *udata, int cbidx)
{
  startui_t *starter = udata;
  char        *uri;
  char        tmp [200];

  uri = starter->linkinfo [cbidx].uri;
  if (uri != NULL) {
    snprintf (tmp, sizeof (tmp), "open %s", uri);
    (void) ! system (tmp);
  }
}

static void
starterSaveWindowPosition (startui_t *starter)
{
  int     x, y, ws;

  uiWindowGetSize (starter->wcont [START_W_WINDOW], &x, &y);
  nlistSetNum (starter->options, STARTERUI_SIZE_X, x);
  nlistSetNum (starter->options, STARTERUI_SIZE_Y, y);
  uiWindowGetPosition (starter->wcont [START_W_WINDOW], &x, &y, &ws);
  nlistSetNum (starter->options, STARTERUI_POSITION_X, x);
  nlistSetNum (starter->options, STARTERUI_POSITION_Y, y);
}

static void
starterSetWindowPosition (startui_t *starter)
{
  int   x, y;

  x = nlistGetNum (starter->options, STARTERUI_POSITION_X);
  y = nlistGetNum (starter->options, STARTERUI_POSITION_Y);
  uiWindowMove (starter->wcont [START_W_WINDOW], x, y, -1);
}

static void
starterLoadOptions (startui_t *starter)
{
  char  tbuff [MAXPATHLEN];

  if (starter->optionsalloc) {
    nlistFree (starter->options);
  }
  datafileFree (starter->optiondf);
  starter->optiondf = NULL;
  starter->options = NULL;
  starter->optionsalloc = false;

  pathbldMakePath (tbuff, sizeof (tbuff),
      STARTERUI_OPT_FN, BDJ4_CONFIG_EXT, PATHBLD_MP_DREL_DATA);
  starter->optiondf = datafileAllocParse ("ui-starter", DFTYPE_KEY_VAL, tbuff,
      starteruidfkeys, STARTERUI_KEY_MAX, DF_NO_OFFSET, NULL);
  starter->options = datafileGetList (starter->optiondf);
  if (starter->options == NULL || nlistGetCount (starter->options) == 0) {
    starter->optionsalloc = true;
    starter->options = nlistAlloc ("ui-starter", LIST_ORDERED, NULL);

    nlistSetNum (starter->options, STARTERUI_POSITION_X, -1);
    nlistSetNum (starter->options, STARTERUI_POSITION_Y, -1);
    nlistSetNum (starter->options, STARTERUI_SIZE_X, 1200);
    nlistSetNum (starter->options, STARTERUI_SIZE_Y, 800);
  }
}

static bool
starterSetUpAlternate (void *udata)
{
  char        prog [MAXPATHLEN];
  const char  *targv [7];
  int         targc = 0;


  logProcBegin (LOG_PROC, "starterSetUpAlternate");

  pathbldMakePath (prog, sizeof (prog),
      "bdj4", sysvarsGetStr (SV_OS_EXEC_EXT), PATHBLD_MP_DIR_EXEC);
  targv [targc++] = prog;
  targv [targc++] = "--bdj4altsetup";
  targv [targc++] = NULL;
  osProcessStart (targv, OS_PROC_DETACH, NULL, NULL);

  logProcEnd (LOG_PROC, "starterSetUpAlternate", "");
  return UICB_CONT;
}

static void
starterSendPlayerActive (startui_t *starter)
{
  char  tmp [40];

  snprintf (tmp, sizeof (tmp), "%d", starter->started [ROUTE_PLAYERUI]);
  connSendMessage (starter->conn, ROUTE_MANAGEUI, MSG_PLAYERUI_ACTIVE, tmp);
}


