/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_RATING_H
#define INC_RATING_H

#include "datafile.h"
#include "ilist.h"

typedef enum {
  RATING_RATING,
  RATING_WEIGHT,
  RATING_KEY_MAX
} ratingkey_t;

typedef struct rating rating_t;

enum {
  RATING_UNRATED_IDX = 0,
};

rating_t    *ratingAlloc (void);
void        ratingFree (rating_t *ratings);
ssize_t     ratingGetCount (rating_t *ratings);
int         ratingGetMaxWidth (rating_t *ratings);
const char  * ratingGetRating (rating_t *ratings, ilistidx_t idx);
ssize_t     ratingGetWeight (rating_t *ratings, ilistidx_t idx);
void        ratingSetRating (rating_t *ratings, ilistidx_t ikey, const char *val);
void        ratingSetWeight (rating_t *ratings, ilistidx_t ikey, int val);
void        ratingDelete (rating_t *ratings, ilistidx_t ikey);
void        ratingStartIterator (rating_t *ratings, ilistidx_t *iteridx);
ilistidx_t  ratingIterate (rating_t *ratings, ilistidx_t *iteridx);
void        ratingConv (datafileconv_t *conv);
void        ratingSave (rating_t *ratings, ilist_t *list);

#endif /* INC_RATING_H */
