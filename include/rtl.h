#ifndef __RTL_SOURCE_H__
#define __RTL_SOURCE_H__

#include <rtl-sdr.h>

#define RTL_DEFAULT_SAMPLE_RATE 24000
#define RTL_DEFAULT_BUFFER_LENGTH 16384
#define RTL_MAX_OVERSAMPLE 16
#define RTL_MAX_BUFFER_LENGTH (RTL_MAX_OVERSAMPLE * RTL_DEFAULT_BUFFER_LENGTH)

typedef void (* rtl_execute_callback)(int8_t * buf, int len, void * ctx);
typedef struct rtl_s * rtl;

rtl rtl_create(int device_index);
void rtl_destroy(rtl r);
void rtl_execute(rtl r, rtl_execute_callback cb, void * ctx);

int rtl_reset_buffer(rtl r);
uint32_t rtl_get_center_freq(rtl r);
int rtl_set_center_freq(rtl r, uint32_t center_freq);
int rtl_set_auto_gain(rtl r);
int rtl_set_freq_correction(rtl r, int ppm_error);
int rtl_set_gain_mode(rtl r, int gain);
uint32_t rtl_get_sample_rate(rtl r);
int rtl_set_sample_rate(rtl r, uint32_t sample_rate);

#endif
