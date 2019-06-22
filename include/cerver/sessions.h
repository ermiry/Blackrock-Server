#ifndef _CERVER_SESSIONS_H_
#define _CERVER_SESSIONS_H_

#include "cerver/types/types.h"
#include "cerver/network.h"

// create a unique session id for each client based on connection values
extern char *session_default_generate_id (i32 fd, const struct sockaddr_storage address);

#endif