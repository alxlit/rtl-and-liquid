#include <complex.h>
#include <liquid/liquid.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "demod.h"
#include "macros.h"

#define NF (1.0f / 32767.0f) /* normalization factor for float to int16 */

typedef enum { DEMOD_HALTED, DEMOD_RUNNING, DEMOD_EXITING } demod_state;

struct demod_common_s
{
  demod_mode mode;
  uint32_t center_freq;
  int input_rate;
  int output_rate;
};

struct demod_am_s
{
  ampmodem dem;
  msresamp_crcf resamp1;
  float r1;
  float mi;
};

struct demod_fm_s
{
  freqdem dem;
  float kf;
  msresamp_crcf resamp1;
  resamp_rrrf resamp2;
  float r1;
  float r2;
};

struct demod_metrics_s
{
  float snr;
  float throughput;
};

struct demod_s
{
  pthread_t thread;
  
  // common parameters
  struct demod_common_s common;
  pthread_mutex_t common_m;

  // FM parameters
  struct demod_fm_s fm;
  pthread_mutex_t fm_m;

  // AM parameters
  struct demod_am_s am;
  pthread_mutex_t am_m; 

  // input buffer
  int8_t input[RTL_MAX_BUFFER_LENGTH];
  int input_len;
  pthread_mutex_t input_m;
  pthread_cond_t input_ready;
  pthread_mutex_t input_ready_m;

  // output buffer
  int16_t output[RTL_MAX_BUFFER_LENGTH];
  int output_len;
  pthread_mutex_t output_m;
  pthread_cond_t output_ready;
  pthread_mutex_t output_ready_m;

  // metrics
  struct demod_metrics_s metrics;
  pthread_mutex_t metrics_m;

  // operational state
  demod_state state;
  pthread_mutex_t state_m;
};

demod_state _demod_get_state(demod dem)
{
  demod_state state;
  pthread_mutex_lock( & dem->state_m);
  state = dem->state;
  pthread_mutex_unlock( & dem->state_m);
  return state;
}

void _demod_set_state(demod dem, demod_state state)
{
  pthread_mutex_lock( & dem->state_m);
  dem->state = state;
  pthread_mutex_unlock( & dem->state_m);
}

static const unsigned int _demod_num_modes = 6;

static const char * _demod_mode_names[] = {
  [DEMOD_FM] = "fm",
  [DEMOD_AM] = "am",
  [DEMOD_NONE] = "none"
};

const char * demod_lookup_mode_name(demod_mode mode)
{
  return _demod_mode_names[mode];
}

demod_mode demod_lookup_mode(char * s)
{
  demod_mode mode = DEMOD_NONE;
  int i;
  
  for (i = 0; i < _demod_num_modes; i++) {
    if (strcmp(s, _demod_mode_names[i]) == 0) {
      mode = (demod_mode) i;
      break;
    }
  }
  
  return mode;
}

static const int _demod_mode_frequency_steps[] = {
  [DEMOD_FM] = 0.2e6,
  [DEMOD_AM] = 10e3,
};

int demod_lookup_frequency_step(demod_mode mode)
{
  return _demod_mode_frequency_steps[mode];
};

void _demod_am(demod dem);
void _demod_am_init(demod dem);
void _demod_am_teardown(demod dem);

void _demod_am_init(demod dem)
{
  // make sure any previously allocated memory is freed
  _demod_am_teardown(dem);
  
  pthread_mutex_lock( & dem->am_m);
  
  struct demod_am_s * am = & dem->am;

  int input_rate = demod_get_input_rate(dem);
  int output_rate = demod_get_output_rate(dem);
  
  float As = 60.0f;
  
  // first stage decimation factor to get sample rate to 400kHz
  am->r1 = (float) output_rate/ ((float) input_rate);
  
  // initialize multistage resampler
  am->resamp1 = msresamp_crcf_create(am->r1, As);

  am->mi = 0.9f;

  // create AM demodulator
  float fc = 0.0f;
  int suppressed = 0;
  liquid_ampmodem_type t = LIQUID_AMPMODEM_DSB;
  
  am->dem = ampmodem_create(am->mi, fc, t, suppressed);
  
  pthread_mutex_unlock( & dem->am_m);
}

void _demod_am_teardown(demod dem)
{
  pthread_mutex_lock( & dem->am_m);
  
  if (dem->am.dem) { ampmodem_destroy(dem->am.dem); }
  if (dem->am.resamp1) { msresamp_crcf_destroy(dem->am.resamp1); }
  
  pthread_mutex_unlock( & dem->am_m);
}

void _demod_am(demod dem)
{
  struct demod_am_s * am = & dem->am;

  pthread_mutex_lock( & dem->am_m);

  unsigned int nx = dem->input_len / 2;
  unsigned int ny = ceil(am->r1 * (float) nx);
  
  float complex x[nx];
  float complex y[ny];

  unsigned int i;
  
  for (i = 0; i < nx; i++) {
    x[i] = ((float) dem->input[2*i]) + ((float) dem->input[2*i+1])*_Complex_I;
  }

  // downsample
  msresamp_crcf_execute(am->resamp1, x, nx, y, & ny);
  
  float t = 0.0f;
  
  for (i = 0; i < ny; i++) {
    ampmodem_demodulate(am->dem, y[i], & t);
    dem->output[i] = (int16_t) (am->mi * t / NF);
  }
  
  dem->output_len =  ny;
  
  pthread_mutex_unlock( & dem->am_m);
}

void _demod_fm(demod dem);
void _demod_fm_init(demod dem);
void _demod_fm_teardown(demod dem);

void _demod_fm_init(demod dem)
{
  // make sure any previously allocated memory is freed
  _demod_fm_teardown(dem);

  pthread_mutex_lock( & dem->fm_m);
  
  struct demod_fm_s * fm = & dem->fm;

  fm->kf = 1.0f;
  fm->dem = freqdem_create(fm->kf);
  
  int input_rate = demod_get_input_rate(dem);
  int output_rate = demod_get_output_rate(dem);

  // choose a multiple of the output rate
  float intermediate_rate = 4.0f * ((float) output_rate);
  
  float As = 60.0f;

  // first stage decimation factor
  fm->r1 = intermediate_rate / ((float) input_rate);

  // second stage, to get to final output rate (usually 48 kHz)
  fm->r2 = ((float) output_rate) / intermediate_rate;
  
  // initialize multistage resampler
  fm->resamp1 = msresamp_crcf_create(fm->r1, As);

  // initialize final resampler
  unsigned int h_len = 9;
  float bw = 20e3 / intermediate_rate;
  unsigned int npfb = 16;

  fm->resamp2 = resamp_rrrf_create(fm->r2, h_len, bw, As, npfb);

  pthread_mutex_unlock( & dem->fm_m);
}

void _demod_fm_teardown(demod dem)
{
  pthread_mutex_lock( & dem->fm_m);
  
  struct demod_fm_s * fm = & dem->fm;
  
  if (fm->dem) { freqdem_destroy(fm->dem); }
  if (fm->resamp1) { msresamp_crcf_destroy(fm->resamp1); }
  if (fm->resamp2) { resamp_rrrf_destroy(fm->resamp2); }
  
  pthread_mutex_unlock( & dem->fm_m);
}

void _demod_fm(demod dem)
{
  pthread_mutex_lock( & dem->fm_m);

  struct demod_fm_s * fm = & dem->fm;
  
  unsigned int nx = dem->input_len / 2;
  unsigned int ny = ceil(fm->r1 * (float) nx);
  unsigned int nz = ceil(fm->r2 * (float) ny);
  
  float complex x[nx];
  float complex y[ny];
  float z[nz];

  unsigned int i, j;
  unsigned int num_written = 0;
  
  for (i = 0; i < nx; i++) {
    x[i] = ((float) dem->input[2*i]) + ((float) dem->input[2*i+1])*_Complex_I;
  }

  // downsample to intermediate rate
  msresamp_crcf_execute(fm->resamp1, x, nx, y, & ny);
  
  float t = 0.0f;

  for (i = 0, j = 0; i < ny; i++, j += num_written) {
    freqdem_demodulate(fm->dem, y[i], & t);
    
    // downsample to output rate
    resamp_rrrf_execute(fm->resamp2, t, & z[j], & num_written);
  }
  
  dem->output_len = (int) j;

  for (i = 0; i < dem->output_len; i++) {
    dem->output[i] = (int16_t) (fm->kf * z[i] * 32768.0f);
  }

  pthread_mutex_unlock( & dem->fm_m);
}

void _demod_measure_snr(demod dem)
{
  // power
  float S = 0.0f, N = 0.0f;
  
  int input_rate = demod_get_input_rate(dem);
  
  int step = 10;
  int i, j;

  // effective sample rate
  float fs = ((float) input_rate) / step;
  
  // don't need every sample; reduce, pick a power of two
  int n = 1 << (int) ceilf(log2f(((float) dem->input_len) / step));

  // one-sided bandwidth of the signal
  float bw = 250.0f;

  // number of bins occupied by the signal, per-side
  int num_bins = (int) (bw / fs * n);
  
  float complex x[n];
  float complex y[n];
  
  int flags = 0; // ignored
  
  fftplan q = fft_create_plan(n, x, y, LIQUID_FFT_FORWARD, flags);

  // fill with data, zero padding
  for (i = 0, j = 0; i < n; i++, j += step) {
    if (j >= dem->input_len) {
      x[i] = 0.0f;
    }
    else {
      x[i] = ((float) dem->input[i]) + ((float) dem->input[i+1])*_Complex_I;
      x[i] = NF * x[i];
    }
  }
  
  fft_execute(q);

  // compute signal power, noise floor
  for (i = 0; i < n; i++) {
    y[i] = cabs(y[i]);
    
    if (i < num_bins || i > (n - num_bins)) {
      S += y[i];
    }
    else {
      N += y[i];
    }
  }

  // average
  S = S / (2 * num_bins);
  N = N / (n - (2 * num_bins));

  // compute SNR (in dB)
  dem->metrics.snr = 20*log10f(S / N);

  fft_destroy_plan(q);
} 

static void * _demod_thread_fn(void * ctx)
{
  demod dem = (demod) ctx;
  
  clock_t t1, t2;
  float dt;

  int i;

  while (_demod_get_state(dem) != DEMOD_EXITING) {
    safe_cond_wait( & dem->input_ready, & dem->input_ready_m);

    t1 = clock();
    
    pthread_mutex_lock( & dem->metrics_m);    
    pthread_mutex_lock( & dem->input_m);
    pthread_mutex_lock( & dem->output_m);

    _demod_measure_snr(dem);

    switch (demod_get_mode(dem)) {
    case DEMOD_FM:
      _demod_fm(dem);
      break;
    case DEMOD_AM:
      _demod_am(dem);
      break;
    default: break;
    }

    // rudimentary squelch
    if (dem->metrics.snr < SQUELCH_THRESHOLD) {
      for (i = 0; i < dem->output_len; i++) { dem->output[i] = 0; } }

    pthread_mutex_unlock( & dem->input_m);
    pthread_mutex_unlock( & dem->output_m);

    t2 = clock();
    dt = ((float) t2 - (float) t1) / CLOCKS_PER_SEC;
    
    dem->metrics.throughput = ((float) dem->output_len) / dt;
    
    pthread_mutex_unlock( & dem->metrics_m);
    
    // signal that we've got new output
    safe_cond_signal( & dem->output_ready, & dem->output_ready_m);
  }
  
  return NULL;
}

demod demod_create()
{
  demod dem = (demod) malloc(sizeof(struct demod_s));
  
  struct demod_common_s * common = & dem->common;
  struct demod_am_s * am = & dem->am;
  struct demod_fm_s * fm = & dem->fm;
  
  // initialize common parameters
  common->mode = DEMOD_NONE;
  common->center_freq = -1;
  common->input_rate = -1;
  common->output_rate = -1;

  // initialize FM parameters
  fm->dem = NULL;
  fm->resamp1 = NULL;
  fm->resamp2 = NULL;
  fm->r1 = 0.0f;
  fm->r2 = 0.0f;

  // initialize AM parameters
  am->dem = NULL;
  am->resamp1 = NULL;
  am->r1 = 0.0f;
    
  // initialize buffers
  dem->input_len = 0;
  dem->output_len = 0;

  // initialize operational state
  dem->state = DEMOD_HALTED;
  
  struct demod_metrics_s * metrics = & dem->metrics;
  
  // initialize metrics
  metrics->snr = 0.0f;
  metrics->throughput = 0.0f;

  pthread_mutex_init( & dem->common_m, NULL);
  pthread_mutex_init( & dem->am_m, NULL);
  pthread_mutex_init( & dem->fm_m, NULL);
  
  pthread_mutex_init( & dem->input_m, NULL);
  pthread_cond_init( & dem->input_ready, NULL);
  pthread_mutex_init( & dem->input_ready_m, NULL);
  
  pthread_mutex_init( & dem->output_m, NULL);
  pthread_cond_init( & dem->output_ready, NULL);
  pthread_mutex_init( & dem->output_ready_m, NULL);
  
  pthread_mutex_init( & dem->state_m, NULL);
  
  return dem;
}

void demod_destroy(demod dem)
{
  DEBUG("Destroying demod...\n");
  
  if (_demod_get_state(dem) != DEMOD_EXITING) { demod_exit(dem); }
  
  _demod_am_teardown(dem);
  _demod_fm_teardown(dem);
  
  pthread_mutex_destroy( & dem->common_m);
  pthread_mutex_destroy( & dem->am_m);  
  pthread_mutex_destroy( & dem->fm_m);

  pthread_mutex_destroy( & dem->input_m);
  pthread_cond_destroy( & dem->input_ready);
  pthread_mutex_destroy( & dem->input_ready_m);
  
  pthread_mutex_destroy( & dem->output_m);  
  pthread_cond_destroy( & dem->output_ready);  
  pthread_mutex_destroy( & dem->output_ready_m);
  
  pthread_mutex_destroy( & dem->state_m);
  
  free(dem);
}

void demod_exit(demod dem)
{
  _demod_set_state(dem, DEMOD_EXITING);
  
  pthread_join(dem->thread, NULL);
}

/**
 * This method is called to apply any changes to the modulation parameters,
 * e.g. after changing center frequency.
 */
void demod_execute(demod dem)
{
  pthread_mutex_lock( & dem->metrics_m);
  
  // reset metrics  
  dem->metrics.snr = 0.0f;
  dem->metrics.throughput = 0.0f;
  
  pthread_mutex_unlock( & dem->metrics_m);

  switch (demod_get_mode(dem)) {
  case DEMOD_FM:
    _demod_fm_init(dem);
    break;
  case DEMOD_AM:
    _demod_am_init(dem);
    break;
  default: break;
  }

  if (_demod_get_state(dem) == DEMOD_HALTED) {
    pthread_create( & dem->thread, NULL, _demod_thread_fn, (void *) dem); }
  
  _demod_set_state(dem, DEMOD_RUNNING);
}

float demod_get_snr(demod dem)
{
  float snr;
  pthread_mutex_lock( & dem->metrics_m);
  snr = dem->metrics.snr;
  pthread_mutex_unlock( & dem->metrics_m);
  return snr;
}

float demod_get_throughput(demod dem)
{
  float throughput;
  pthread_mutex_lock( & dem->metrics_m);
  throughput = dem->metrics.throughput;
  pthread_mutex_unlock( & dem->metrics_m);
  return throughput;
}

int demod_get_decim_factor(demod dem)
{
  int decim_factor;
  pthread_mutex_lock( & dem->common_m);
  decim_factor = dem->common.input_rate / dem->common.output_rate;
  pthread_mutex_unlock( & dem->common_m);
  return decim_factor;
}

int demod_get_frequency_step(demod dem)
{
  return demod_lookup_frequency_step( demod_get_mode(dem) );
}

demod_mode demod_get_mode(demod dem)
{
  demod_mode mode;
  pthread_mutex_lock( & dem->common_m);
  mode = dem->common.mode;
  pthread_mutex_unlock( & dem->common_m);
  return mode;
}

const char * demod_get_mode_name(demod dem)
{
  return demod_lookup_mode_name(demod_get_mode(dem));
}

void demod_set_mode(demod dem, demod_mode mode)
{
  pthread_mutex_lock( & dem->common_m);
  dem->common.mode = mode;
  pthread_mutex_unlock( & dem->common_m);
}

uint32_t demod_get_center_freq(demod dem)
{
  uint32_t center_freq;
  pthread_mutex_lock( & dem->common_m);
  center_freq = dem->common.center_freq;
  pthread_mutex_unlock( & dem->common_m);
  return center_freq;
}

void demod_set_center_freq(demod dem, uint32_t center_freq)
{
  pthread_mutex_lock( & dem->common_m);
  dem->common.center_freq = center_freq;
  pthread_mutex_unlock( & dem->common_m);
}

int demod_get_input_rate(demod dem)
{
  int input_rate;
  pthread_mutex_lock( & dem->common_m);
  input_rate = dem->common.input_rate;
  pthread_mutex_unlock( & dem->common_m);
  return input_rate;
}

void demod_set_input_rate(demod dem, int input_rate)
{
  pthread_mutex_lock( & dem->common_m);
  dem->common.input_rate = input_rate;
  pthread_mutex_unlock( & dem->common_m);
}

int demod_get_output_rate(demod dem)
{
  int output_rate;
  pthread_mutex_lock( & dem->common_m);
  output_rate = dem->common.output_rate;
  pthread_mutex_unlock( & dem->common_m);
  return output_rate;
}

void demod_set_output_rate(demod dem, int output_rate)
{
  pthread_mutex_lock( & dem->common_m);
  dem->common.output_rate = output_rate;
  pthread_mutex_unlock( & dem->common_m);
}

void demod_pop_and_lock(demod dem, int16_t ** buf, int * len)
{
  safe_cond_wait( & dem->output_ready, & dem->output_ready_m);
  
  *buf = dem->output;
  *len = dem->output_len;
  
  pthread_mutex_lock( & dem->output_m);
}

void demod_push(demod dem, int8_t * buf, int len)
{
  pthread_mutex_lock( & dem->input_m);
  dem->input_len = len;
  memcpy(dem->input, buf, len * sizeof(int8_t));
  pthread_mutex_unlock( & dem->input_m);

  // we've acquired new samples
  safe_cond_signal( & dem->input_ready, & dem->input_ready_m);
}

void demod_release(demod dem)
{
  pthread_mutex_unlock( & dem->output_m);
}
