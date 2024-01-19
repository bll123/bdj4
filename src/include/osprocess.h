/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_OSPROCESS_H
#define INC_OSPROCESS_H

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

#include "config.h"

#include <sys/types.h>

enum {
  OS_PROC_NONE      = 0x0000,
  OS_PROC_DETACH    = 0x0001,
  OS_PROC_WAIT      = 0x0002,
  OS_PROC_NOSTDERR  = 0x0004,
  OS_PROC_WINDOW_OK = 0x0008,   // windows, default is no window
};

pid_t osProcessStart (const char *targv[], int flags, void **handle, char *outfname);
int osProcessPipe (const char *targv[], int flags, char *rbuff, size_t sz, size_t *retsz);
char *osRunProgram (const char *prog, ...);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_OSPROCESS_H */
