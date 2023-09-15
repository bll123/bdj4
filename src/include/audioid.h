#ifndef INC_AUDIOID_H
#define INC_AUDIOID_H

#include "ilist.h"
#include "nlist.h"
#include "song.h"

typedef struct audioid audioid_t;
typedef struct audioidmb audioidmb_t;
typedef struct audioidacoustid audioidacoustid_t;
typedef struct audioidacr audioidacr_t;

/* audioid.c */

audioid_t *audioidInit (void);
void audioidFree (audioid_t *audioid);
nlist_t *audioidLookup (audioid_t *audioid, const song_t *song);

/* musicbrainz.c */

audioidmb_t *mbInit (void);
void mbFree (audioidmb_t *mb);
ilist_t *mbRecordingIdLookup (audioidmb_t *mb, const char *recid);

/* acoustid.c */

audioidacoustid_t * acoustidInit (void);
void acoustidFree (audioidacoustid_t *acoustid);
nlist_t * acoustidLookup (audioidacoustid_t *acoustid, song_t *song);

/* acrcloud.c */

audioidacr_t * acrInit (void);
void acrFree (audioidacr_t *acr);
nlist_t * acrLookup (audioidacr_t *acr, song_t *song);


#endif /* INC_AUDIOID_H */
