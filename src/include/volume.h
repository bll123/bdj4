/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_VOLUME
#define INC_VOLUME

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

#include "dylib.h"

typedef struct volsinklist volsinklist_t;

typedef enum {
  VOL_GET,
  VOL_SET,
  VOL_HAVE_SINK_LIST,
  VOL_GETSINKLIST,
  VOL_SET_SYSTEM_DFLT,
} volaction_t;

typedef struct volume volume_t;

volume_t  *volumeInit (const char *volpkg);
void      volumeFree (volume_t *volume);
bool      volumeHaveSinkList (volume_t *volume);
void      volumeSinklistInit (volsinklist_t *sinklist);
int       volumeGet (volume_t *volume, const char *sinkname);
int       volumeSet (volume_t *volume, const char *sinkname, int vol);
int       volumeGetSinkList (volume_t *volume, const char *sinkname, volsinklist_t *sinklist);
void      volumeSetSystemDefault (volume_t *volume, const char *sinkname);
void      volumeFreeSinkList (volsinklist_t *sinklist);

int       volumeProcess (volaction_t action, const char *sinkname,
              int *vol, volsinklist_t *sinklist, void **udata);
void      volumeDisconnect (void);
void      volumeCleanup (void **udata);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_VOLUME */
