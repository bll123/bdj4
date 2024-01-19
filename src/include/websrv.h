/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_WEBSRV_H
#define INC_WEBSRV_H

#include "mongoose.h"

typedef struct {
  struct mg_mgr   mgr;
} websrv_t;

websrv_t *websrvInit (uint16_t listenPort, mg_event_handler_t eventHandler,
    void *userdata);
void websrvFree (websrv_t *websrv);
void websrvProcess (websrv_t *websrv);

#endif /* INC_WEBSRV_H */
