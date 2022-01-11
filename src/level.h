#ifndef INC_LEVEL_H
#define INC_LEVEL_H

#include "datafile.h"

typedef enum {
  LEVEL_LABEL,
  LEVEL_DEFAULT_FLAG,
  LEVEL_WEIGHT,
  LEVEL_KEY_MAX
} levelkey_t;

typedef struct {
  datafile_t        *df;
  list_t            *level;
} level_t;

level_t     *levelAlloc (char *);
void        levelFree (level_t *);
long        levelGetWeight (level_t *level, long idx);
void        levelConv (char *keydata, datafileret_t *ret);

#endif /* INC_LEVEL_H */
