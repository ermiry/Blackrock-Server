#ifndef _CERVER_AUTH_H_
#define _CERVER_AUTH_H_

#include <stdlib.h>

#include "cerver/types/types.h"

// info for the server to perfom a correct client authentication
typedef struct Auth {

    void *req_auth_packet;
    size_t auth_packet_size;

    u8 max_auth_tries;                // client's chances of auth before being dropped
    delegate authenticate;            // authentication function

} Auth;

extern Auth *auth_new (void);
extern void auth_delete (Auth *auth);

#endif