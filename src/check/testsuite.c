#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <locale.h>
#include <signal.h>

#include "bdj4.h"
#include "bdj4init.h"
#include "bdjmsg.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "bdjvars.h"
#include "conn.h"
#include "dirlist.h"
#include "filedata.h"
#include "fileop.h"
#include "istring.h"
#include "localeutil.h"
#include "log.h"
#include "nlist.h"
#include "osrandom.h"
#include "ossignal.h"
#include "pathbld.h"
#include "procutil.h"
#include "progstate.h"
#include "slist.h"
#include "sockh.h"
#include "sysvars.h"
#include "tmutil.h"

typedef struct {
  int       testcount;
  int       testfail;
  int       chkcount;
  int       chkfail;
  mstime_t  start;
} results_t;

typedef enum {
  TS_OK,
  TS_BAD_ROUTE,
  TS_BAD_MSG,
  TS_BAD_COMMAND,
  TS_UNKNOWN,
  TS_NOT_IMPLEMENTED,
  TS_CHECK_FAILED,
  TS_CHECK_TIMEOUT,
  TS_FINISHED,
} ts_return_t;

enum {
  TS_TYPE_MSG,
  TS_TYPE_GET,
};

enum {
  TS_CHK_TIMEOUT = 200,
};

typedef struct {
  progstate_t *progstate;
  procutil_t  *processes [ROUTE_MAX];
  conn_t      *conn;
  int         stopwaitcount;
  slist_t     *routetxtlist;
  slist_t     *msgtxtlist;
  long        responseTimeout;
  mstime_t    responseTimeoutCheck;
  slist_t     *chkresponse;
  slist_t     *chkexpect;
  char        *lastResponse;
  bdjmsgroute_t waitRoute;
  bdjmsgmsg_t waitMessage;
  mstime_t    waitCheck;
  results_t   results;
  results_t   sresults;
  results_t   gresults;
  slist_t     *failedlist;
  char        sectionnum [10];
  char        sectionname [80];
  char        testnum [10];
  char        testname [80];
  slist_t     *testlist;
  slistidx_t  tliteridx;
  FILE        *fh;
  int         lineno;
  long        defaultVol;
  bool        greaterthan : 1;
  bool        lessthan : 1;
  bool        checkor : 1;
  bool        checknot : 1;
  bool        expectresponse : 1;
  bool        haveresponse : 1;
  bool        runsection : 1;         // --runsection was specified
  bool        runtest : 1;            // --runtest was specified
  bool        processsection : 1;     // process the current section
  bool        processtest : 1;        // process the current test
  bool        wait : 1;
  bool        chkwait : 1;
  bool        skiptoend : 1;          // used for test timeout
  bool        verbose : 1;
} testsuite_t;

static int  gKillReceived = 0;

static int  tsProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
                bdjmsgmsg_t msg, char *args, void *udata);
static int  tsProcessing (void *udata);
static bool tsListeningCallback (void *tts, programstate_t programState);
static bool tsConnectingCallback (void *tts, programstate_t programState);
static bool tsHandshakeCallback (void *tts, programstate_t programState);
static bool tsStoppingCallback (void *tts, programstate_t programState);
static bool tsStopWaitCallback (void *tts, programstate_t programState);
static bool tsClosingCallback (void *tts, programstate_t programState);
static void clearResults (results_t *results);
static void startResultTimer (results_t *results);
static void tallyResults (testsuite_t *testsuite);
static void printResults (testsuite_t *testsuite, results_t *results);
static int  tsProcessScript (testsuite_t *testsuite);
static int  tsScriptSection (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptTest (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptMsg (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptGet (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptChk (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptWait (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptSleep (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptDisp (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptPrint (testsuite_t *testsuite, const char *tcmd);
static int  tsParseExpect (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptChkResponse (testsuite_t *testsuite);
static int  tsSendMessage (testsuite_t *testsuite, const char *tcmd, int type);
static void tsProcessChkResponse (testsuite_t *testsuite, char *args);
static void tsDisplayCommandResult (testsuite_t *testsuite, ts_return_t ok);
static void resetChkResponse (testsuite_t *testsuite);
static void resetPlayer (testsuite_t *testsuite);
static bool tsNextFile (testsuite_t *testsuite);
static void tsSigHandler (int sig);

int
main (int argc, char *argv [])
{
  testsuite_t testsuite;
  int         flags;
  int         listenPort;
  char        *state;
  int         rc;
  char        tbuff [MAXPATHLEN];

  osSetStandardSignals (tsSigHandler);
#if _define_SIGCHLD
  osDefaultSignal (SIGCHLD);
#endif
  osCatchSignal (tsSigHandler, SIGINT);
  osCatchSignal (tsSigHandler, SIGTERM);

  flags = BDJ4_INIT_NO_DB_LOAD | BDJ4_INIT_NO_DATAFILE_LOAD;
  flags = bdj4startup (argc, argv, NULL, "ts", ROUTE_TEST_SUITE, flags);
  logEnd ();

  logStart ("testsuite", "ts",
      LOG_IMPORTANT | LOG_BASIC | LOG_MAIN | LOG_MSGS | LOG_ACTIONS);

  pathbldMakePath (tbuff, sizeof (tbuff),
      VOLREG_FN, BDJ4_CONFIG_EXT, PATHBLD_MP_DATA);
  fileopDelete (tbuff);

  procutilInitProcesses (testsuite.processes);
  testsuite.conn = connInit (ROUTE_TEST_SUITE);
  clearResults (&testsuite.results);
  clearResults (&testsuite.sresults);
  clearResults (&testsuite.gresults);
  startResultTimer (&testsuite.gresults);
  testsuite.failedlist = slistAlloc ("ts-failed", LIST_ORDERED, NULL);
  testsuite.progstate = progstateInit ("testsuite");
  testsuite.stopwaitcount = 0;
  testsuite.skiptoend = false;
  testsuite.chkexpect = NULL;
  mstimeset (&testsuite.waitCheck, 100);
  mstimeset (&testsuite.responseTimeoutCheck, TS_CHK_TIMEOUT);
  strlcpy (testsuite.sectionnum, "1", sizeof (testsuite.sectionnum));
  strlcpy (testsuite.sectionname, "Init", sizeof (testsuite.sectionname));
  strlcpy (testsuite.testnum, "1", sizeof (testsuite.testnum));
  strlcpy (testsuite.testname, "Init", sizeof (testsuite.testname));
  testsuite.fh = NULL;
  testsuite.runsection = false;
  testsuite.runtest = false;
  testsuite.processsection = false;
  testsuite.processtest = false;
  testsuite.verbose = false;
  testsuite.chkresponse = NULL;
  testsuite.expectresponse = false;
  testsuite.wait = false;
  testsuite.lastResponse = NULL;
  /* chkresponse, haveresponse, lessthan, greaterthan, */
  /* checkor, chkwait, checknot */
  resetChkResponse (&testsuite);
  testsuite.defaultVol = bdjoptGetNum (OPT_P_DEFAULTVOLUME);

  if ((flags & BDJ4_TS_RUNSECTION) == BDJ4_TS_RUNSECTION) {
    testsuite.runsection = true;
  }

  if ((flags & BDJ4_TS_RUNTEST) == BDJ4_TS_RUNTEST) {
    testsuite.runtest = true;
  }

  if ((flags & BDJ4_TS_VERBOSE) == BDJ4_TS_VERBOSE) {
    testsuite.verbose = true;
  }

  testsuite.routetxtlist = slistAlloc ("ts-route-txt", LIST_UNORDERED, NULL);
  slistSetSize (testsuite.routetxtlist, ROUTE_MAX);
  for (int i = 0; i < ROUTE_MAX; ++i) {
    slistSetNum (testsuite.routetxtlist, bdjmsgroutetxt [i], i);
  }
  slistSort (testsuite.routetxtlist);

  testsuite.msgtxtlist = slistAlloc ("ts-msg-txt", LIST_UNORDERED, NULL);
  slistSetSize (testsuite.msgtxtlist, MSG_MAX);
  for (int i = 0; i < MSG_MAX; ++i) {
    slistSetNum (testsuite.msgtxtlist, bdjmsgtxt [i], i);
  }
  slistSort (testsuite.msgtxtlist);

  testsuite.testlist = dirlistBasicDirList ("test-templates/tests", ".txt");
  slistSort (testsuite.testlist);
  slistStartIterator (testsuite.testlist, &testsuite.tliteridx);
  tsNextFile (&testsuite);
  testsuite.lineno = 0;

  progstateSetCallback (testsuite.progstate, STATE_LISTENING,
      tsListeningCallback, &testsuite);
  progstateSetCallback (testsuite.progstate, STATE_CONNECTING,
      tsConnectingCallback, &testsuite);
  progstateSetCallback (testsuite.progstate, STATE_WAIT_HANDSHAKE,
      tsHandshakeCallback, &testsuite);
  progstateSetCallback (testsuite.progstate, STATE_STOPPING,
      tsStoppingCallback, &testsuite);
  progstateSetCallback (testsuite.progstate, STATE_STOP_WAIT,
      tsStopWaitCallback, &testsuite);
  progstateSetCallback (testsuite.progstate, STATE_CLOSING,
      tsClosingCallback, &testsuite);

  listenPort = bdjvarsGetNum (BDJVL_TEST_SUITE_PORT);
  sockhMainLoop (listenPort, tsProcessMsg, tsProcessing, &testsuite);

  strlcpy (testsuite.testnum, "", sizeof (testsuite.testnum));
  strlcpy (testsuite.testname, "", sizeof (testsuite.testname));

  if (testsuite.processsection) {
    printResults (&testsuite, &testsuite.sresults);
  }

  strlcpy (testsuite.sectionnum, "", sizeof (testsuite.sectionnum));
  strlcpy (testsuite.sectionname, "Final", sizeof (testsuite.sectionname));
  state = "FAIL";
  rc = 1;
  if (testsuite.gresults.testfail == 0) {
    state = "OK";
    rc = 0;
  }
  printResults (&testsuite, &testsuite.gresults);
  fprintf (stdout, "%s %s tests: %d failed: %d\n",
      state, testsuite.sectionname,
      testsuite.gresults.testcount, testsuite.gresults.testfail);
  if (testsuite.gresults.testfail > 0) {
    slistidx_t  iteridx;
    char        *key;

    fprintf (stdout, "Failed: ");
    slistStartIterator (testsuite.failedlist, &iteridx);
    while ((key = slistIterateKey (testsuite.failedlist, &iteridx)) != NULL) {
      fprintf (stdout, "%s ", key);
    }
    fprintf (stdout, "\n");
  }
  fflush (stdout);

  slistFree (testsuite.failedlist);
  progstateFree (testsuite.progstate);
  logEnd ();

  if (fileopFileExists ("core")) {
    fprintf (stdout, "core dumped\n");
    fflush (stdout);
    rc = 1;
  }

  return rc;
}


static int
tsProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, void *udata)
{
  testsuite_t *testsuite = udata;


  logMsg (LOG_DBG, LOG_MSGS, "got: from:%d/%s route:%d/%s msg:%d/%s args:%s",
      routefrom, msgRouteDebugText (routefrom),
      route, msgRouteDebugText (route), msg, msgDebugText (msg), args);

  switch (route) {
    case ROUTE_NONE:
    case ROUTE_TEST_SUITE: {
      switch (msg) {
        case MSG_HANDSHAKE: {
          connProcessHandshake (testsuite->conn, routefrom);
          break;
        }
        case MSG_SOCKET_CLOSE: {
          connDisconnect (testsuite->conn, routefrom);
          break;
        }
        case MSG_EXIT_REQUEST: {
          logMsg (LOG_SESS, LOG_IMPORTANT, "got exit request");
          progstateShutdownProcess (testsuite->progstate);
          break;
        }
        case MSG_CHK_MAIN_MUSICQ:
        case MSG_CHK_PLAYER_STATUS:
        case MSG_CHK_PLAYER_SONG: {
          tsProcessChkResponse (testsuite, args);
          break;
        }
        default: {
          break;
        }
      }
    }
    default: {
      break;
    }
  }

  return 0;
}

static int
tsProcessing (void *udata)
{
  testsuite_t *testsuite = udata;
  bool        stop = false;

  if (! progstateIsRunning (testsuite->progstate)) {
    progstateProcess (testsuite->progstate);
    if (progstateCurrState (testsuite->progstate) == STATE_CLOSED) {
      stop = true;
    }
    if (gKillReceived) {
      progstateShutdownProcess (testsuite->progstate);
      logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
      gKillReceived = 0;
    }
    return stop;
  }

  connProcessUnconnected (testsuite->conn);

  if (testsuite->chkwait && testsuite->haveresponse) {
    ts_return_t ok;

    logMsg (LOG_DBG, LOG_BASIC, "process chk response");
    ok = tsScriptChkResponse (testsuite);
    testsuite->chkwait = false;
    testsuite->expectresponse = false;

    if (ok != TS_OK) {
      ++testsuite->results.chkfail;
    }
    tsDisplayCommandResult (testsuite, ok);
  }

  if (testsuite->wait && testsuite->haveresponse) {
    ts_return_t ok;

    logMsg (LOG_DBG, LOG_BASIC, "process wait response");
    ok = tsScriptChkResponse (testsuite);
    if (ok == TS_OK) {
      logMsg (LOG_DBG, LOG_BASIC, "wait response ok");
      testsuite->expectresponse = false;
      testsuite->wait = false;
    } else {
      testsuite->expectresponse = true;
      testsuite->haveresponse = false;
    }
  }

  if ((testsuite->chkwait || testsuite->wait) &&
      mstimeCheck (&testsuite->responseTimeoutCheck)) {
    logMsg (LOG_DBG, LOG_BASIC, "timed out %d", testsuite->lineno);
    fprintf (stdout, "\n");
    fprintf (stdout, "       %d %d/%d (%ld)\n", testsuite->lineno,
        testsuite->chkwait, testsuite->wait, testsuite->responseTimeout);
    fflush (stdout);
    ++testsuite->results.chkfail;
    testsuite->skiptoend = true;
    tsDisplayCommandResult (testsuite, TS_CHECK_TIMEOUT);
    resetChkResponse (testsuite);
    testsuite->wait = false;
    testsuite->expectresponse = false;
  }

  if ((testsuite->wait) &&
      mstimeCheck (&testsuite->waitCheck)) {
    resetChkResponse (testsuite);
    connSendMessage (testsuite->conn, testsuite->waitRoute,
        testsuite->waitMessage, NULL);
    mstimeset (&testsuite->waitCheck, 100);
  }

  if (testsuite->wait == false && testsuite->chkwait == false) {
    int   count = 1;

    while (count == 1 ||
        (! testsuite->processtest && ! testsuite->processsection)) {
      if (tsProcessScript (testsuite)) {
        progstateShutdownProcess (testsuite->progstate);
        logMsg (LOG_DBG, LOG_BASIC, "finished script");
        break;
      }
      --count;
    }
  }

  if (gKillReceived) {
    progstateShutdownProcess (testsuite->progstate);
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
    gKillReceived = 0;
  }

  return stop;
}

static bool
tsListeningCallback (void *tts, programstate_t programState)
{
  testsuite_t *testsuite = tts;
  const char  *targv [5];
  int         targc = 0;

  /* start main */
  targc = 0;
  targv [targc++] = "--nomarquee";
  targv [targc++] = NULL;
  testsuite->processes [ROUTE_MAIN] = procutilStartProcess (
        ROUTE_MAIN, "bdj4main", PROCUTIL_DETACH, targv);
  return STATE_FINISHED;
}

static bool
tsConnectingCallback (void *tts, programstate_t programState)
{
  testsuite_t *testsuite = tts;
  bool    rc = STATE_NOT_FINISH;

  connProcessUnconnected (testsuite->conn);

  if (! connIsConnected (testsuite->conn, ROUTE_MAIN)) {
    connConnect (testsuite->conn, ROUTE_MAIN);
  }
  if (! connIsConnected (testsuite->conn, ROUTE_PLAYER)) {
    connConnect (testsuite->conn, ROUTE_PLAYER);
  }

  if (connIsConnected (testsuite->conn, ROUTE_MAIN) &&
      connIsConnected (testsuite->conn, ROUTE_PLAYER)) {
    rc = STATE_FINISHED;
  }

  return rc;
}

static bool
tsHandshakeCallback (void *tts, programstate_t programState)
{
  testsuite_t *testsuite = tts;
  bool    rc = STATE_NOT_FINISH;

  connProcessUnconnected (testsuite->conn);

  if (connHaveHandshake (testsuite->conn, ROUTE_MAIN) &&
      connHaveHandshake (testsuite->conn, ROUTE_PLAYER)) {
    resetPlayer (testsuite);
    rc = STATE_FINISHED;
  }

  return rc;
}

static bool
tsStoppingCallback (void *tts, programstate_t programState)
{
  testsuite_t *testsuite = tts;

  procutilStopAllProcess (testsuite->processes, testsuite->conn, false);

  connDisconnect (testsuite->conn, ROUTE_MAIN);
  connDisconnect (testsuite->conn, ROUTE_PLAYER);

  return STATE_FINISHED;
}

static bool
tsStopWaitCallback (void *tts, programstate_t programState)
{
  testsuite_t *testsuite = tts;
  bool    rc = STATE_NOT_FINISH;

  rc = connWaitClosed (testsuite->conn, &testsuite->stopwaitcount);
  return rc;
}

static bool
tsClosingCallback (void *tts, programstate_t programState)
{
  testsuite_t *testsuite = tts;

  if (testsuite->fh != NULL) {
    fclose (testsuite->fh);
    testsuite->fh = NULL;
  }
  bdj4shutdown (ROUTE_TEST_SUITE, NULL);
  procutilStopAllProcess (testsuite->processes, testsuite->conn, true);
  procutilFreeAll (testsuite->processes);
  connFree (testsuite->conn);
  slistFree (testsuite->routetxtlist);
  slistFree (testsuite->msgtxtlist);
  if (testsuite->chkresponse != NULL) {
    slistFree (testsuite->chkresponse);
  }
  if (testsuite->chkexpect != NULL) {
    slistFree (testsuite->chkexpect);
  }
  if (testsuite->lastResponse != NULL) {
    free (testsuite->lastResponse);
  }

  return STATE_FINISHED;
}


static int
tsProcessScript (testsuite_t *testsuite)
{
  ts_return_t ok;
  bool        disp = false;
  char        tcmd [200];
  bool        processtest;

  if (fgets (tcmd, sizeof (tcmd), testsuite->fh) == NULL) {
    if (tsNextFile (testsuite)) {
      return true;
    }
    return false;
  }

  ++testsuite->lineno;

  if (*tcmd == '#') {
    return false;
  }
  if (*tcmd == '\n') {
    return false;
  }
  stringTrim (tcmd);

  ok = TS_UNKNOWN;

  if (strncmp (tcmd, "section", 7) == 0) {
    ok = tsScriptSection (testsuite, tcmd);
  }

  if (ok == TS_FINISHED) {
    return true;
  }

  if (testsuite->processtest) {
    logMsg (LOG_DBG, LOG_BASIC, "-- cmd: %3d %s", testsuite->lineno, tcmd);
  }

  if (strncmp (tcmd, "test", 4) == 0) {
    ok = tsScriptTest (testsuite, tcmd);
  }

  if (testsuite->processtest) {
    if (strncmp (tcmd, "end", 3) == 0) {
      ++testsuite->results.testcount;
      printResults (testsuite, &testsuite->results);
      tallyResults (testsuite);
      clearResults (&testsuite->results);
      resetPlayer (testsuite);
      resetChkResponse (testsuite);
      testsuite->expectresponse = false;
      testsuite->wait = false;
      testsuite->chkwait = false;
      ok = TS_OK;
      testsuite->skiptoend = false;
      if (testsuite->runtest) {
        ok = TS_FINISHED;
      }
    }
    if (strncmp (tcmd, "reset", 5) == 0) {
      fprintf (stdout, "\n");
      clearResults (&testsuite->results);
      resetPlayer (testsuite);
      resetChkResponse (testsuite);
      testsuite->expectresponse = false;
      testsuite->wait = false;
      testsuite->chkwait = false;
      ok = TS_OK;
      testsuite->skiptoend = false;
    }
  }

  processtest =
      testsuite->skiptoend == false &&
      testsuite->processtest;

  if (processtest) {
    if (strncmp (tcmd, "resptimeout", 11) == 0) {
      testsuite->responseTimeout = atol (tcmd + 12);
      ok = TS_OK;
      disp = true;
    }
    if (strncmp (tcmd, "msg", 3) == 0) {
      ok = tsScriptMsg (testsuite, tcmd);
      disp = true;
    }
    if (strncmp (tcmd, "get", 3) == 0) {
      ok = tsScriptGet (testsuite, tcmd);
      disp = true;
    }
    /* chk, chk-or, chk-lt, chk-gt, chk-not */
    if (strncmp (tcmd, "chk", 3) == 0) {
      ok = tsScriptChk (testsuite, tcmd);
      disp = true;
    }
    if (strncmp (tcmd, "wait", 4) == 0) {
      ok = tsScriptWait (testsuite, tcmd);
      disp = true;
    }
    if (strncmp (tcmd, "mssleep", 7) == 0) {
      ok = tsScriptSleep (testsuite, tcmd);
      disp = true;
    }
    if (strncmp (tcmd, "disp", 4) == 0) {
      ok = tsScriptDisp (testsuite, tcmd);
      disp = true;
    }
    if (strncmp (tcmd, "print", 4) == 0) {
      ok = tsScriptPrint (testsuite, tcmd);
    }
  } else {
    ok = TS_OK;
  }

  if (testsuite->verbose && disp) {
    char  ttm [40];

    tmutilTstamp (ttm, sizeof (ttm));
    fprintf (stdout, "      %3d %s %s\n", testsuite->lineno, ttm, tcmd);
    fflush (stdout);
  }
  tsDisplayCommandResult (testsuite, ok);

  return false;
}

static void
clearResults (results_t *results)
{
  results->testcount = 0;
  results->testfail = 0;
  results->chkcount = 0;
  results->chkfail = 0;
}

static void
startResultTimer (results_t *results)
{
  mstimestart (&results->start);
}

static void
tallyResults (testsuite_t *testsuite)
{
  testsuite->sresults.testcount += testsuite->results.testcount;
  testsuite->sresults.testfail += testsuite->results.testfail;
  testsuite->sresults.chkcount += testsuite->results.chkcount;
  testsuite->sresults.chkfail += testsuite->results.chkfail;
  testsuite->gresults.testcount += testsuite->results.testcount;
  testsuite->gresults.testfail += testsuite->results.testfail;
  testsuite->gresults.chkcount += testsuite->results.chkcount;
  testsuite->gresults.chkfail += testsuite->results.chkfail;
}

static void
printResults (testsuite_t *testsuite, results_t *results)
{
  const char  *state = "FAIL";

  if (results->chkcount == 0 || results->chkfail == 0) {
    state = "OK";
  } else if (results->testfail == 0) {
    slistSetStr (testsuite->failedlist, testsuite->testnum, NULL);
    ++results->testfail;
  }
  if (! *testsuite->testname) {
    fprintf (stdout, "   %s %s ", testsuite->sectionnum, testsuite->sectionname);
  }
  if (*testsuite->testname) {
    if (testsuite->verbose) {
      fprintf (stdout, "   -- %s %s %s ", testsuite->testnum,
          testsuite->sectionname, testsuite->testname);
    } else {
      fprintf (stdout, "   ");
    }
  }
  fprintf (stdout, "checks: %d failed: %d %s",
      results->chkcount, results->chkfail, state);
  fprintf (stdout, "  (%zd)", mstimeend (&results->start));
  fprintf (stdout, "\n");
  fflush (stdout);
}

static int
tsScriptSection (testsuite_t *testsuite, const char *tcmd)
{
  char    *p = NULL;
  char    *tokstr = NULL;
  char    *tstr = NULL;

  if (testsuite->processsection) {
    strlcpy (testsuite->testnum, "", sizeof (testsuite->testnum));
    strlcpy (testsuite->testname, "", sizeof (testsuite->testname));
    printResults (testsuite, &testsuite->sresults);
    if (testsuite->runsection) {
      testsuite->processsection = false;
      return TS_FINISHED;
    }
  }

  tstr = strdup (tcmd);
  p = strtok_r (tstr, " ", &tokstr);
  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    free (tstr);
    return TS_BAD_COMMAND;
  }
  strlcpy (testsuite->sectionnum, p, sizeof (testsuite->sectionnum));

  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    free (tstr);
    return TS_BAD_COMMAND;
  }
  strlcpy (testsuite->sectionname, p, sizeof (testsuite->sectionname));

  clearResults (&testsuite->results);
  clearResults (&testsuite->sresults);
  startResultTimer (&testsuite->sresults);

  testsuite->processsection = false;

  if (testsuite->runsection) {
    if (strcmp (testsuite->sectionnum, bdjvarsGetStr (BDJV_TS_SECTION)) == 0) {
      testsuite->processsection = true;
    }
  } else if (! testsuite->runtest) {
    /* if no runsection/runtest was specified, run the section */
    testsuite->processsection = true;
  }

  if (testsuite->processsection) {
    fprintf (stdout, "== %s %s\n", testsuite->sectionnum, testsuite->sectionname);
    fflush (stdout);
  }

  free (tstr);
  return TS_OK;
}

static int
tsScriptTest (testsuite_t *testsuite, const char *tcmd)
{
  char    *p;
  char    *tokstr;
  char    *tstr;

  tstr = strdup (tcmd);
  p = strtok_r (tstr, " ", &tokstr);
  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    free (tstr);
    return TS_BAD_COMMAND;
  }
  strlcpy (testsuite->testnum, p, sizeof (testsuite->testnum));
  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    free (tstr);
    return TS_BAD_COMMAND;
  }
  strlcpy (testsuite->testname, p, sizeof (testsuite->testname));

  clearResults (&testsuite->results);
  startResultTimer (&testsuite->results);

  testsuite->processtest = false;

  if (testsuite->runtest) {
    if (strcmp (testsuite->testnum, bdjvarsGetStr (BDJV_TS_TEST)) == 0) {
      testsuite->processtest = true;
    }
  }

  if (testsuite->processsection) {
    testsuite->processtest = true;
  }

  if (testsuite->processtest) {
    logMsg (LOG_DBG, LOG_BASIC, "-- test: %3d %s", testsuite->lineno, tcmd);
    fprintf (stdout, "   -- %s %s %s", testsuite->testnum,
        testsuite->sectionname, testsuite->testname);
    if (testsuite->verbose) {
      fprintf (stdout, "\n");
    }
    fflush (stdout);
  }

  free (tstr);
  return TS_OK;
}


static int
tsScriptMsg (testsuite_t *testsuite, const char *tcmd)
{
  int   ok;

  ok = tsSendMessage (testsuite, tcmd, TS_TYPE_MSG);
  return ok;
}

static int
tsScriptGet (testsuite_t *testsuite, const char *tcmd)
{
  int   ok;

  resetChkResponse (testsuite);
  testsuite->expectresponse = true;
  ok = tsSendMessage (testsuite, tcmd, TS_TYPE_GET);
  return ok;
}

static int
tsScriptChk (testsuite_t *testsuite, const char *tcmd)
{
  int   ok;

  ok = tsParseExpect (testsuite, tcmd);
  if (ok != TS_OK) {
    return ok;
  }
  testsuite->chkwait = true;
  ++testsuite->results.chkcount;
  logMsg (LOG_DBG, LOG_BASIC, "start response timer 200");
  mstimeset (&testsuite->responseTimeoutCheck, TS_CHK_TIMEOUT);
  return TS_OK;
}

static int
tsScriptWait (testsuite_t *testsuite, const char *tcmd)
{
  int   ok;

  testsuite->lessthan = false;
  testsuite->greaterthan = false;
  testsuite->checkor = false;
  testsuite->checknot = false;

  ok = tsParseExpect (testsuite, tcmd);
  if (ok != TS_OK) {
    return ok;
  }
  if (testsuite->expectresponse == false && ! testsuite->haveresponse) {
    testsuite->expectresponse = true;
  }
  testsuite->wait = true;
  ++testsuite->results.chkcount;
  mstimeset (&testsuite->waitCheck, 100);
  logMsg (LOG_DBG, LOG_BASIC, "start response timer %d", testsuite->responseTimeout);
  mstimeset (&testsuite->responseTimeoutCheck, testsuite->responseTimeout);
  return TS_OK;
}

static int
tsScriptSleep (testsuite_t *testsuite, const char *tcmd)
{
  char    *tstr;
  char    *p;
  char    *tokstr;
  time_t  t;

  tstr = strdup (tcmd);
  p = strtok_r (tstr, " ", &tokstr);
  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    free (tstr);
    return TS_BAD_COMMAND;
  }
  t = atol (p);
  mssleep (t);
  free (tstr);
  return TS_OK;
}

static int
tsScriptDisp (testsuite_t *testsuite, const char *tcmd)
{
  char        *tstr;
  char        *p;
  char        *tokstr;
  char        *key;
  char        *valchk;
  slistidx_t  iteridx;

  tstr = strdup (tcmd);
  p = strtok_r (tstr, " ", &tokstr);
  if (strcmp (p, "dispall") == 0) {
    slistStartIterator (testsuite->chkresponse, &iteridx);
    while ((key = slistIterateKey (testsuite->chkresponse, &iteridx)) != NULL) {
      valchk = slistGetStr (testsuite->chkresponse, key);
      fprintf (stdout, "       %s %s\n", key, valchk);
    }
  } else {
    p = strtok_r (NULL, " ", &tokstr);
    while (p != NULL) {
      valchk = slistGetStr (testsuite->chkresponse, p);
      fprintf (stdout, "       %s %s\n", p, valchk);
      p = strtok_r (NULL, " ", &tokstr);
    }
  }

  fflush (stdout);
  free (tstr);
  return TS_OK;
}

static int
tsScriptPrint (testsuite_t *testsuite, const char *tcmd)
{
  char  *p;
  char  *tokstr;
  char  *tstr;

  tstr = strdup (tcmd);
  p = strtok_r (tstr, " ", &tokstr);
  p += strlen (p) + 1;
  if (p == NULL) {
    free (tstr);
    return TS_BAD_COMMAND;
  }
  fprintf (stdout, " %s", p);
  fflush (stdout);
  free (tstr);
  return TS_OK;
}

static int
tsParseExpect (testsuite_t *testsuite, const char *tcmd)
{
  char    *p;
  char    *tokstr;
  char    *tstr;

  if (testsuite->chkexpect != NULL) {
    slistFree (testsuite->chkexpect);
  }
  testsuite->chkexpect = slistAlloc ("ts-chk-expect", LIST_UNORDERED, free);

  tstr = strdup (tcmd);
  p = strtok_r (tstr, " ", &tokstr);
  if (strcmp (p, "chk-gt") == 0) {
    testsuite->greaterthan = true;
  }
  if (strcmp (p, "chk-lt") == 0) {
    testsuite->lessthan = true;
  }
  if (strcmp (p, "chk-or") == 0) {
    testsuite->checkor = true;
  }
  if (strcmp (p, "chk-not") == 0) {
    testsuite->checknot = true;
  }

  p = strtok_r (NULL, " ", &tokstr);
  while (p != NULL) {
    char    *a = NULL;

    if (testsuite->checkor) {
      slistSetStr (testsuite->chkexpect, p, NULL);
    } else {
      a = strtok_r (NULL, " ", &tokstr);
      if (a == NULL) {
        free (tstr);
        return TS_BAD_COMMAND;
      }
      slistSetStr (testsuite->chkexpect, p, a);
    }
    p = strtok_r (NULL, " ", &tokstr);
  }
  if (! testsuite->checkor) {
    slistSort (testsuite->chkexpect);
  }

  free (tstr);
  return TS_OK;
}

static int
tsScriptChkResponse (testsuite_t *testsuite)
{
  int         rc = TS_CHECK_FAILED;
  int         count = 0;
  int         countok = 0;
  bool        dispflag = false;
  slistidx_t  iteridx;
  const char  *key;
  const char  *val;
  const char  *valchk;
  char        tmp [40];
  bool        retchk = false;

  slistStartIterator (testsuite->chkexpect, &iteridx);

  if (testsuite->checkor) {
    key = slistIterateKey (testsuite->chkexpect, &iteridx);
    valchk = slistGetStr (testsuite->chkresponse, key);
    count = 1;
    while ((val = slistIterateKey (testsuite->chkexpect, &iteridx)) != NULL) {
      if (valchk != NULL && strcmp (val, valchk) == 0) {
        ++countok;
        break;
      }
    }

    if (count != countok) {
      if (! dispflag) {
        fprintf (stdout, "\n");
        dispflag = true;
      }
      fprintf (stdout, "          %3d chk-or-fail\n", testsuite->lineno);
      fflush (stdout);
    }
  } else {
    char  *compdisp;

    while ((key = slistIterateKey (testsuite->chkexpect, &iteridx)) != NULL) {
      val = slistGetStr (testsuite->chkexpect, key);
      valchk = slistGetStr (testsuite->chkresponse, key);

      if (strcmp (val, "defaultvol") == 0) {
        snprintf (tmp, sizeof (tmp), "%ld", testsuite->defaultVol);
        val = tmp;
      }

      logMsg (LOG_DBG, LOG_BASIC, "exp-resp: %s %s %s", key, val, valchk);
      if (testsuite->lessthan) {
        long  a, b;

        a = atol (val);
        b = -2;
        if (valchk != NULL) {
          b = atol (valchk);
        }
        if (valchk != NULL && b < a) {
          if (testsuite->verbose) {
            fprintf (stdout, "          %3d chk-ok: %s: resp: %ld < exp: %ld\n",
                  testsuite->lineno, key, b, a);
            fflush (stdout);
          }
          ++countok;
        } else {
          if (! dispflag) {
            fprintf (stdout, "\n");
            dispflag = true;
          }
          fprintf (stdout, "          %3d chk-lt-fail: %s: resp: %ld > exp: %ld\n",
              testsuite->lineno, key, b, a);
          fflush (stdout);
        }
      }
      if (testsuite->greaterthan) {
        long  a, b;

        a = atol (val);
        b = -2;
        if (valchk != NULL) {
          b = atol (valchk);
        }
        if (valchk != NULL && b > a) {
          if (testsuite->verbose) {
            fprintf (stdout, "          %3d chk-ok: %s: resp: %ld > exp: %ld\n",
                  testsuite->lineno, key, b, a);
            fflush (stdout);
          }
          ++countok;
        } else {
          if (! dispflag) {
            fprintf (stdout, "\n");
            dispflag = true;
          }
          fprintf (stdout, "          %3d chk-gt-fail: %s: resp: %ld < exp: %ld\n",
              testsuite->lineno, key, b, a);
          fflush (stdout);
        }
      }
      retchk = false;
      if (val != NULL && valchk != NULL) {
        if (testsuite->checknot) {
          retchk = strcmp (val, valchk) != 0;
        } else {
          retchk = strcmp (val, valchk) == 0;
        }
      }
      if (! testsuite->lessthan && ! testsuite->greaterthan &&
          val != NULL && valchk != NULL && retchk) {
        if (testsuite->verbose) {
          compdisp = "==";
          if (testsuite->checknot) {
            compdisp = "!=";
          }
          if (! dispflag) {
            fprintf (stdout, "\n");
            dispflag = true;
          }
          fprintf (stdout, "          %3d chk-ok: %s: exp: %s %s resp: %s\n",
                testsuite->lineno, key, val, compdisp, valchk);
          fflush (stdout);
        }
        ++countok;
      } else if (testsuite->chkwait &&
          ! testsuite->lessthan && ! testsuite->greaterthan) {
        compdisp = "!=";
        if (testsuite->checknot) {
          compdisp = "==";
        }
        if (! dispflag) {
          fprintf (stdout, "\n");
          dispflag = true;
        }
        fprintf (stdout, "          %3d chk-fail: %s: exp: %s %s resp: %s\n",
              testsuite->lineno, key, val, compdisp, valchk);
        fflush (stdout);
      } else if (testsuite->wait && testsuite->verbose &&
          ! testsuite->lessthan && ! testsuite->greaterthan) {
        compdisp = "!=";
        if (testsuite->checknot) {
          compdisp = "==";
        }
        if (testsuite->lastResponse == NULL ||
            (valchk != NULL && strcmp (valchk, testsuite->lastResponse) != 0)) {
          fprintf (stdout, "          %3d wait-test: %s: exp: %s %s resp: %s\n",
                testsuite->lineno, key, val, compdisp, valchk);
          fflush (stdout);
          if (valchk != NULL) {
            if (testsuite->lastResponse != NULL) {
              free (testsuite->lastResponse);
            }
            testsuite->lastResponse = strdup (valchk);
          }
        }
      }
      ++count;
    }
  }

  if (count == countok) {
    rc = TS_OK;
  }

  return rc;
}


static void
tsProcessChkResponse (testsuite_t *testsuite, char *args)
{
  char    *p;
  char    *a;
  char    *tokstr;

  logMsg (LOG_DBG, LOG_BASIC, "resp: %s", args);
  if (testsuite->expectresponse) {
    testsuite->expectresponse = false;
    testsuite->haveresponse = true;
  }

  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  while (p != NULL) {
    a = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    if (a == NULL) {
      return;
    }
    if (*a == MSG_ARGS_EMPTY) {
      *a = '\0';
    }
    slistSetStr (testsuite->chkresponse, p, a);
    p = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
  }
}

static int
tsSendMessage (testsuite_t *testsuite, const char *tcmd, int type)
{
  char    *tstr;
  char    *p;
  char    *d;
  size_t  dlen;
  char    *tokstr;
  int     route;
  int     msg;
  char    tmp [40];

  tstr = strdup (tcmd);

  /* test-script command */
  p = strtok_r (tstr, " ", &tokstr);
  if (p == NULL) {
    return TS_BAD_COMMAND;
  }

  /* route */
  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    free (tstr);
    return TS_BAD_ROUTE;
  }
  stringAsciiToUpper (p);
  route = slistGetNum (testsuite->routetxtlist, p);
  if (route < 0) {
    free (tstr);
    return TS_BAD_ROUTE;
  }

  /* message */
  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    free (tstr);
    return TS_BAD_MSG;
  }
  stringAsciiToUpper (p);
  msg = slistGetNum (testsuite->msgtxtlist, p);
  if (msg < 0) {
    free (tstr);
    return TS_BAD_MSG;
  }

  /* args */
  p = strtok_r (NULL, " ", &tokstr);
  d = NULL;
  if (type == TS_TYPE_MSG && p != NULL) {
    if (strcmp (p, "defaultvol") == 0) {
      snprintf (tmp, sizeof (tmp), "%ld", testsuite->defaultVol);
      p = tmp;
    }

    dlen = strlen (p);
    d = filedataReplace (p, &dlen, "~", MSG_ARGS_RS_STR);
  }
  if (type == TS_TYPE_GET) {
    testsuite->waitRoute = route;
    testsuite->waitMessage = msg;
  }

  connSendMessage (testsuite->conn, route, msg, d);
  free (d);
  free (tstr);
  return TS_OK;
}

static void
tsDisplayCommandResult (testsuite_t *testsuite, ts_return_t ok)
{
  const char  *tmsg;

  tmsg = "unknown";
  switch (ok) {
    case TS_OK: {
      tmsg = "ok";
      break;
    }
    case TS_BAD_ROUTE: {
      tmsg = "bad route";
      break;
    }
    case TS_BAD_MSG: {
      tmsg = "bad msg";
      break;
    }
    case TS_BAD_COMMAND: {
      tmsg = "bad command";
      break;
    }
    case TS_UNKNOWN: {
      tmsg = "unknown cmd";
      break;
    }
    case TS_NOT_IMPLEMENTED: {
      tmsg = "not implemented";
      break;
    }
    case TS_CHECK_FAILED: {
      tmsg = "check failed";
      break;
    }
    case TS_CHECK_TIMEOUT: {
      tmsg = "check timeout";
      break;
    }
    case TS_FINISHED: {
      tmsg = "finished";
      break;
    }
  }
  if (ok != TS_OK && ok != TS_FINISHED) {
    fprintf (stdout, "\n");
    fprintf (stdout, "          %3d %s\n", testsuite->lineno, tmsg);
    fflush (stdout);
  }
}

static void
resetChkResponse (testsuite_t *testsuite)
{
  if (testsuite->chkresponse != NULL) {
    slistFree (testsuite->chkresponse);
  }
  testsuite->chkresponse = slistAlloc ("ts-chk-response", LIST_ORDERED, free);
  testsuite->haveresponse = false;
  testsuite->lessthan = false;
  testsuite->greaterthan = false;
  testsuite->checkor = false;
  testsuite->checknot = false;
  testsuite->chkwait = false;
}

static void
resetPlayer (testsuite_t *testsuite)
{
  /* clears both queue and playlist queue, resets manage idx */
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_QUEUE_CLEAR, "1");
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_QUEUE_CLEAR, "0");
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_MUSICQ_SET_PLAYBACK, "1");
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_CMD_NEXTSONG, NULL);
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_MUSICQ_SET_PLAYBACK, "0");
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_CMD_NEXTSONG, NULL);
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_MUSICQ_SET_MANAGE, "0");
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_QUEUE_PLAY_ON_ADD, "0");
  /* macos seems to need this sleep. */
  /* there may be a race condition between the 'end' and  */
  /* the start of the next test. or main needs to wait to receive a */
  /* message from the player. */
  /* this should be researched at a later date to find out exactly */
  /* what's going on. */
  mssleep (200);
  connSendMessage (testsuite->conn, ROUTE_MAIN, MSG_QUEUE_SWITCH_EMPTY, "0");
}

static bool
tsNextFile (testsuite_t *testsuite)
{
  const char  *fn;
  bool        rc = false;
  char        tbuff [MAXPATHLEN];

  testsuite->lineno = 0;

  if (testsuite->fh != NULL) {
    fclose (testsuite->fh);
    testsuite->fh = NULL;
  }

  while (1) {
    fn = slistIterateKey (testsuite->testlist, &testsuite->tliteridx);
    if (fn != NULL && strcmp (fn, "README.txt") == 0) {
      continue;
    }
    break;
  }

  if (fn != NULL) {
    logMsg (LOG_DBG, LOG_BASIC, "-- file: %s", fn);
    snprintf (tbuff, sizeof (tbuff), "test-templates/tests/%s", fn);
    testsuite->fh = fopen (tbuff, "r");
  } else {
    rc = true;
  }

  return rc;
}

static void
tsSigHandler (int sig)
{
  gKillReceived = 1;
}
