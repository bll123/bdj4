/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
/*
 *  bdj4dbtag
 *  reads the tags from an audio file.
 *  uses pthreads.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>

#include "audiotag.h"
#include "bdj4.h"
#include "bdj4init.h"
#include "bdjmsg.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "bdjvarsdf.h"
#include "bdjvars.h"
#include "conn.h"
#include "lock.h"
#include "log.h"
#include "mdebug.h"
#include "ossignal.h"
#include "pathbld.h"
#include "progstate.h"
#include "procutil.h"
#include "queue.h"
#include "slist.h"
#include "sockh.h"
#include "sysvars.h"
#include "tmutil.h"

#define DBTAG_TMP_FN        "dbtag"

enum {
  DBTAG_T_STATE_INIT,
  DBTAG_T_STATE_ACTIVE,
  DBTAG_T_STATE_HAVE_DATA,
  DBTAG_STATE_NOT_RUNNING,
  DBTAG_STATE_RUNNING,
  DBTAG_STATE_WAIT,
  DBTAG_STATE_FINISH,
};

typedef struct {
  pthread_t   thread;
  int         state;
  int         idx;
  long        count;
  char        *fn;
  void        * data;
} dbthread_t;

typedef struct {
  progstate_t       *progstate;
  procutil_t        *processes [ROUTE_MAX];
  conn_t            *conn;
  char              *locknm;
  int               stopwaitcount;
  int               numActiveThreads;
  dbidx_t           received;
  dbidx_t           sent;
  qidx_t            maxqueuelen;
  queue_t           *fileQueue;
  int               maxThreads;
  dbthread_t        *threads;
  mstime_t          starttm;
  mstime_t          waitCheck;
  int               iterations;
  int               threadActiveSum;
  int               running;
  bool              havealldata : 1;
} dbtag_t;

static int      dbtagProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
                    bdjmsgmsg_t msg, char *args, void *udata);
static int      dbtagProcessing (void *udata);
static bool     dbtagConnectingCallback (void *tdbtag, programstate_t programState);
static bool     dbtagHandshakeCallback (void *tdbtag, programstate_t programState);
static bool     dbtagStoppingCallback (void *tdbtag, programstate_t programState);
static bool     dbtagStopWaitCallback (void *tdbtag, programstate_t programState);
static bool     dbtagClosingCallback (void *tdbtag, programstate_t programState);
static void     dbtagProcessFileMsg (dbtag_t *dbtag, char *args);
static void     * dbtagProcessFile (void *tdbthread);
static void     dbtagSigHandler (int sig);

static int  gKillReceived = 0;
static int  gcount = 0;

int
main (int argc, char *argv[])
{
  dbtag_t     dbtag;
  uint16_t    listenPort;
  long        flags;

#if BDJ4_MEM_DEBUG
  mdebugInit ("dbtg");
#endif

  osSetStandardSignals (dbtagSigHandler);

  flags = BDJ4_INIT_NO_DB_LOAD | BDJ4_INIT_NO_DATAFILE_LOAD;
  bdj4startup (argc, argv, NULL, "dbtg", ROUTE_DBTAG, &flags);
  logProcBegin (LOG_PROC, "dbtag");

  dbtag.maxThreads = sysvarsGetNum (SVL_NUM_PROC);
#if BDJ4_MEM_DEBUG
  dbtag.maxThreads = 1;
#endif

  dbtag.threads = mdmalloc (sizeof (dbthread_t) * dbtag.maxThreads);
  dbtag.running = DBTAG_STATE_NOT_RUNNING;
  dbtag.havealldata = false;
  dbtag.numActiveThreads = 0;
  dbtag.iterations = 0;
  dbtag.threadActiveSum = 0;
  dbtag.progstate = progstateInit ("dbtag");
  dbtag.stopwaitcount = 0;
  for (int i = 0; i < dbtag.maxThreads; ++i) {
    dbtag.threads [i].state = DBTAG_T_STATE_INIT;
    dbtag.threads [i].idx = i;
    dbtag.threads [i].count = 0;
    dbtag.threads [i].fn = NULL;
    dbtag.threads [i].data = NULL;
  }
  dbtag.fileQueue = queueAlloc ("file-q", NULL);
  dbtag.received = 0;
  dbtag.sent = 0;
  dbtag.maxqueuelen = 0;

  progstateSetCallback (dbtag.progstate, STATE_CONNECTING,
      dbtagConnectingCallback, &dbtag);
  progstateSetCallback (dbtag.progstate, STATE_WAIT_HANDSHAKE,
      dbtagHandshakeCallback, &dbtag);
  progstateSetCallback (dbtag.progstate, STATE_STOPPING,
      dbtagStoppingCallback, &dbtag);
  progstateSetCallback (dbtag.progstate, STATE_STOP_WAIT,
      dbtagStopWaitCallback, &dbtag);
  progstateSetCallback (dbtag.progstate, STATE_CLOSING,
      dbtagClosingCallback, &dbtag);

  procutilInitProcesses (dbtag.processes);
  dbtag.conn = connInit (ROUTE_DBTAG);

  listenPort = bdjvarsGetNum (BDJVL_DBTAG_PORT);
  sockhMainLoop (listenPort, dbtagProcessMsg, dbtagProcessing, &dbtag);
  connFree (dbtag.conn);
  progstateFree (dbtag.progstate);

  logProcEnd (LOG_PROC, "dbtag", "");
  logEnd ();
#if BDJ4_MEM_DEBUG
  mdebugReport ();
  mdebugCleanup ();
#endif
  return 0;
}

/* internal routines */

static int
dbtagProcessMsg (bdjmsgroute_t routefrom, bdjmsgroute_t route,
    bdjmsgmsg_t msg, char *args, void *udata)
{
  dbtag_t      *dbtag;

  dbtag = (dbtag_t *) udata;

  logMsg (LOG_DBG, LOG_MSGS, "got: from:%d/%s route:%d/%s msg:%d/%s args:%s",
      routefrom, msgRouteDebugText (routefrom),
      route, msgRouteDebugText (route), msg, msgDebugText (msg), args);

  switch (route) {
    case ROUTE_NONE:
    case ROUTE_DBTAG: {
      switch (msg) {
        case MSG_HANDSHAKE: {
          connProcessHandshake (dbtag->conn, routefrom);
          break;
        }
        case MSG_SOCKET_CLOSE: {
          connDisconnect (dbtag->conn, routefrom);
          break;
        }
        case MSG_EXIT_REQUEST: {
          logMsg (LOG_SESS, LOG_IMPORTANT, "got exit request");
          progstateShutdownProcess (dbtag->progstate);
          break;
        }
        case MSG_DB_FILE_CHK: {
          dbtagProcessFileMsg (dbtag, args);
          break;
        }
        case MSG_DB_ALL_FILES_SENT: {
          dbtag->havealldata = true;
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

  return 0;
}

static int
dbtagProcessing (void *udata)
{
  dbtag_t       * dbtag = udata;
  char          sbuff [BDJMSG_MAX];
  qidx_t        count;
  int           stop = false;


  if (! progstateIsRunning (dbtag->progstate)) {
    progstateProcess (dbtag->progstate);
    if (progstateCurrState (dbtag->progstate) == STATE_CLOSED) {
      stop = true;
    }
    if (gKillReceived) {
      progstateShutdownProcess (dbtag->progstate);
      logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
      gKillReceived = 0;
    }
    return stop;
  }

  connProcessUnconnected (dbtag->conn);

  for (int i = 0; i < dbtag->maxThreads; ++i) {
    if (dbtag->threads [i].state == DBTAG_T_STATE_HAVE_DATA) {
      pthread_join (dbtag->threads [i].thread, NULL);
      snprintf (sbuff, sizeof (sbuff), "%s%c%s",
          dbtag->threads [i].fn, MSG_ARGS_RS, (char *) dbtag->threads [i].data);
      ++dbtag->sent;
      connSendMessage (dbtag->conn, ROUTE_DBUPDATE, MSG_DB_FILE_TAGS, sbuff);
      dataFree (dbtag->threads [i].fn);
      dbtag->threads [i].fn = NULL;
      dataFree (dbtag->threads [i].data);
      dbtag->threads [i].data = NULL;
      dbtag->threads [i].state = DBTAG_T_STATE_INIT;
      --dbtag->numActiveThreads;
    }
  }

  if (dbtag->running == DBTAG_STATE_RUNNING) {
    count = queueGetCount (dbtag->fileQueue);
    if (count > dbtag->maxqueuelen) {
      dbtag->maxqueuelen = count;
    }
    while (queueGetCount (dbtag->fileQueue) > 0 &&
        dbtag->numActiveThreads < dbtag->maxThreads) {
      ++dbtag->iterations;
      if (dbtag->iterations > dbtag->maxThreads) {
        dbtag->threadActiveSum += dbtag->numActiveThreads;
      }
      for (int i = 0; i < dbtag->maxThreads; ++i) {
        if (dbtag->threads [i].state == DBTAG_T_STATE_INIT) {
          char    *fn;

          fn = queuePop (dbtag->fileQueue);
          if (fn == NULL) {
            break;
          }

          dbtag->threads [i].state = DBTAG_T_STATE_ACTIVE;
          ++dbtag->numActiveThreads;
          dataFree (dbtag->threads [i].fn);
          /* fn is already allocated */
          dbtag->threads [i].fn = fn;
          logMsg (LOG_DBG, LOG_DBUPDATE, "process: %s", fn);
          ++gcount;
          dbtag->threads [i].count = gcount;
          pthread_create (&dbtag->threads [i].thread, NULL, dbtagProcessFile, &dbtag->threads [i]);
        }
      }
    }

    if (queueGetCount (dbtag->fileQueue) == 0 &&
        dbtag->numActiveThreads == 0 &&
        dbtag->havealldata) {
      connSendMessage (dbtag->conn, ROUTE_DBUPDATE, MSG_DB_TAG_FINISHED, NULL);
      logMsg (LOG_DBG, LOG_IMPORTANT, "-- queue empty: %" PRId64 " ms",
          (int64_t) mstimeend (&dbtag->starttm));
      logMsg (LOG_DBG, LOG_IMPORTANT, "     received: %d", dbtag->received);
      logMsg (LOG_DBG, LOG_IMPORTANT, "         sent: %d", dbtag->sent);
      logMsg (LOG_DBG, LOG_IMPORTANT, "max queue len: %d", dbtag->maxqueuelen);
      logMsg (LOG_DBG, LOG_IMPORTANT, "average num threads active: %.2f",
          (double) dbtag->threadActiveSum /
          (double) (dbtag->iterations - dbtag->maxThreads));
      /* windows had some issues with the socket buffering, so wait a bit */
      /* before exiting. */
      mstimeset (&dbtag->waitCheck, 2000);
      dbtag->running = DBTAG_STATE_WAIT;
    }
  }

  if (dbtag->running == DBTAG_STATE_WAIT) {
    if (mstimeCheck (&dbtag->waitCheck)) {
      dbtag->running = DBTAG_STATE_FINISH;
      progstateShutdownProcess (dbtag->progstate);
    }
  }

  if (gKillReceived) {
    progstateShutdownProcess (dbtag->progstate);
    logMsg (LOG_SESS, LOG_IMPORTANT, "got kill signal");
    gKillReceived = 0;
  }
  return stop;
}

static bool
dbtagConnectingCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t *dbtag = tdbtag;
  bool    rc = STATE_NOT_FINISH;

  logProcBegin (LOG_PROC, "dbtagConnectingCallback");

  connProcessUnconnected (dbtag->conn);

  if (! connIsConnected (dbtag->conn, ROUTE_DBUPDATE)) {
    connConnect (dbtag->conn, ROUTE_DBUPDATE);
  }

  if (connIsConnected (dbtag->conn, ROUTE_DBUPDATE)) {
    rc = STATE_FINISHED;
  }

  logProcEnd (LOG_PROC, "dbtagConnectingCallback", "");
  return rc;
}

static bool
dbtagHandshakeCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t *dbtag = tdbtag;
  bool    rc = STATE_NOT_FINISH;

  logProcBegin (LOG_PROC, "dbtagHandshakeCallback");

  connProcessUnconnected (dbtag->conn);

  if (connHaveHandshake (dbtag->conn, ROUTE_DBUPDATE)) {
    rc = STATE_FINISHED;
  }
  logProcEnd (LOG_PROC, "dbtagHandshakeCallback", "");
  return rc;
}

static bool
dbtagStoppingCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t    *dbtag = tdbtag;

  logProcBegin (LOG_PROC, "dbtagStoppingCallback");
  connDisconnect (dbtag->conn, ROUTE_DBUPDATE);
  logProcEnd (LOG_PROC, "dbtagStoppingCallback", "");
  return STATE_FINISHED;
}

static bool
dbtagStopWaitCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t   *dbtag = tdbtag;
  bool        rc;

  rc = connWaitClosed (dbtag->conn, &dbtag->stopwaitcount);
  return rc;
}

static bool
dbtagClosingCallback (void *tdbtag, programstate_t programState)
{
  dbtag_t    *dbtag = tdbtag;

  logProcBegin (LOG_PROC, "dbtagClosingCallback");
  bdj4shutdown (ROUTE_DBTAG, NULL);

  mdfree (dbtag->threads);
  queueFree (dbtag->fileQueue);

  logProcEnd (LOG_PROC, "dbtagClosingCallback", "");
  return STATE_FINISHED;
}

static void
dbtagProcessFileMsg (dbtag_t *dbtag, char *args)
{
  if (dbtag->running == DBTAG_STATE_NOT_RUNNING) {
    dbtag->running = DBTAG_STATE_RUNNING;
    mstimestart (&dbtag->starttm);
  }
  ++dbtag->received;
  queuePush (dbtag->fileQueue, mdstrdup (args));
}

static void *
dbtagProcessFile (void *tdbthread)
{
  dbthread_t    * dbthread = tdbthread;


  dbthread->data = audiotagReadTags (dbthread->fn);
  dbthread->state = DBTAG_T_STATE_HAVE_DATA;
  pthread_exit (NULL);
  return NULL;
}

static void
dbtagSigHandler (int sig)
{
  gKillReceived = 1;
}
