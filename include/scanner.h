#ifndef __SCANNER_H__
#define __SCANNER_H__

#include "demod.h"
#include "rtl.h"

typedef enum { SCANNER_OFF, SCANNER_ON, SCANNER_SEEK_UP,
	       SCANNER_SEEK_DOWN } scanner_mode;

typedef struct scanner_s * scanner;

scanner scanner_create();
void scanner_destroy(scanner scan);
void scanner_execute(scanner scan, demod dem, rtl r);

int scanner_get_last_station_found(scanner scan);

scanner_mode scanner_get_mode(scanner scan);
void scanner_set_mode(scanner scan, scanner_mode mode);

#endif
