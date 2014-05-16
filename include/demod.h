#ifndef __DEMOD_H__
#define __DEMOD_H__

#include <pthread.h>
#include <rtl-sdr.h>
#include <time.h>

#include "rtl.h"

typedef enum { DEMOD_NONE, DEMOD_FM, DEMOD_AM } demod_mode;

typedef struct demod_s * demod;

demod demod_create();
void demod_destroy(demod dem);
void demod_execute(demod dem);
void demod_exit(demod dem);

const char * demod_lookup_mode_name(demod_mode mode);
demod_mode demod_lookup_mode(char * s);

int demod_lookup_frequency_step(demod_mode mode);

int demod_get_decim_factor(demod dem);
int demod_get_frequency_step(demod dem);
demod_mode demod_get_mode(demod dem);
const char * demod_get_mode_name(demod dem);
void demod_set_mode(demod dem, demod_mode mode);
uint32_t demod_get_center_freq(demod dem);
void demod_set_center_freq(demod dem, uint32_t center_freq);
int demod_get_input_rate(demod dem);
void demod_set_input_rate(demod dem, int input_rate);
int demod_get_output_rate(demod dem);
void demod_set_output_rate(demod dem, int output_rate);

float demod_get_snr(demod dem);
float demod_get_throughput(demod dem);

void demod_pop_and_lock(demod dem,
			int16_t ** buf,
			int * len);
void demod_push(demod dem, int8_t * buf, int len);
void demod_release(demod dem);

#endif
