#ifndef __CONTROLLER_SOURCE_H__
#define __CONTROLLER_SOURCE_H__

#include "demod.h"
#include "rtl.h"
#include "scanner.h"
#include "websocket.h"

typedef struct controller_s * controller;

controller controller_create(demod dem, rtl r, scanner scan, websocket ws);
void controller_destroy(controller ctrl);
void controller_execute(controller ctrl);

void controller_command(controller ctrl, char * cmd, int len);

#endif
