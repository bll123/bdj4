#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>
#include <stdarg.h>

#if _hdr_winsock2
# include <winsock2.h>
#endif
#if _hdr_windows
# include <windows.h>
#endif

#include "bdj4.h"
#include "bdjstring.h"
#include "osutils.h"
#include "tmutil.h"

int
osSetEnv (const char *name, const char *value)
{
  int     rc;

  /* setenv is better */
#if _lib_setenv
  rc = setenv (name, value, 1);
#else
  {
    char    tbuff [4096];
    snprintf (tbuff, sizeof (tbuff), "%s=%s", name, value);
    rc = putenv (tbuff);
  }
#endif
  return rc;
}

#if _lib_symlink

int
osCreateLink (const char *target, const char *linkpath)
{
  int rc;

  rc = symlink (target, linkpath);
  return rc;
}

#endif
