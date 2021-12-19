#include "bdjconfig.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "musicdb.h"
#include "rafile.h"
#include "song.h"
#include "list.h"
#include "bdjstring.h"
#include "lock.h"

/* globals */
int         initialized = 0;
db_t        *bdjdb = NULL;

void
dbOpen (char *fn)
{
  if (! initialized) {
    bdjdb = malloc (sizeof (db_t));
    assert (bdjdb != NULL);
    bdjdb->songs = vlistAlloc (LIST_UNORDERED, VALUE_DATA, istringCompare,
        free, songFree);
    dbLoad (bdjdb, fn);
    vlistSort (bdjdb->songs);
    initialized = 1;
  }
}

void
dbClose (void)
{
  /* for each song in db, free the song */
  vlistFree (bdjdb->songs);
  initialized = 0;
  bdjdb = NULL;
}

int
dbLoad (db_t *db, char *fn)
{
  char        data [RAFILE_REC_SIZE];
  char        *tokptr;
  char        *str;
  int         songDataCount;
  int         allocCount;
  char        **strdata;
  char        *fstr;
  song_t      *song;
  rafile_t    *radb;

  tokptr = NULL;
  allocCount = 60;
  strdata = malloc (sizeof (char *) * (size_t) allocCount);
  assert (strdata != NULL);

  fstr = "";
  radb = raOpen (fn, 10);
  vlistSetSize (db->songs, raGetCount (radb));

  for (size_t i = 1L; i <= radb->count; ++i) {
    raRead (radb, i, data);
    if (! *data) {
      continue;
    }

    str = strtok_r (data, "\n", &tokptr);
    songDataCount = 0;
    while (str != NULL) {
      if (songDataCount > allocCount) {
        allocCount += 10;
        strdata = realloc (strdata, sizeof (char *) * (size_t) allocCount);
      }
      if (songDataCount % 2 == 1) {
        /* value data is indented */
        str += 2;
      }
      strdata [songDataCount++] = str;
      if (songDataCount == 2) {
        /* save the filename */
        fstr = str;
      }
      str = strtok_r (NULL, "\n", &tokptr);
    }
    song = songAlloc ();
    songSetAll (song, strdata, songDataCount);
    songSetNumeric (song, "rrn", (long) i);
    vlistSetData (db->songs, strdup (fstr), song);
    ++db->count;
  }

  return 0;
}
