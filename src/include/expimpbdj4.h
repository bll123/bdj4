#ifndef INC_EXPIMPBDJ4_H
#define INC_EXPIMPBDJ4_H

#include "musicdb.h"
#include "nlist.h"

enum {
  EIBDJ4_EXPORT,
  EIBDJ4_IMPORT,
};

typedef struct eibdj4 eibdj4_t;

eibdj4_t *eibdj4Init (musicdb_t *musicdb, const char *dirname, int eiflag);
void eibdj4Free (eibdj4_t *eibdj4);
void eibdj4SetDBIdxList (eibdj4_t *eibdj4, nlist_t *dbidxlist);
void eibdj4SetPlaylist (eibdj4_t *eibdj4, const char *name);
void eibdj4SetNewName (eibdj4_t *eibdj4, const char *name);
void eibdj4SetUpdate (eibdj4_t *eibdj4, bool updateflag);
void eibdj4GetCount (eibdj4_t *eibdj4, int *count, int *tot);
bool eibdj4Process (eibdj4_t *eibdj4);

#endif /* INC_EXPIMPBDJ4_H */