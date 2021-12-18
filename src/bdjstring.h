#ifndef _INC_BDJSTRING
#define _INC_BDJSTRING

#define _(str) gettext(str)

int       stringCompare (void *, void *);
int       istringCompare (void *, void *);

#if 0

typedef struct {
  char      *str;
  size_t    len;
  size_t    bytelen;
  size_t    alloclen;
  int       isstatic;
} bdjstring_t;

bdjstring_t *stringAlloc (const char *, const int);
void stringFree (bdjstring_t *str);
size_t stringLen (bdjstring_t *str);

#define STR(str) (str->str)
#define STRLEN(str) (stringLen (str))

#endif

#endif /* _INC_BDJSTRING */
