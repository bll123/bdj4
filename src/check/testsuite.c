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
#include "bdjstring.h"
#include "bdjvars.h"
#include "conn.h"
#include "filedata.h"
#include "fileop.h"
#include "istring.h"
#include "localeutil.h"
#include "log.h"
#include "osrandom.h"
#include "ossignal.h"
#include "procutil.h"
#include "progstate.h"
#include "slist.h"
#include "sockh.h"
#include "sysvars.h"
#include "tmutil.h"

typedef struct {
  int     testcount;
  int     testfail;
  int     chkcount;
  int     chkfail;
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
} ts_return_t;

enum {
  TS_TYPE_MSG,
  TS_TYPE_CHK,
  TS_TYPE_WAIT,
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
  bdjmsgroute_t waitRoute;
  bdjmsgmsg_t waitMessage;
  mstime_t    waitCheck;
  results_t   results;
  results_t   gresults;
  char        tsection [80];
  char        tname [80];
  FILE        *fh;
  int         step;
  int         lineno;
  bool        waitresponse : 1;
  bool        haveresponse : 1;
  bool        wait : 1;
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
static void tallyResults (testsuite_t *testsuite);
static void printResults (testsuite_t *testsuite, results_t *results);
static int  tsProcessScript (testsuite_t *testsuite);
static int  tsScriptMsg (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptChk (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptWait (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptSleep (testsuite_t *testsuite, const char *tcmd);
static int  tsScriptChkResponse (testsuite_t *testsuite);
static int  tsSendMessage (testsuite_t *testsuite, const char *tcmd, int type);
static void tsProcessChkResponse (testsuite_t *testsuite, char *args);
static void tsDisplayCommandResult (testsuite_t *testsuite, ts_return_t ok);
static void tsSigHandler (int sig);

int
main (int argc, char *argv [])
{
  testsuite_t testsuite;
  int         flags;
  int         listenPort;
  char        *state;

  osSetStandardSignals (tsSigHandler);
  osCatchSignal (tsSigHandler, SIGINT);

  flags = BDJ4_INIT_NO_DB_LOAD | BDJ4_INIT_NO_DATAFILE_LOAD;
  bdj4startup (argc, argv, NULL, "ts", ROUTE_TEST_SUITE, flags);
  logEnd ();
  logStart ("testsuite", "ts",
      LOG_IMPORTANT | LOG_BASIC | LOG_MAIN | LOG_MSGS | LOG_ACTIONS);

  procutilInitProcesses (testsuite.processes);
  testsuite.conn = connInit (ROUTE_TEST_SUITE);
  clearResults (&testsuite.results);
  clearResults (&testsuite.gresults);
  testsuite.progstate = progstateInit ("testsuite");
  testsuite.stopwaitcount = 0;
  testsuite.waitresponse = false;
  testsuite.haveresponse = false;
  testsuite.wait = false;
  testsuite.chkresponse = NULL;
  testsuite.chkexpect = NULL;
  mstimeset (&testsuite.waitCheck, 100);
  mstimeset (&testsuite.responseTimeoutCheck, TS_CHK_TIMEOUT);
  strlcpy (testsuite.tsection, "Init", sizeof (testsuite.tsection));
  strlcpy (testsuite.tname, "Init", sizeof (testsuite.tname));

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

  testsuite.fh = fopen ("test-templates/test-script.txt", "r");
  testsuite.lineno = 0;
  testsuite.step = 0;

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

  strlcpy (testsuite.tsection, "Final", sizeof (testsuite.tsection));
  strlcpy (testsuite.tname, "", sizeof (testsuite.tname));
  state = "FAIL";
  if (testsuite.gresults.testfail == 0) {
    state = "OK";
  }
  printResults (&testsuite, &testsuite.gresults);
  fprintf (stdout, "%s %s tests: %d failed: %d\n",
      state, testsuite.tsection,
      testsuite.gresults.testcount, testsuite.gresults.testfail);

  progstateFree (testsuite.progstate);
  logEnd ();

  if (fileopFileExists ("core")) {
    fprintf (stdout, "core dumped\n");
  }
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
        case MSG_CHK_MAIN_STATUS:
        case MSG_CHK_PLAYER_STATUS: {
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

  if (testsuite->waitresponse && testsuite->haveresponse) {
    ts_return_t ok;

    logMsg (LOG_DBG, LOG_BASIC, "process chk response");
    ok = tsScriptChkResponse (testsuite);
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
      testsuite->haveresponse = false;
      testsuite->waitresponse = false;
      testsuite->wait = false;
    }
  }

  if ((testsuite->waitresponse || testsuite->wait) &&
      mstimeCheck (&testsuite->responseTimeoutCheck)) {
    logMsg (LOG_DBG, LOG_BASIC, "timed out %d", testsuite->lineno);
    testsuite->haveresponse = false;
    testsuite->waitresponse = false;
    testsuite->wait = false;
    ++testsuite->results.chkfail;
    tsDisplayCommandResult (testsuite, TS_CHECK_TIMEOUT);
  }

  if ((testsuite->wait) &&
      mstimeCheck (&testsuite->waitCheck)) {
    connSendMessage (testsuite->conn, testsuite->waitRoute,
        testsuite->waitMessage, NULL);
    mstimeset (&testsuite->waitCheck, 100);
  }

  if (testsuite->wait == false && testsuite->waitresponse == false) {
    if (tsProcessScript (testsuite)) {
      progstateShutdownProcess (testsuite->progstate);
      logMsg (LOG_DBG, LOG_BASIC, "finished script");
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

  return STATE_FINISHED;
}


static int
tsProcessScript (testsuite_t *testsuite)
{
  ts_return_t ok;
  bool        disp = false;
  char        tcmd [200];

  if (fgets (tcmd, sizeof (tcmd), testsuite->fh) == NULL) {
    return true;
  }

  ++testsuite->step;
  ++testsuite->lineno;

  if (*tcmd == '#') {
    return false;
  }
  if (*tcmd == '\n') {
    return false;
  }
  stringTrim (tcmd);

  ok = TS_UNKNOWN;
  logMsg (LOG_DBG, LOG_BASIC, "-- cmd: %2d %3d %s", testsuite->step, testsuite->lineno, tcmd);
  if (strncmp (tcmd, "section", 7) == 0) {
    strlcpy (testsuite->tsection, tcmd + 8, sizeof (testsuite->tsection));
    clearResults (&testsuite->results);
    fprintf (stdout, "== %s\n", testsuite->tsection);
    fflush (stdout);
    ok = TS_OK;
  }
  if (strncmp (tcmd, "test", 4) == 0) {
    testsuite->step = 0;
    strlcpy (testsuite->tname, tcmd + 5, sizeof (testsuite->tname));
    clearResults (&testsuite->results);
    fprintf (stdout, "-- %s %s\n", testsuite->tsection, testsuite->tname);
    fflush (stdout);
    ok = TS_OK;
  }
  if (strncmp (tcmd, "resptimeout", 11) == 0) {
    testsuite->responseTimeout = atol (tcmd + 12);
    ok = TS_OK;
    disp = true;
  }
  if (strncmp (tcmd, "msg", 3) == 0) {
    ok = tsScriptMsg (testsuite, tcmd);
    disp = true;
  }
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
  if (strncmp (tcmd, "end", 3) == 0) {
    ++testsuite->results.testcount;
    printResults (testsuite, &testsuite->results);
    tallyResults (testsuite);
    clearResults (&testsuite->results);
    ok = TS_OK;
  }
  if (strncmp (tcmd, "reset", 5) == 0) {
    clearResults (&testsuite->results);
    ok = TS_OK;
  }

  if (disp) {
    fprintf (stdout, "   %2d %3d %s\n", testsuite->step, testsuite->lineno, tcmd);
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
tallyResults (testsuite_t *testsuite)
{
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
  } else {
    ++results->testfail;
  }
  fprintf (stdout, "   %s %s %s checks: %d failed: %d\n",
      state, testsuite->tsection, testsuite->tname,
      results->chkcount, results->chkfail);
  fflush (stdout);
}


static int
tsScriptMsg (testsuite_t *testsuite, const char *tcmd)
{
  tsSendMessage (testsuite, tcmd, TS_TYPE_MSG);
  return TS_OK;
}

static int
tsScriptChk (testsuite_t *testsuite, const char *tcmd)
{
  testsuite->waitresponse = true;
  testsuite->haveresponse = false;
  tsSendMessage (testsuite, tcmd, TS_TYPE_CHK);
  ++testsuite->results.chkcount;
  mstimeset (&testsuite->responseTimeoutCheck, TS_CHK_TIMEOUT);
  return TS_OK;
}

static int
tsScriptWait (testsuite_t *testsuite, const char *tcmd)
{
  testsuite->wait = true;
  testsuite->haveresponse = false;
  tsSendMessage (testsuite, tcmd, TS_TYPE_WAIT);
  ++testsuite->results.chkcount;
  mstimeset (&testsuite->waitCheck, 100);
  mstimeset (&testsuite->responseTimeoutCheck, testsuite->responseTimeout);
  return TS_OK;
}

static int
tsScriptChkResponse (testsuite_t *testsuite)
{
  int         rc = TS_CHECK_FAILED;
  int         count = 0;
  int         countok = 0;
  slistidx_t  iteridx;
  const char  *key;
  const char  *val;
  const char  *valchk;

  slistStartIterator (testsuite->chkexpect, &iteridx);
  while ((key = slistIterateKey (testsuite->chkexpect, &iteridx)) != NULL) {
    val = slistGetStr (testsuite->chkexpect, key);
    valchk = slistGetStr (testsuite->chkresponse, key);
    logMsg (LOG_DBG, LOG_BASIC, "exp-resp: %s %s %s", key, val, valchk);
    if (valchk != NULL && strcmp (val, valchk) == 0) {
      ++countok;
    }
    ++count;
  }

  if (count == countok) {
    rc = TS_OK;
  }

  testsuite->waitresponse = false;
  testsuite->haveresponse = false;
  return rc;
}


static int
tsScriptSleep (testsuite_t *testsuite, const char *tcmd)
{
  char  *tstr;
  char  *p;
  char  *tokstr;

  tstr = strdup (tcmd);
  p = strtok_r (tstr, " ", &tokstr);
  p = strtok_r (NULL, " ", &tokstr);
  mssleep (atol (p));
  free (tstr);
  return TS_OK;
}

static void
tsProcessChkResponse (testsuite_t *testsuite, char *args)
{
  char    *p;
  char    *a;
  char    *tokstr;

  logMsg (LOG_DBG, LOG_BASIC, "resp: %s", args);
  if (testsuite->chkresponse != NULL) {
    slistFree (testsuite->chkresponse);
  }
  testsuite->chkresponse = slistAlloc ("ts-chk-response", LIST_ORDERED, free);
  testsuite->haveresponse = true;

  p = strtok_r (args, MSG_ARGS_RS_STR, &tokstr);
  while (p != NULL) {
    a = strtok_r (NULL, MSG_ARGS_RS_STR, &tokstr);
    if (a == NULL) {
      return;
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

  tstr = strdup (tcmd);

  /* test-script command */
  p = strtok_r (tstr, " ", &tokstr);
  if (p == NULL) {
    return TS_BAD_COMMAND;
  }

  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    return TS_BAD_ROUTE;
  }
  stringAsciiToUpper (p);
  route = slistGetNum (testsuite->routetxtlist, p);
  if (route < 0) {
    free (tstr);
    return TS_BAD_ROUTE;
  }
  p = strtok_r (NULL, " ", &tokstr);
  if (p == NULL) {
    return TS_BAD_MSG;
  }
  stringAsciiToUpper (p);
  msg = slistGetNum (testsuite->msgtxtlist, p);
  if (msg < 0) {
    free (tstr);
    return TS_BAD_MSG;
  }
  p = strtok_r (NULL, " ", &tokstr);
  d = NULL;
  if (type == TS_TYPE_MSG && p != NULL) {
    dlen = strlen (p);
    d = filedataReplace (p, &dlen, "~", MSG_ARGS_RS_STR);
  }
  if ((type == TS_TYPE_CHK || type == TS_TYPE_WAIT) && p != NULL) {
    if (testsuite->chkexpect != NULL) {
      slistFree (testsuite->chkexpect);
    }
    testsuite->chkexpect = slistAlloc ("ts-chk-expect", LIST_ORDERED, free);

    while (p != NULL) {
      char    *a;
      a = strtok_r (NULL, " ", &tokstr);
      if (a == NULL) {
        return TS_BAD_COMMAND;
      }
      slistSetStr (testsuite->chkexpect, p, a);
      p = strtok_r (NULL, " ", &tokstr);
    }

    if (type == TS_TYPE_WAIT) {
      testsuite->waitRoute = route;
      testsuite->waitMessage = msg;
    }
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
  }
  if (ok != TS_OK) {
    fprintf (stdout, "       %s\n", tmsg);
    fflush (stdout);
  }
}


static void
tsSigHandler (int sig)
{
  gKillReceived = 1;
}

