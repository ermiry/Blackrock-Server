#ifndef _CERVER_HANDLER_H_
#define _CERVER_HANDLER_H_

#include <stdlib.h>

#include "cerver/types/types.h"
#include "cerver/cerver.h"

struct _Server;

// auxiliary struct for handle_recieved_buffer Action
typedef struct RecvdBufferData {

    struct _Server *server; 
    i32 sock_fd;
    char *buffer; 
    size_t total_size; 
    bool onHold;

} RecvdBufferData;

extern void rcvd_buffer_data_delete (RecvdBufferData *data);

#endif