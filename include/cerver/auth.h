#ifndef _CERVER_AUTH_H_
#define _CERVER_AUTH_H_

#include <stdlib.h>

#include "cerver/types/types.h"

#include "cerver/packets.h"
#include "cerver/client.h"

/*** Sessions ***/

// create a unique session id for each client based on connection values
extern char *session_default_generate_id (i32 fd, const struct sockaddr_storage address);

/*** Auth ***/

// info for the server to perfom a correct client authentication
typedef struct Auth {

    Packet *auth_packet;              // requests client authentication

    u8 max_auth_tries;                // client's chances of auth before being dropped
    delegate authenticate;            // authentication function

} Auth;

extern Auth *auth_new (void);
extern void auth_delete (Auth *auth);

// if the cerver requires authentication, we send the newly connected clients to an on hold
// structure until they authenticate, if not, they are just dropped by the cerver
void client_on_hold (Cerver *cerver, Client *client, i32 fd);

#endif