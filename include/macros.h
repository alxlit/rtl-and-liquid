#ifndef __MACROS_H__
#define __MACROS_H__

#include <stdio.h>

#define DEBUG(...) fprintf(stdout, __VA_ARGS__);
#define ERROR(...) fprintf(stderr, __VA_ARGS__);

// pthread helpers (from rtl_fm.c)
#define safe_cond_signal(n, m) \
  pthread_mutex_lock(m); \
  pthread_cond_signal(n); \
  pthread_mutex_unlock(m);

#define safe_cond_wait(n, m) \
  pthread_mutex_lock(m); \
  pthread_cond_wait(n, m); \
  pthread_mutex_unlock(m);

#define safe_cond_timedwait(r, ts, n, m) \
  pthread_mutex_lock(m); \
  r = pthread_cond_timedwait(n, m, ts); \
  pthread_mutex_unlock(m);
  
#endif
