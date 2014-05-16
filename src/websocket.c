#include <errno.h>
#include <libwebsockets.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "macros.h"
#include "rtl.h"
#include "websocket.h"

#define WEBSOCKET_MAX_HEADER_LENGTH 1024
#define WEBSOCKET_MAX_BUFFER_LENGTH (LWS_SEND_BUFFER_PRE_PADDING + \
				     WEBSOCKET_MAX_HEADER_LENGTH + \
				     RTL_MAX_BUFFER_LENGTH * sizeof(int16_t) + \
				     LWS_SEND_BUFFER_POST_PADDING)

typedef enum { WEBSOCKET_HALTED, WEBSOCKET_RUNNING, WEBSOCKET_EXITING }
  websocket_state;

struct websocket_s
{
  unsigned char output[WEBSOCKET_MAX_BUFFER_LENGTH];
  size_t output_size;
  enum libwebsocket_write_protocol output_protocol;
  pthread_mutex_t output_m;
  
  sem_t output_sem; // eventually implement a queue?
  
  websocket_receive_callback receive_callback;
  void * receive_ctx;
  pthread_mutex_t receive_callback_m; // not really needed
  
  pthread_t thread;
  struct libwebsocket_context * context;
  struct lws_context_creation_info * context_info;
  websocket_state state;
  pthread_mutex_t state_m;
};

websocket_state _websocket_get_state(websocket ws)
{
  websocket_state state;
  pthread_mutex_lock( & ws->state_m);
  state = ws->state;
  pthread_mutex_unlock( & ws->state_m);
  return state;
}

void _websocket_set_state(websocket ws, websocket_state state)
{
  pthread_mutex_lock( & ws->state_m);
  ws->state = state;
  pthread_mutex_unlock( & ws->state_m);
}

static void * _websocket_thread_fn(void * ctx)
{
  websocket ws = (websocket) ctx;
  
  while (_websocket_get_state(ws) != WEBSOCKET_EXITING) {
    libwebsocket_service(ws->context, 50);
  }
  
  return NULL;
}

struct per_session_data_sdr {};

static int _websocket_sdr_callback(struct libwebsocket_context * ctx,
				   struct libwebsocket * wsi,
				   enum libwebsocket_callback_reasons reason,
				   void * arg,
				   void * received,
				   size_t received_len)
{
  websocket ws = (websocket) libwebsocket_context_user(ctx);
  
  int err;
  int n;
  int status = 0;

  // we only support one connection, at least for now
  static struct libwebsocket * primary_wsi = NULL;

  // ignore any others
  if (primary_wsi != NULL && wsi != NULL && primary_wsi != wsi) { return -1; }
  
  switch (reason) {
  case LWS_CALLBACK_ESTABLISHED:
    primary_wsi = wsi;
    break;
    
  case LWS_CALLBACK_SERVER_WRITEABLE:
    err = sem_trywait( & ws->output_sem);
    if (err != 0) { break; }
    
    pthread_mutex_lock( & ws->output_m);

    n = libwebsocket_write(wsi,
			   (unsigned char *) & ws->output[LWS_SEND_BUFFER_PRE_PADDING],
			   ws->output_size,
			   ws->output_protocol);
    
    pthread_mutex_unlock( & ws->output_m);

    if (n < 0) {
      ERROR("problem writing to socket\n");
      status = -1;
    }
    else if (n < (int) ws->output_size) {
      ERROR("partial write\n");
      status = -1;
    }
    
    break;
    
  case LWS_CALLBACK_RECEIVE:
    if (ws->receive_callback != NULL) {
      ws->receive_callback(received, received_len, ws->receive_ctx);
    }
    break;
    
  case LWS_CALLBACK_CLOSED:
    primary_wsi = NULL;
    break;
    
  case LWS_CALLBACK_GET_THREAD_ID:
    status = (int) ws->thread;
    break;
    
  default: break;
  }

  // TODO this can probably be improved
  if (primary_wsi != NULL && reason <= LWS_CALLBACK_GET_THREAD_ID) {
    sem_getvalue( & ws->output_sem, & n);
    
    if (n > 0) {
      libwebsocket_callback_on_writable(ctx, primary_wsi);
    }
  }
  
  return status;
}

static struct libwebsocket_protocols _websocket_protocols[] = {
  {
    "sdr",
    _websocket_sdr_callback,
    sizeof(struct per_session_data_sdr),
    0
  },
  { NULL, NULL, 0, 0 }
};
 
websocket websocket_create()
{
  websocket ws = (websocket) malloc(sizeof(struct websocket_s));

  struct lws_context_creation_info * info =
    malloc(sizeof(struct lws_context_creation_info));
  
  lws_set_log_level(LLL_ERR, NULL);

  info->port = 8080;  
  info->iface = NULL;
  info->protocols = _websocket_protocols;  
  info->extensions = libwebsocket_get_internal_extensions();
  info->gid = -1;
  info->uid = -1;
  info->options = 0;
  info->user = ws;
  info->ka_time = 30;
  info->ka_probes = 10;
  info->ka_interval = 1;

  // don't care about proxy
  info->http_proxy_address = "";
  info->http_proxy_port = 0;
  
  // don't care about using SSL stuff
  info->ssl_cert_filepath = NULL;
  info->ssl_private_key_filepath = NULL;
  info->ssl_ca_filepath = NULL;
  info->ssl_cipher_list = NULL;

  ws->context = libwebsocket_create_context(info);
  ws->context_info = info;
  
  if (ws->context == NULL) {
    ERROR("Failed to create websocket!\n");
    exit(1);
  }

  ws->receive_callback = NULL;
  ws->receive_ctx = NULL;

  ws->state = WEBSOCKET_HALTED;
  
  pthread_mutex_init( & ws->output_m, NULL);
  pthread_mutex_init( & ws->state_m, NULL);
  
  sem_init( & ws->output_sem, 0, 0);
  
  return ws;
}

void websocket_destroy(websocket ws)
{
  _websocket_set_state(ws, WEBSOCKET_EXITING);

  libwebsocket_context_destroy(ws->context);

  pthread_join(ws->thread, NULL);
  pthread_mutex_destroy( & ws->output_m);
  pthread_mutex_destroy( & ws->state_m);
  
  sem_destroy( & ws->output_sem);
  
  free(ws);
}

void websocket_execute(websocket ws, websocket_receive_callback cb, void * ctx)
{
  if (_websocket_get_state(ws) != WEBSOCKET_HALTED) return;
  
  _websocket_set_state(ws, WEBSOCKET_RUNNING);
  
  ws->receive_callback = cb;
  ws->receive_ctx = ctx;

  // spawn thread
  pthread_create( & ws->thread, NULL, _websocket_thread_fn, (void *) ws);
}

void websocket_send(websocket ws,
		    void * header,
		    size_t header_size,
		    void * data,
		    size_t data_size)
{
  pthread_mutex_lock( & ws->output_m);
  
  ws->output_protocol = LWS_WRITE_BINARY;
  ws->output_size = sizeof(uint32_t) + header_size + data_size;
  
  void * dest = (void *) & ws->output[LWS_SEND_BUFFER_PRE_PADDING];
  
  // write header size
  uint32_t tmp = header_size;
  memcpy(dest, & tmp, sizeof(uint32_t));

  // write header
  memcpy(dest + sizeof(uint32_t), header, header_size);
  
  // write data
  memcpy(dest + sizeof(uint32_t) + header_size, data, data_size);
  
  pthread_mutex_unlock( & ws->output_m);
  
  int val;
  sem_getvalue( & ws->output_sem, & val);

  // discard if not ready
  if (val == 0) { sem_post( & ws->output_sem); }
}
