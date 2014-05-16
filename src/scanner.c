#include <liquid/liquid.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "macros.h"
#include "scanner.h"

#define THRESHOLD_AM 3.0f
#define THRESHOLD_FM 8.0f /* dB */

struct scanner_common_s
{
  scanner_mode mode;
  int fc;

  // frequency of the last station found
  int last_station_found;

  // last execute
  struct timespec tune_time;
};

struct scanner_s
{
  struct scanner_common_s common;
  pthread_mutex_t common_m;
};

scanner scanner_create()
{
  scanner scan = (scanner) malloc(sizeof(struct scanner_s));
  
  scan->common.mode = SCANNER_OFF;
  scan->common.fc = -1;
  scan->common.last_station_found = -1;
  scan->common.tune_time.tv_sec = (time_t) 0;
  scan->common.tune_time.tv_nsec = 0;

  pthread_mutex_init( & scan->common_m, NULL);

  return scan;
}

void scanner_destroy(scanner scan)
{
  pthread_mutex_destroy( & scan->common_m);

  free(scan);
}

void scanner_execute(scanner scan, demod dem, rtl r)
{
  scanner_mode smode = scanner_get_mode(scan);
  if (smode == SCANNER_OFF) return;

  struct timespec time;
  float dt;

  clock_gettime(CLOCK_REALTIME_COARSE, & time);

  demod_mode dmode = demod_get_mode(dem);

  bool just_started = scan->common.fc < 0;
  
  int fc = demod_get_center_freq(dem);
  int fstep = demod_get_frequency_step(dem);
  int foffset = dmode == DEMOD_AM ? (int) 125e6 : 0;
  
  // max and min freqs in scannable range for modulation type (these should
  // really be defined somewhere)
  int fmax = dmode == DEMOD_AM ? 1700e3 : 107.9e6;
  int fmin = dmode == DEMOD_AM ? 540e3 : 87.9e6;

  float thresh = dmode == DEMOD_AM ? THRESHOLD_AM : THRESHOLD_FM;

  // last computed signal to noise ratio
  float snr = demod_get_snr(dem);

  bool wrapped = false;

  int fc_next;

  // ugh
  struct timespec * tune_time = & scan->common.tune_time;
  
  pthread_mutex_lock( & scan->common_m);

  dt = (time.tv_sec - tune_time->tv_sec);
  dt += (time.tv_nsec - tune_time->tv_nsec) / 1e9;

  // starting frequency
  if (just_started) { scan->common.fc = fc; }

  int fc_initial = scan->common.fc; 

  // wait a tick for the SNR measurement to settle
  if ( ! just_started && dt >= 0.5f) {

    // mark time
    scan->common.tune_time.tv_sec = time.tv_sec;
    scan->common.tune_time.tv_nsec = time.tv_nsec;
    
    switch (smode) {
    case SCANNER_ON:
      fc_next = fc + fstep;
      if (fc_next > fmax) { wrapped = true; fc_next = fmin; }
      
      if (snr >= thresh) {
	scan->common.last_station_found = fc;
      }
    
      // done scanning
      if ((wrapped || fc < fc_initial) && fc_initial <= fc_next) {
	scan->common.mode = SCANNER_OFF;
      }

      demod_set_center_freq(dem, fc_next);
      rtl_set_center_freq(r, fc_next + foffset);
    
      break;
    
    case SCANNER_SEEK_UP:
      fc_next = fc + fstep;
      if (fc_next > fmax) { wrapped = true; fc_next = fmin; }

      if (snr >= thresh && (fc != fc_initial || wrapped)) {
	scan->common.last_station_found = fc;
	scan->common.mode = SCANNER_OFF;
      }
      else {
	demod_set_center_freq(dem, fc_next);
	rtl_set_center_freq(r, fc_next + foffset);
      }
      
      break;
    
    case SCANNER_SEEK_DOWN:
      fc_next = fc - fstep;
      if (fc_next < fmin) { wrapped = true; fc_next = fmax; }
      
      if (snr >= thresh && (fc != fc_initial || wrapped)) {
	scan->common.last_station_found = fc;
	scan->common.mode = SCANNER_OFF;
      }
      else {
	demod_set_center_freq(dem, fc_next);
	rtl_set_center_freq(r, fc_next + foffset);
      }
      
      break;
    
    default: break;
    }
  }
  
  pthread_mutex_unlock( & scan->common_m);
}

static const char * _scanner_mode_names[] = {
  [SCANNER_ON] = "on",
  [SCANNER_OFF] = "off",
  [SCANNER_SEEK_UP] = "up",
  [SCANNER_SEEK_DOWN] = "down"
};

int scanner_get_last_station_found(scanner scan)
{
  int station;
  pthread_mutex_lock( & scan->common_m);
  station = scan->common.last_station_found;
  pthread_mutex_unlock( & scan->common_m);
  return station;
}

scanner_mode scanner_get_mode(scanner scan)
{
  scanner_mode mode;
  pthread_mutex_lock( & scan->common_m);
  mode = scan->common.mode;
  pthread_mutex_unlock( & scan->common_m);
  return mode;
}

void scanner_set_mode(scanner scan, scanner_mode mode)
{
  DEBUG("Scanner set to %s\n", _scanner_mode_names[mode]);
	
  pthread_mutex_lock( & scan->common_m);
  
  scan->common.mode = mode;
  scan->common.fc = -1;
  scan->common.last_station_found = -1;
  
  pthread_mutex_unlock( & scan->common_m);
}
