#ifndef _CERVER_AUTH_H_
#define _CERVER_AUTH_H_

#include <stdlib.h>

#include "cerver/types/types.h"

#include "cerver/packets.h"
#include "cerver/cerver.h"
#include "cerver/client.h"

#define DEFAULT_AUTH_TRIES              3

struct _Cerver;

// info for the server to perfom a correct client authentication
typedef struct Auth {

    Packet *auth_packet;              // requests client authentication

    u8 max_auth_tries;                // client's chances of auth before being dropped
    delegate authenticate;            // authentication function

} Auth;

extern Auth *auth_new (void);
extern void auth_delete (Auth *auth);

// generates an authentication packet with client auth request
extern Packet *auth_packet_generate (void);

// handles an packet from an on hold connection
extern void on_hold_packet_handler (void *ptr);

// if the cerver requires authentication, we put the connection on hold
// until it has a sucess authentication or it failed to, so it is dropped
// returns 0 on success, 1 on error
extern u8 on_hold_connection (struct _Cerver *cerver, Connection *connection);

#endif