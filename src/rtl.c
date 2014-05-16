#include <pthread.h>
#include <rtl-sdr.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"
#include "rtl.h"

typedef enum { RTL_HALTED, RTL_RUNNING, RTL_EXITING } rtl_state;

struct rtl_s
{
  rtl_state state;
  pthread_mutex_t state_m;
  
  pthread_t thread;

  // the librtlsdr device object and the index used to look it up
  rtlsdr_dev_t * device;
  int device_index;

  // called whenever samples are received from the RTL
  rtl_execute_callback execute_callback;
  void * execute_ctx;

  // buffer for samples received from the RTL
  int8_t buffer[RTL_MAX_BUFFER_LENGTH];
  int buffer_len;
  pthread_mutex_t buffer_m;

  // our own record of parameters otherwise hidden by librtlsdr
  int center_freq_correction;
  uint32_t sample_rate;
};

rtl_state _rtl_get_state(rtl r)
{
  rtl_state state;
  pthread_mutex_lock( & r->state_m);
  state = r->state;
  pthread_mutex_unlock( & r->state_m);
  return state;
}

void _rtl_set_state(rtl r, rtl_state state)
{
  pthread_mutex_lock( & r->state_m);
  r->state = state;
  pthread_mutex_unlock( & r->state_m);
}

/**
 * Adapted from verbose_device_search, currently just searches device indexes.
 * See librtlsdr/src/convenience/convenience.c
 */
int _rtl_search_for_device(char * s)
{
  int i, device_count, device_index;
  char * s_end;
  char vendor[256], product[256], serial[256];

  device_count = rtlsdr_get_device_count();

  if ( ! device_count) {
    ERROR("No supported devices found.\n");
    return -1;
  }
  
  // print out device information
  DEBUG("Found %d device(s):\n", device_count);

  for (i = 0; i < device_count; i++) {
    rtlsdr_get_device_usb_strings(i, vendor, product, serial);
    DEBUG("  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
  }

  device_index = (int) strtol(s, & s_end, 0);

  // check for device index
  if (s_end[0] == '\0' && device_index >= 0 && device_index < device_count) {
    return device_index;
  }

  ERROR("No matching devices found.\n");

  return -1;
}

static void _rtl_read_async_callback(unsigned char * buf, uint32_t len, void * ctx)
{
  rtl r = (rtl) ctx;

  if ( ! ctx || _rtl_get_state(r) == RTL_EXITING) { return; }
  
  pthread_mutex_lock( & r->buffer_m);
  
  r->buffer_len = (int) len;
  
  int i;
  
  for (i = 0; i < len; i++) {
    r->buffer[i] = ((int8_t) buf[i]) + 128;
    // r->buffer[i] = ((int16_t) buf[i]) - 127;
  }
  
  // invoke callback
  r->execute_callback(r->buffer, r->buffer_len, r->execute_ctx);
  
  pthread_mutex_unlock( & r->buffer_m);
}

static void * _rtl_thread_fn(void * arg)
{
  rtl r = (rtl) arg;

  // this takes over thread
  rtlsdr_read_async(r->device, _rtl_read_async_callback, (void *) r, 0, 0);

  // async canceled, spin until we're supposed to exit
  while (_rtl_get_state(r) != RTL_EXITING) {
    usleep(0.01e6);
  }

  return NULL;
}

rtl rtl_create(int device_index)
{
  int status;

  // create object
  rtl r = (rtl) malloc(sizeof(struct rtl_s));

  if (device_index < 0) {
    device_index = _rtl_search_for_device("0");

    if (device_index < 0) {
      exit(1);
    }
  }
  
  status = rtlsdr_open( & r->device, device_index);
    
  if (status < 0) {
    ERROR("Failed to open rtlsdr device %d.\n", device_index);
    exit(1);
  }

  r->sample_rate = RTL_DEFAULT_SAMPLE_RATE;
  r->state = RTL_HALTED;

  pthread_mutex_init( & r->buffer_m, NULL);
  pthread_mutex_init( & r->state_m, NULL);
  
  return r;
}

void rtl_destroy(rtl r)
{
  DEBUG("Destroying rtl...\n");
  
  _rtl_set_state(r, RTL_EXITING);
  
  rtlsdr_cancel_async(r->device);
  
  pthread_join(r->thread, NULL);
  pthread_mutex_destroy( & r->buffer_m);
  pthread_mutex_destroy( & r->state_m);
  
  rtlsdr_close(r->device);
  free(r);
}

void rtl_execute(rtl r, rtl_execute_callback cb, void * ctx)
{
  if (_rtl_get_state(r) != RTL_HALTED) return;
  
  r->execute_callback = cb;
  r->execute_ctx = ctx;
  
  _rtl_set_state(r, RTL_RUNNING);

  // spawn thread
  pthread_create( & r->thread, NULL, _rtl_thread_fn, (void *) r);
}

int rtl_reset_buffer(rtl r)
{
  int status;
  status = rtlsdr_reset_buffer(r->device);
  
  if (status < 0) {
    ERROR("Failed to reset buffers.\n");
  }

  return status;
}

uint32_t rtl_get_center_freq(rtl r)
{
  return rtlsdr_get_center_freq(r->device);
}

int rtl_set_center_freq(rtl r, uint32_t center_freq)
{
  int status;
  status = rtlsdr_set_center_freq(r->device, center_freq);
  
  if (status < 0) {
    ERROR("Failed to set center frequency.\n");
  }
  else {
    DEBUG("Tuned to %u Hz.\n", center_freq);
  }

  return status;
}

int rtl_set_auto_gain(rtl r)
{
  int status;
  status = rtlsdr_set_tuner_gain_mode(r->device, 0);

  if (status != 0) {
    ERROR("Failed to set tuner gain.\n");
  }
  else {
    DEBUG("Tuner gain set to automatic.\n");
  }

  return status;
}

int rtl_set_freq_correction(rtl r, int ppm_error)
{
  int status;
  status = rtlsdr_set_freq_correction(r->device, ppm_error);

  if (status < 0) {
    ERROR("Failed to set ppm error.\n");
  }
  else {
    DEBUG("Tuner error set to %i ppm.\n", ppm_error);
  }

  return status;
}

int rtl_set_gain_mode(rtl r, int gain)
{
  int status;
  status = rtlsdr_set_tuner_gain_mode(r->device, 1);
  
  if (status < 0) {
    ERROR("Failed to enable manual gain.\n");
    return status;
  }
  
  status = rtlsdr_set_tuner_gain(r->device, gain);
  
  if (status != 0) {
    ERROR("Failed to set tuner gain.\n");
  }
  else {
    DEBUG("Tuner gain set to %0.2f dB.\n", gain/10.0);
  }

  return status;
}

uint32_t rtl_get_sample_rate(rtl r)
{
  return rtlsdr_get_sample_rate(r->device);
}

int rtl_set_sample_rate(rtl r, uint32_t sample_rate)
{
  int status;
  status = rtlsdr_set_sample_rate(r->device, sample_rate);
  
  if (status < 0) {
    ERROR("Failed to set sample rate.\n");
  }
  else {
    DEBUG("Sampling at %u S/s.\n", sample_rate);
  }

  return status;
}
