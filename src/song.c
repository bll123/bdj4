#include "bdjconfig.h"

#include <stdlib.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>

#include "song.h"
#include "list.h"
#include "bdjstring.h"

song_t *
songAlloc ()
{
  song_t    *song;

  song = malloc (sizeof (song_t));
  assert (song != NULL);
  song->songInfo = vlistAlloc (LIST_UNORDERED, VALUE_STR, istringCompare);
  return song;
}

void
songFree (song_t *song)
{
  if (song != NULL) {
    if (song->songInfo != NULL) {
      vlistFree (song->songInfo);
    }
    free (song);
  }
}

void
songSet (song_t *song, char *data[], int count)
{
  for (int i = 0; i < count; i += 2) {
    vlistAddStr (song->songInfo, data[i], data[i+1]);
  }
  vlistSort (song->songInfo);
}
