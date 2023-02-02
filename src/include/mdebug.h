/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MDEBUG_H
#define INC_MDEBUG_H

#if defined (BDJ4_MEM_DEBUG)
# define mdfree(d)        mdfree_r (d, __FILE__, __LINE__)
# define mdextfree(d)     mdextfree_r (d, __FILE__, __LINE__)
# define mdmalloc(sz)     mdmalloc_r (sz, __FILE__, __LINE__)
# define mdrealloc(d,sz)  mdrealloc_r (d, sz, __FILE__, __LINE__)
# define mdstrdup(s)      mdstrdup_r (s, __FILE__, __LINE__)
# define mdextalloc(d)    mdextalloc_r (d, __FILE__, __LINE__)
# define dataFree(d)      mddataFree_r (d, __FILE__, __LINE__)
#else
# define mdfree free
# define mdextfree(d)
# define mdmalloc malloc
# define mdrealloc realloc
# define mdstrdup strdup
# define mdextalloc(d)
# define dataFree dataFree_r
#endif

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

void mdfree_r (void *data, const char *fn, int lineno);
void mdextfree_r (void *data, const char *fn, int lineno);
void * mdmalloc_r (size_t sz, const char *fn, int lineno);
void * mdrealloc_r (void *data, size_t sz, const char *fn, int lineno);
char * mdstrdup_r (const char *s, const char *fn, int lineno);
void * mdextalloc_r (void *data, const char *fn, int lineno);
void dataFree_r (void *data);
void mddataFree_r (void *data, const char *fn, int lineno);
void mdebugReport (void);
void mdebugInit (const char *tag);
void mdebugCleanup (void);
long mdebugCount (void);
long mdebugErrors (void);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MDEBUG_H */
