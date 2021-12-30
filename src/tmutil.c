#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#if _hdr_unistd
# include <unistd.h>
#endif

#include "tmutil.h"
#include "bdjstring.h"

void
msleep (size_t mt)
{
  size_t t = mt * 1000;
  usleep ((useconds_t) t);
}

void
mtimestart (mtime_t *t)
{
  gettimeofday (&t->start, NULL);
}

size_t
mtimeend (mtime_t *t)
{
  struct timeval    end;

  gettimeofday (&end, NULL);
  long s = end.tv_sec - t->start.tv_sec;
  long u = end.tv_usec - t->start.tv_usec;
  long m = s * 1000 + u / 1000;
  return (size_t) m;
}

char *
dstamp (char *buff, size_t max)
{
  struct timeval    curr;
  struct tm         *tp;
  char              tbuff [20];

  gettimeofday (&curr, NULL);
  time_t s = curr.tv_sec;
  long u = curr.tv_usec;
  long m = u / 1000;
#if _lib_localtime_r
  struct tm         t;
  localtime_r (&s, &t);
  tp = &t;
#else
  tp = localtime (&s);
#endif
  strftime (buff, max, "%Y-%m-%d", tp);
  return buff;
}

char *
tstamp (char *buff, size_t max)
{
  struct timeval    curr;
  struct tm         *tp;
  char              tbuff [20];

  gettimeofday (&curr, NULL);
  time_t s = curr.tv_sec;
  long u = curr.tv_usec;
  long m = u / 1000;
#if _lib_localtime_r
  struct tm         t;
  localtime_r (&s, &t);
  tp = &t;
#else
  tp = localtime (&s);
#endif
  strftime (buff, max, "%H:%M:%S", tp);
  snprintf (tbuff, sizeof (tbuff), ".%03ld", m);
  strlcat (buff, tbuff, max);
  return buff;
}
