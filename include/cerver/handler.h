#ifndef _CERVER_HANDLER_H_
#define _CERVER_HANDLER_H_

#include <stdlib.h>

#include "cerver/types/types.h"
#include "cerver/cerver.h"

struct _Cerver;

// reallocs main cerver poll fds
// returns 0 on success, 1 on error
extern u8 cerver_realloc_main_poll_fds (struct _Cerver *cerver);

// get a free index in the main cerver poll strcuture
extern i32 cerver_poll_get_free_idx (struct _Cerver *cerver);

// get the idx of the connection sock fd in the cerver poll fds
extern i32 cerver_poll_get_idx_by_sock_fd (struct _Cerver *cerver, i32 sock_fd);

// server poll loop to handle events in the registered socket's fds
extern u8 cerver_poll (struct _Cerver *cerver);

#endif