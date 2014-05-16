#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include <pthread.h>

typedef void (* websocket_receive_callback)(void * buf, size_t len, void * ctx);
typedef struct websocket_s * websocket;

websocket websocket_create();
void websocket_destroy(websocket ws);
void websocket_execute(websocket ws, websocket_receive_callback cb, void * ctx);

void websocket_send(websocket ws,
		    void * header,
		    size_t header_size,
		    void * data,
		    size_t data_size);

#endif
