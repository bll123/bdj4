#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <math.h>

#include "bdj4.h"
#include "autosel.h"
#include "bdjvarsdf.h"
#include "dance.h"
#include "dancesel.h"
#include "ilist.h"
#include "log.h"
#include "nlist.h"
#include "osrandom.h"
#include "playlist.h"
#include "queue.h"
#include "slist.h"

typedef struct {
  ilistidx_t    danceIdx;
} playedDance_t;

enum {
  DANCESEL_DEBUG = 0,
};

typedef struct dancesel {
  dance_t       *dances;
  autosel_t     *autosel;
  danceselQueueLookup_t queueLookupProc;
  void          *userdata;
  /* probability tables */
  nlist_t       *base;
  double        basetotal;
  nlist_t       *distance;
  ssize_t       maxDistance;
  nlist_t       *selectedCounts;
  double        totalSelCount;
  queue_t       *playedDances;
  nlist_t       *adjustBase;
  ssize_t       selCount;
  nlist_t       *danceProbTable;
  /* autosel variables that will be used */
  ssize_t       histDistance;
  double        tagAdjust;
  double        tagMatch;
  double        priorVar;
  double        priorMult;
  double        expectedVar;
  double        expectedMult;
} dancesel_t;

static bool     danceselProcessClose (dancesel_t *dancesel,
                    ilistidx_t didx, ilistidx_t hdidx, ilistidx_t dist,
                    slist_t *tags);
static bool     danceselProcessFar (dancesel_t *dancesel,
                    ilistidx_t didx, ilistidx_t hdidx, ilistidx_t dist);
static bool     matchTag (slist_t *tags, slist_t *otags);
static bool     danceSelGetPriorInfo (dancesel_t *dancesel,
                    ilistidx_t queueCount, ilistidx_t prioridx,
                    ilistidx_t *pddanceIdx);

/* the countlist should contain a danceIdx/count pair */
dancesel_t *
danceselAlloc (nlist_t *countList,
    danceselQueueLookup_t queueLookupProc, void *userdata)
{
  dancesel_t  *dancesel;
  ilistidx_t  didx;
  nlistidx_t  iteridx;


  if (countList == NULL) {
    return NULL;
  }

  logProcBegin (LOG_PROC, "danceselAlloc");
  dancesel = malloc (sizeof (dancesel_t));
  assert (dancesel != NULL);
  dancesel->dances = bdjvarsdfGet (BDJVDF_DANCES);
  dancesel->autosel = bdjvarsdfGet (BDJVDF_AUTO_SEL);
  dancesel->base = nlistAlloc ("dancesel-base", LIST_ORDERED, NULL);
  dancesel->basetotal = 0.0;
  dancesel->distance = nlistAlloc ("dancesel-dist", LIST_ORDERED, NULL);
  dancesel->maxDistance = 0.0;
  dancesel->selectedCounts = nlistAlloc ("dancesel-sel-count", LIST_ORDERED, NULL);
  dancesel->playedDances = queueAlloc ("played-dances", free);
  dancesel->totalSelCount = 0.0;
  dancesel->adjustBase = NULL;
  dancesel->danceProbTable = NULL;
  dancesel->selCount = 0;
  dancesel->queueLookupProc = queueLookupProc;
  dancesel->userdata = userdata;

  /* autosel variables that will be used */
  dancesel->histDistance = autoselGetNum (dancesel->autosel, AUTOSEL_HIST_DISTANCE);
  dancesel->tagMatch = autoselGetDouble (dancesel->autosel, AUTOSEL_TAGMATCH);
  dancesel->tagAdjust = autoselGetDouble (dancesel->autosel, AUTOSEL_TAGADJUST);
  dancesel->priorVar = autoselGetDouble (dancesel->autosel, AUTOSEL_PRIOR_VAR);
  dancesel->priorMult = autoselGetDouble (dancesel->autosel, AUTOSEL_PRIOR_MULT);
  dancesel->expectedVar = autoselGetDouble (dancesel->autosel, AUTOSEL_EXPECTED_VAR);
  dancesel->expectedMult = autoselGetDouble (dancesel->autosel, AUTOSEL_EXPECTED_MULT);

  nlistStartIterator (countList, &iteridx);
  while ((didx = nlistIterateKey (countList, &iteridx)) >= 0) {
    long    count;

    count = nlistGetNum (countList, didx);
    if (count <= 0) {
      continue;
    }
    nlistSetNum (dancesel->base, didx, count);
    dancesel->basetotal += (double) count;
    logMsg (LOG_DBG, LOG_DANCESEL, "base: %d/%s: %ld", didx,
        danceGetStr (dancesel->dances, didx, DANCE_DANCE), count);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "base: %d/%s: %ld\n", didx,
          danceGetStr (dancesel->dances, didx, DANCE_DANCE), count);
    }
    nlistSetNum (dancesel->selectedCounts, didx, 0);
  }

  nlistStartIterator (dancesel->base, &iteridx);
  while ((didx = nlistIterateKey (dancesel->base, &iteridx)) >= 0) {
    double  dcount, dtemp;

    dcount = (double) nlistGetNum (dancesel->base, didx);
    dtemp = floor (dancesel->basetotal / dcount);
    nlistSetDouble (dancesel->distance, didx, dtemp);
    logMsg (LOG_DBG, LOG_DANCESEL, "dist: %d/%s: %.2f", didx,
        danceGetStr (dancesel->dances, didx, DANCE_DANCE), dtemp);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "dist: %d/%s: %.2f\n", didx,
          danceGetStr (dancesel->dances, didx, DANCE_DANCE), dtemp);
    }
    dancesel->maxDistance = (ssize_t)
        (dtemp > dancesel->maxDistance ? dtemp : dancesel->maxDistance);
  }
  dancesel->maxDistance = (ssize_t) round (dancesel->maxDistance);

  logProcEnd (LOG_PROC, "danceselAlloc", "");
  return dancesel;
}

void
danceselFree (dancesel_t *dancesel)
{
  logProcBegin (LOG_PROC, "danceselFree");
  if (dancesel != NULL) {
    nlistFree (dancesel->base);
    nlistFree (dancesel->distance);
    nlistFree (dancesel->selectedCounts);
    if (dancesel->playedDances != NULL) {
      queueFree (dancesel->playedDances);
    }
    nlistFree (dancesel->adjustBase);
    nlistFree (dancesel->danceProbTable);
    free (dancesel);
  }
  logProcEnd (LOG_PROC, "danceselFree", "");
}

/* used by main for mix */
void
danceselDecrementBase (dancesel_t *dancesel, ilistidx_t danceIdx)
{
  if (dancesel == NULL || dancesel->playedDances == NULL) {
    return;
  }

  nlistDecrement (dancesel->base, danceIdx);
}

void
danceselAddCount (dancesel_t *dancesel, ilistidx_t danceIdx)
{
  if (dancesel == NULL || dancesel->selectedCounts == NULL) {
    return;
  }

  nlistIncrement (dancesel->selectedCounts, danceIdx);
  ++dancesel->totalSelCount;
  ++dancesel->selCount;
}

/* played history queue */
/* this is only relevant if there is nothing queued by the caller */
void
danceselAddPlayed (dancesel_t *dancesel, ilistidx_t danceIdx)
{
  playedDance_t *pd = NULL;

  if (dancesel == NULL || dancesel->playedDances == NULL) {
    return;
  }

  logProcBegin (LOG_PROC, "danceselAddPlayed");
  pd = malloc (sizeof (playedDance_t));
  pd->danceIdx = danceIdx;
  queuePushHead (dancesel->playedDances, pd);
  logProcEnd (LOG_PROC, "danceselAddPlayed", "");
}

/* after selecting a dance, call danceselAddCount() if the dance will */
/* be used as a selection */
ilistidx_t
danceselSelect (dancesel_t *dancesel, ilistidx_t queueCount)
{
  ilistidx_t    didx;
  double        tbase;
  double        tprob;
  double        abase;
  nlistidx_t    iteridx;
  /* current dance values */
  slist_t       *tags = NULL;
  int           speed;
  int           type;
  /* previous dance data */
  ilistidx_t    pddanceIdx;
  slist_t       *pdtags = NULL;
  int           pdspeed = 0;
  int           pdtype = 0;
  /* queued dances */
  ilistidx_t    queueIdx;
  ilistidx_t    priordidx;        // prior dance index
  ilistidx_t    queueDist;
  bool          priorLookupDone;
  /* probability calculation */
  double        adjTotal;
  double        tval;


  logProcBegin (LOG_PROC, "danceselSelect");
  nlistFree (dancesel->adjustBase);
  nlistFree (dancesel->danceProbTable);
  dancesel->adjustBase = nlistAlloc ("dancesel-adjust-base", LIST_ORDERED, NULL);
  dancesel->danceProbTable = nlistAlloc ("dancesel-prob-table", LIST_ORDERED, NULL);

  /* data from the previous dance */

  danceSelGetPriorInfo (dancesel, queueCount, queueCount - 1, &pddanceIdx);

  if (pddanceIdx >= 0) {
    logMsg (LOG_DBG, LOG_DANCESEL, "found previous dance %d/%s", pddanceIdx,
        danceGetStr (dancesel->dances, pddanceIdx, DANCE_DANCE));
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "  get previous dance: %d/%s\n", pddanceIdx,
          danceGetStr (dancesel->dances, pddanceIdx, DANCE_DANCE));
    }
    pdspeed = danceGetNum (dancesel->dances, pddanceIdx, DANCE_SPEED);
    pdtags = danceGetList (dancesel->dances, pddanceIdx, DANCE_TAGS);
    pdtype = danceGetNum (dancesel->dances, pddanceIdx, DANCE_TYPE);
  } else {
    logMsg (LOG_DBG, LOG_DANCESEL, "no previous dance");
  }

  nlistStartIterator (dancesel->base, &iteridx);
  while ((didx = nlistIterateKey (dancesel->base, &iteridx)) >= 0) {
    tbase = nlistGetNum (dancesel->base, didx);
    tprob = tbase / dancesel->basetotal;
    abase = tbase;
    logMsg (LOG_DBG, LOG_DANCESEL, "didx:%d/%s base:%.2f tprob:%.6f",
        didx, danceGetStr (dancesel->dances, didx, DANCE_DANCE),
        tbase, tprob);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "didx:%d/%s base:%.2f tprob:%.6f\n",
        didx, danceGetStr (dancesel->dances, didx, DANCE_DANCE),
        tbase, tprob);
    }

    speed = danceGetNum (dancesel->dances, didx, DANCE_SPEED);

    /* if this selection is at the beginning of the playlist */
    if (dancesel->selCount < autoselGetNum (dancesel->autosel, AUTOSEL_BEG_COUNT)) {

      /* if this dance is a fast dance ( / 1000) */
      if (speed == DANCE_SPEED_FAST) {
        abase = abase / autoselGetDouble (dancesel->autosel, AUTOSEL_BEG_FAST);
        logMsg (LOG_DBG, LOG_DANCESEL, "  fast / begin of playlist: abase: %.6f", abase);
        if (DANCESEL_DEBUG) {
          fprintf (stderr, "  fast / begin of playlist: abase: %.6f\n", abase);
          fprintf (stderr, "    selcount: %ld\n", (long) dancesel->selCount);
        }
      }
    }

    /* if this dance and the previous dance were both fast ( / 1000) */
    if (pddanceIdx >= 0 &&
        pdspeed == speed && speed == DANCE_SPEED_FAST) {
      abase = abase / autoselGetDouble (dancesel->autosel, AUTOSEL_BOTHFAST);
      logMsg (LOG_DBG, LOG_DANCESEL, "  speed is fast and same as previous: abase: %.6f", abase);
      if (DANCESEL_DEBUG) {
        fprintf (stderr, "  speed is fast and same as previous: abase: %.6f\n", abase);
      }
    }

    /* if there is a tag match between the previous dance and this one */
    /* ( / 600 ) */
    tags = danceGetList (dancesel->dances, didx, DANCE_TAGS);
    if (pddanceIdx >= 0 && matchTag (tags, pdtags)) {
      abase = abase / dancesel->tagMatch;
      logMsg (LOG_DBG, LOG_DANCESEL, "  matched tags with previous: abase: %.6f", abase);
      if (DANCESEL_DEBUG) {
        fprintf (stderr, "  matched tags with previous: abase: %.6f\n", abase);
      }
    }

    /* if this dance and the previous dance have matching types ( / 600 ) */
    type = danceGetNum (dancesel->dances, didx, DANCE_TYPE);
    if (pddanceIdx >= 0 && pdtype == type) {
      abase = abase / autoselGetDouble (dancesel->autosel, AUTOSEL_TYPE_MATCH);
      logMsg (LOG_DBG, LOG_DANCESEL, "  matched type with previous: abase: %.6f", abase);
      if (DANCESEL_DEBUG) {
        fprintf (stderr, "  matched type with previous: abase: %.6f\n", abase);
      }
    }

    nlistSetDouble (dancesel->adjustBase, didx, abase);

    queueIdx = queueCount - 1;
    priorLookupDone = false;
    queueDist = 0;
    priordidx = -1;
    while (! priorLookupDone && queueDist < dancesel->maxDistance) {
      priorLookupDone = danceSelGetPriorInfo (dancesel, queueCount, queueIdx, &priordidx);
      logMsg (LOG_DBG, LOG_DANCESEL, "  prior dist:%d found:%d", queueDist, didx == priordidx);
      if (DANCESEL_DEBUG) {
        fprintf (stderr, "  prior dist:%d found:%d\n", queueDist, didx == priordidx);
      }
      if (! priorLookupDone && didx == priordidx) {
        /* process close is only done for matching dance indexes */
        /* and if close processing is done, no more processing is necessary */
        if (danceselProcessClose (dancesel, didx, priordidx, queueDist, tags)) {
          priorLookupDone = true;
        } else {
          /* if no close processing and found, do the far processing */
          danceselProcessFar (dancesel, didx, priordidx, queueDist);
          priorLookupDone = true;
        }
      }
      --queueIdx;
      ++queueDist;
    } /* for prior queue/played entries */

    if (priordidx >= 0 && didx != priordidx) {
      /* if the dance was not found, do the far processing */
      danceselProcessFar (dancesel, didx, priordidx, queueDist);
    }
  } /* for each dance in the base list */

  /* get the totals for the adjusted base values */
  adjTotal = 0.0;
  nlistStartIterator (dancesel->adjustBase, &iteridx);
  while ((didx = nlistIterateKey (dancesel->adjustBase, &iteridx)) >= 0) {
    abase = nlistGetDouble (dancesel->adjustBase, didx);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "   pre-final:%d/%s %.6f\n", didx,
          danceGetStr (dancesel->dances, didx, DANCE_DANCE), abase);
    }
    adjTotal += abase;
  }

  /* create a probability table of running totals */
  tprob = 0.0;
  nlistStartIterator (dancesel->adjustBase, &iteridx);
  while ((didx = nlistIterateKey (dancesel->adjustBase, &iteridx)) >= 0) {
    abase = nlistGetDouble (dancesel->adjustBase, didx);
    tval = 0.0;
    if (adjTotal > 0.0) {
      tval = abase / adjTotal;
    }
    tprob += tval;
    logMsg (LOG_DBG, LOG_DANCESEL, "final prob: %d/%s: %.6f",
        didx, danceGetStr (dancesel->dances, didx, DANCE_DANCE), tprob);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "   final prob: %d/%s: %.6f\n",
          didx, danceGetStr (dancesel->dances, didx, DANCE_DANCE), tprob);
    }
    nlistSetDouble (dancesel->danceProbTable, didx, tprob);
  }

  tval = dRandom ();
  didx = nlistSearchProbTable (dancesel->danceProbTable, tval);
  logMsg (LOG_DBG, LOG_BASIC, "== select %.6f %d/%s",
        tval, didx, danceGetStr (dancesel->dances, didx, DANCE_DANCE));
  if (DANCESEL_DEBUG) {
    fprintf (stderr, "== select %.6f %d/%s\n",
        tval, didx, danceGetStr (dancesel->dances, didx, DANCE_DANCE));
  }

  logProcEnd (LOG_PROC, "danceselSelect", "");
  return didx;
}

/* internal routines */

static bool
danceselProcessClose (dancesel_t *dancesel, ilistidx_t didx,
    ilistidx_t priordidx, ilistidx_t priordist, slist_t *tags)
{
  slist_t       *priortags = NULL;
  double        expdist;
  double        dpriordist = (double) priordist;
  double        findex;
  double        abase;
  int           rc = false;

  logProcBegin (LOG_PROC, "danceselProcessClose");
  abase = nlistGetDouble (dancesel->adjustBase, didx);

  logMsg (LOG_DBG, LOG_DANCESEL, "process close didx:%d priordidx:%d/%s",
      didx, priordidx, danceGetStr (dancesel->dances, priordidx, DANCE_DANCE));

  /* the tags of the first few dances in the history are checked */
  /* the previous dance's tags have already been adjusted */
  if (priordist > 0 && priordist <= dancesel->histDistance) {
    priortags = danceGetList (dancesel->dances, priordidx, DANCE_TAGS);
    if (matchTag (tags, priortags)) {
      double    tmp;

      /* further distance, smaller value */
      tmp = pow (dancesel->tagAdjust, dancesel->histDistance);
      abase = abase / dancesel->tagMatch * tmp;
      nlistSetDouble (dancesel->adjustBase, didx, abase);
      logMsg (LOG_DBG, LOG_DANCESEL, "    matched prior tags: abase: %.6f", abase);
      if (DANCESEL_DEBUG) {
        fprintf (stderr, "    matched prior tags abase: %.6f\n", abase);
      }
    }
  }

  /* expected distance */
  expdist = nlistGetDouble (dancesel->distance, didx);

  /* for close distances, only adjust the first 3 */
  if (priordist < dancesel->histDistance) {
    /* f(x)=1.2^(-2.4x+1) */
    findex = pow (dancesel->priorVar, ((- dancesel->priorMult) * dpriordist));
    findex = fmax (0.0, findex);
    logMsg (LOG_DBG, LOG_DANCESEL, "      dist adjust: expdist:%.0f ddpriordist:%.0f findex:%.6f", expdist, dpriordist, findex);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "      dist adjust: expdist:%.0f dpriordist:%.0f findex:%.6f\n", expdist, dpriordist, findex);
    }

    logMsg (LOG_DBG, LOG_DANCESEL, "      abase: %.6f - (findex * abase): %.6f", abase, findex * abase);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "      abase: %.6f - (findex * abase): %.6f\n", abase, findex * abase);
    }
    abase = fmax (0.0, abase - (findex * abase));
    logMsg (LOG_DBG, LOG_DANCESEL, "    adjust due to distance: abase: %.6f", abase);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "    adjust due to close distance / %.6f\n", abase);
    }
    nlistSetDouble (dancesel->adjustBase, priordidx, abase);
    rc = true;
  }

  logProcEnd (LOG_PROC, "danceselProcessClose", "");
  return rc;
}


static bool
danceselProcessFar (dancesel_t *dancesel, ilistidx_t didx,
    ilistidx_t priordidx, ilistidx_t priordist)
{
  double        expdist;
  double        dpriordist = (double) priordist;
  double        findex;
  double        abase;

  logProcBegin (LOG_PROC, "danceselProcessFar");
  abase = nlistGetDouble (dancesel->adjustBase, didx);

  logMsg (LOG_DBG, LOG_DANCESEL, "process far didx:%d priordidx:%d/%s",
      didx, priordidx,
      danceGetStr (dancesel->dances, priordidx, DANCE_DANCE));

  /* expected distance */
  expdist = nlistGetDouble (dancesel->distance, didx);

  /* for far distances, adjust only for more than 2/3 of the expected distance */
  if (dpriordist > expdist * 2.0 / 3.0) {
    /* f(x)=1.15^(1.8x-12) */
    findex = pow (dancesel->expectedVar, (dancesel->expectedMult * dpriordist - expdist));
    findex = fmax (0.0, findex);
    logMsg (LOG_DBG, LOG_DANCESEL, "      dist adjust: expdist:%.0f ddpriordist:%.0f findex:%.6f", expdist, dpriordist, findex);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "      dist adjust: expdist:%.0f dpriordist:%.0f findex:%.6f\n", expdist, dpriordist, findex);
    }

    logMsg (LOG_DBG, LOG_DANCESEL, "      abase: %.6f + findex: %.6f", abase, findex);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "      abase: %.6f + findex: %.6f\n", abase, findex);
    }
    abase = fmax (0.0, abase + findex);
    logMsg (LOG_DBG, LOG_DANCESEL, "    adjust due to distance: abase: %.6f", abase);
    if (DANCESEL_DEBUG) {
      fprintf (stderr, "    adjust due to far distance / %.6f\n", abase);
    }
    nlistSetDouble (dancesel->adjustBase, priordidx, abase);
  }

  logProcEnd (LOG_PROC, "danceselProcessFar", "");
  return true;
}


static bool
matchTag (slist_t *tags, slist_t *otags)
{
  char        *ttag;
  char        *otag;
  slistidx_t  titeridx;
  slistidx_t  oiteridx;

  if (tags != NULL && otags != NULL) {
    slistStartIterator (tags, &titeridx);
    while ((ttag = slistIterateKey (tags, &titeridx)) != NULL) {
      slistStartIterator (otags, &oiteridx);
      while ((otag = slistIterateKey (otags, &oiteridx)) != NULL) {
        if (strcmp (ttag, otag) == 0) {
          return true;
        }
      } /* for each tag for the previous dance */
    } /* for each tag for this dance */
  } /* if both dances have tags */
  return false;
}

static bool
danceSelGetPriorInfo (dancesel_t *dancesel, ilistidx_t queueCount,
    ilistidx_t queueIdx, ilistidx_t *pddanceIdx)
{
  ilistidx_t    didx = -1;
  playedDance_t *pd = NULL;
  bool          rc = false;

  /* first try the lookup in the queue */

  didx = dancesel->queueLookupProc (dancesel->userdata, queueIdx);

  /* during a 'mix', there is an empty-head in position 0 */

  if (didx < 0 && queueIdx != 0) {
    ilistidx_t    count;

    queueIdx = queueIdx * -1 - 1;
    count = queueGetCount (dancesel->playedDances);
    if (count <= 0 || count < queueIdx) {
      /* no more data */
      rc = true;
    } else {
      pd = queueGetByIdx (dancesel->playedDances, queueIdx);
      if (pd != NULL) {
        didx = pd->danceIdx;
      }
    }
  }
  logMsg (LOG_DBG, LOG_DANCESEL, "  count: %d prior: %d rc: %d didx: %d", queueCount, queueIdx, rc, didx);
  if (DANCESEL_DEBUG) {
    fprintf (stderr, "  count: %d prior: %d rc: %d didx: %d\n", queueCount, queueIdx, rc, didx);
  }

  *pddanceIdx = didx;
  return rc;
}
