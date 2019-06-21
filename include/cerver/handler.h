#ifndef _CERVER_HANDLER_H_
#define _CERVER_HANDLER_H_

#include <stdlib.h>

#include "cerver/types/types.h"
#include "cerver/cerver.h"

struct _Server;

// FIXME: maybe change this to use packets instead!!
// auxiliary struct for handle_recieved_buffer Action
typedef struct RecvdBufferData {

    struct _Server *server; 
    i32 sock_fd;
    char *buffer; 
    size_t total_size; 
    bool onHold;

} RecvdBufferData;

extern void rcvd_buffer_data_delete (RecvdBufferData *data);

// get a free index in the main cerver poll strcuture
extern i32 cerver_poll_get_free_idx (Cerver *cerver);

// server poll loop to handle events in the registered socket's fds
extern u8 cerver_poll (Cerver *cerver);

#endif