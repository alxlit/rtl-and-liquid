#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "controller.h"
#include "demod.h"
#include "macros.h"
#include "rtl.h"
#include "scanner.h"
#include "websocket.h"

struct controller_s
{
  demod dem;
  rtl r;
  scanner scan;
  websocket ws;
  int heartbeat_num_samples;
  struct timespec heartbeat_time;
};

static void _rtl_callback(int8_t * buf, int len, void * ctx)
{
  controller ctrl = (controller) ctx;
  
  if (ctrl->dem != NULL) {
    demod_push(ctrl->dem, buf, len); }
}

static void _websocket_receive_callback(void * buf, size_t len, void * ctx)
{
  controller ctrl = (controller) ctx;
  char * cmd = (char *) buf;
  
  // make sure it's terminated with a null byte
  cmd[(int) len] = '\0';

  // run it
  controller_command(ctrl, cmd, (int) len);
}

controller controller_create(demod dem, rtl r, scanner scan, websocket ws)
{
  controller ctrl = (controller) malloc(sizeof(struct controller_s));

  ctrl->dem = dem;
  ctrl->r = r;
  ctrl->scan = scan;
  ctrl->ws = ws;
  
  ctrl->heartbeat_num_samples = 0;
  
  ctrl->heartbeat_time.tv_sec = (time_t) 0;
  ctrl->heartbeat_time.tv_nsec = 0;
    
  // initialize the RTL dongle
  rtl_set_auto_gain(r);
  rtl_set_freq_correction(r, 100);
  rtl_set_center_freq(r, (uint32_t) 90.7e6);
  rtl_set_sample_rate(r, (uint32_t) 1e6);
  rtl_reset_buffer(r);
  
  demod_set_center_freq(dem, rtl_get_center_freq(ctrl->r));
  demod_set_input_rate(dem, rtl_get_sample_rate(ctrl->r));
  demod_set_output_rate(dem, (int) 48e3);
  demod_set_mode(dem, DEMOD_FM);

  // start demodulator
  demod_execute(dem);
  
  // start RTL
  rtl_execute(r, _rtl_callback, (void *) ctrl);

  // start websocket
  websocket_execute(ws, _websocket_receive_callback, (void *) ctrl);
  
  return ctrl;
}

void controller_command(controller ctrl, char * cmd, int len)
{
  DEBUG("Command: %s\n", cmd);

  int max_args = 16;
  int argc = 1;
  char * argv[max_args];

  argv[0] = NULL;
  
  char * token;
  token = strtok(cmd, " ");
  
  while (token != NULL && argc <= max_args) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }

  int opt;
  
  float fc = -1;
  int fs = -1;
  demod_mode current_dmode = demod_get_mode(ctrl->dem);
  demod_mode dmode = DEMOD_NONE;

  bool change_smode = false;
  scanner_mode smode = SCANNER_OFF;

  // reset getopt
  optind = 1;

  while ((opt = getopt(argc, argv, "f:m:r:s:")) != -1) {
    switch (opt) {
    case 'f':
      fc = (float) atoi(optarg);
      break;
    case 'r':
      fs = atoi(optarg);
      break;      
    case 'm':
      dmode = demod_lookup_mode(optarg);
      break;
    case 's':
      change_smode = true;
      
      if ( ! strcmp(optarg, "on")) {
	smode = SCANNER_ON;
      }
      else if ( ! strcmp(optarg, "off")) {
	smode = SCANNER_OFF;
      }
      else if ( ! strcmp(optarg, "up")) {
	smode = SCANNER_SEEK_UP;
      }
      else if ( ! strcmp(optarg, "down")) {
	smode = SCANNER_SEEK_DOWN;
      }

      break;
    }
  }

  // change sample rate
  if (fs == 250e3 || fs == 1e6 || fs == 1.92e6 || fs == 2e6 || fs == 2.048e6 ||
      fs == 2.4e6) {
    rtl_set_sample_rate(ctrl->r, (uint32_t) fs);
    demod_set_input_rate(ctrl->dem, fs);
  }

  // change demodulation mode
  if (dmode != DEMOD_NONE && current_dmode != dmode) {
    demod_set_mode(ctrl->dem, dmode);
    current_dmode = dmode;
  }

  // change scanning mode
  if (change_smode) {
    switch (current_dmode) {
    case DEMOD_FM:
    case DEMOD_AM:
      scanner_set_mode(ctrl->scan, smode);
      break;

    // not available for other modes
    default: break;
    }
  }
  
  // change frequency
  if (fc > 0) {
    switch (current_dmode) {
    case DEMOD_FM:
      if (87.9e6 <= fc && fc <= 107.9e6) {
	rtl_set_center_freq(ctrl->r, (uint32_t) fc);
	demod_set_center_freq(ctrl->dem, fc);
      }
      break;

    case DEMOD_AM:
      if (540e3 <= fc && fc <= 1700e3) {
	rtl_set_center_freq(ctrl->r, (uint32_t) (fc + 125e6));
	demod_set_center_freq(ctrl->dem, fc);
      }
      break;
      
    default: break;
    }
  }

  // apply changes to the demodulator
  demod_execute(ctrl->dem);
}

void controller_destroy(controller ctrl)
{
  free(ctrl);
}

void _controller_create_header(controller ctrl, char * header, size_t * header_size)
{
  bool scanning;
  bool seeking;
  int fc, fs;
  int last_station_found;
  float throughput;
  float snr;
  const char * dmode_name;
  scanner_mode smode;

  fc = (int) rtl_get_center_freq(ctrl->r);
  fs = (int) rtl_get_sample_rate(ctrl->r);
  dmode_name = demod_get_mode_name(ctrl->dem);
  throughput = demod_get_throughput(ctrl->dem);
  snr = demod_get_snr(ctrl->dem);
  smode = scanner_get_mode(ctrl->scan);
  scanning = smode == SCANNER_ON;
  seeking = smode == SCANNER_SEEK_UP || smode == SCANNER_SEEK_DOWN;
  
  last_station_found = scanner_get_last_station_found(ctrl->scan);
    
  // format JSON string
  sprintf(header,
	  ("{\"fc\": %d, \"fs\": %d, \"mode\": \"%s\", \"throughput\": %f, "
	   "\"snr\": %f, \"scanning\": %d, \"seeking\": %d, "
	   "\"lastStationFound\": %d }"),
	  fc, fs, dmode_name, throughput, snr, scanning, seeking,
	  last_station_found);
    
  *header_size = strlen(header);
} 

void controller_execute(controller ctrl)
{
  struct timespec time;
  float dt;
  
  char header[2048];
  size_t header_size = 0;
  
  int16_t * data;
  int data_len;
  size_t data_size;
  
  clock_gettime(CLOCK_REALTIME_COARSE, & time);

  // time (secs) since last heartbeat
  dt = (time.tv_sec - ctrl->heartbeat_time.tv_sec);
  dt += (time.tv_nsec - ctrl->heartbeat_time.tv_nsec) / 1e9;

  if (dt >= 0.25f) {
    _controller_create_header(ctrl, header, & header_size);

    // reset sample count
    ctrl->heartbeat_num_samples = 0;
    
    // update time
    ctrl->heartbeat_time.tv_sec = time.tv_sec;
    ctrl->heartbeat_time.tv_nsec = time.tv_nsec;
  }

  // give control to the scanner
  scanner_execute(ctrl->scan, ctrl->dem, ctrl->r);

  // block until new output is available
  demod_pop_and_lock(ctrl->dem, & data, & data_len);
  
  data_size = data_len * sizeof(int16_t);

  // send to client
  websocket_send(ctrl->ws, header, header_size, data, data_size);

  demod_release(ctrl->dem);

  // keep count
  ctrl->heartbeat_num_samples += data_len;
}
