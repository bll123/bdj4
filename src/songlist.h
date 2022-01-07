#ifndef INC_SONGLIST_H
#define INC_SONGLIST_H

#include "datafile.h"

typedef enum {
  SONGLIST_FILE,
  SONGLIST_TITLE,
  SONGLIST_DANCE,
  SONGLIST_KEY_MAX
} songlistkey_t;

typedef struct {
  datafile_t      *df;
  list_t          *songlist;
  char            *fname;
} songlist_t;

songlist_t *  songlistAlloc (char *);
void          songlistFree (songlist_t *);
char          *songlistGetData (songlist_t *, long idx, long key);

#endif /* INC_SONGLIST_H */
