/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
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
#if _hdr_fcntl
# include <fcntl.h>
#endif
#if _sys_wait
# include <sys/wait.h>
#endif

#if _hdr_winsock2
# include <winsock2.h>
#endif
#if _hdr_windows
# include <windows.h>
#endif

#include "bdj4.h"
#include "bdjstring.h"
#include "mdebug.h"
#include "osprocess.h"
#include "tmutil.h"

#if _define_WIFEXITED

static int osProcessWaitStatus (int wstatus);

#endif

/* identical on linux and mac os */
#if _lib_fork

/* handles redirection to a file */
pid_t
osProcessStart (const char *targv[], int flags, void **handle, char *outfname)
{
  pid_t       pid;
  pid_t       tpid;
  int         rc;

  /* this may be slower, but it works; speed is not a major issue */
  tpid = fork ();
  if (tpid < 0) {
    return tpid;
  }

  if (tpid == 0) {
    /* child */
    if ((flags & OS_PROC_DETACH) == OS_PROC_DETACH) {
      setsid ();
    }

    if (outfname != NULL) {
      int fd = open (outfname, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU | S_IRWXG);
      if (fd < 0) {
        outfname = NULL;
      } else {
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);
        close (fd);
      }
    }

    rc = execv (targv [0], (char * const *) targv);
    if (rc < 0) {
      fprintf (stderr, "unable to execute %s %d %s\n", targv [0], errno, strerror (errno));
      exit (1);
    }

    exit (0);
  }

  pid = tpid;
  if ((flags & OS_PROC_WAIT) == OS_PROC_WAIT) {
    int   rc, wstatus;

    if (waitpid (pid, &wstatus, 0) < 0) {
      rc = 0;
      // fprintf (stderr, "waitpid: errno %d %s\n", errno, strerror (errno));
    } else {
      rc = osProcessWaitStatus (wstatus);
    }
    return rc;
  }

  return pid;
}

#endif /* if _lib_fork */

/* identical on linux and mac os */
#if _lib_fork

/* creates a pipe for re-direction and grabs the output */
int
osProcessPipe (const char *targv[], int flags, char *rbuff, size_t sz, size_t *retsz)
{
  pid_t   pid;
  int     rc = 0;
  pid_t   tpid;
  int     pipefd [2];

  if (rbuff != NULL) {
    *rbuff = '\0';
  }
  if (retsz != NULL) {
    *retsz = 0;
  }

  if (pipe (pipefd) < 0) {
    return -1;
  }

  /* this may be slower, but it works; speed is not a major issue */
  tpid = fork ();
  if (tpid < 0) {
    return tpid;
  }

  if (tpid == 0) {
    /* child */
    /* close any open file descriptors, but not the pipe write side */
    for (int i = 3; i < 30; ++i) {
      if (pipefd [1] == i) {
        continue;
      }
      close (i);
    }

    dup2 (pipefd [1], STDOUT_FILENO);
    dup2 (pipefd [1], STDERR_FILENO);

    rc = execv (targv [0], (char * const *) targv);
    if (rc < 0) {
      fprintf (stderr, "unable to execute %s %d %s\n", targv [0], errno, strerror (errno));
      exit (1);
    }

    exit (0);
  }

  /* write end of pipe is not needed by parent */
  close (pipefd [1]);

  pid = tpid;

  /* if there is no data return, can simply wait for the process to exit */
  if (rbuff == NULL &&
      (flags & OS_PROC_WAIT) == OS_PROC_WAIT) {
    int   rc, wstatus;

    if (waitpid (pid, &wstatus, 0) < 0) {
      rc = 0;
      // fprintf (stderr, "waitpid: errno %d %s\n", errno, strerror (errno));
    } else {
      rc = osProcessWaitStatus (wstatus);
    }
  }

  if (rbuff != NULL) {
    int     rc, wstatus;
    ssize_t bytesread = 0;

    rc = 1;
    rbuff [sz - 1] = '\0';

    while (1) {
      size_t    rbytes;

      rbytes = read (pipefd [0], rbuff + bytesread, sz - bytesread);
      bytesread += rbytes;
      if (bytesread < (ssize_t) sz) {
        rbuff [bytesread] = '\0';
      }
      if (retsz != NULL) {
        *retsz = bytesread;
      }
      if (rc == 0) {
        break;
      }
      if ((flags & OS_PROC_WAIT) == OS_PROC_WAIT) {
        mssleep (2);
        rc = waitpid (pid, &wstatus, WNOHANG);
        if (rc < 0) {
          rc = 0;
          // fprintf (stderr, "waitpid: errno %d %s\n", errno, strerror (errno));
        } else if (rc != 0) {
          rc = osProcessWaitStatus (wstatus);
        } else {
          /* force continuation */
          rc = 1;
        }
      }
    }
    close (pipefd [0]);
  } else {
    /* set up so the application can read from stdin */
    close (STDIN_FILENO);
    dup2 (pipefd [0], STDIN_FILENO);
  }

  return rc;
}

#endif /* if _lib_fork */

char *
osRunProgram (const char *prog, ...)
{
  char        data [4096];
  char        *arg;
  const char  *targv [10];
  int         targc;
  va_list     valist;

  va_start (valist, prog);

  targc = 0;
  targv [targc++] = prog;
  while ((arg = va_arg (valist, char *)) != NULL) {
    targv [targc++] = arg;
  }
  targv [targc++] = NULL;
  va_end (valist);

  osProcessPipe (targv, OS_PROC_WAIT | OS_PROC_DETACH, data, sizeof (data), NULL);
  return mdstrdup (data);
}

#if _define_WIFEXITED

static int
osProcessWaitStatus (int wstatus)
{
  int rc = 0;

  if (WIFEXITED (wstatus)) {
    rc = WEXITSTATUS (wstatus);
  } else if (WIFSIGNALED (wstatus)) {
    rc = WTERMSIG (wstatus);
  }
  return rc;
}

#endif
