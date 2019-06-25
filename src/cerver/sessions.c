#include <stdlib.h>

#include "cerver/types/types.h"

#include "cerver/network.h"
#include "cerver/packets.h"
#include "cerver/client.h"
#include "cerver/sessions.h"
#include "cerver/auth.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/sha-256.h"

SessionData *session_data_new (Packet *packet, AuthData *auth_data, Client *client) {

    SessionData *session_data = (SessionData *) malloc (sizeof (SessionData));
    if (session_data) {
        session_data->packet = packet;
        session_data->auth_data = auth_data;
        session_data->client = client;
    }

    return session_data;

}

void session_data_delete (void *ptr) {

    if (ptr) {
        SessionData *session_data = (SessionData *) ptr;
        session_data->packet = NULL;
        session_data->auth_data = NULL;
        session_data->client = NULL;
    }

}

// FIXME:
// TODO: refactor to use timestamps to generate the token
// create a unique session id for each client based on connection values
void *session_default_generate_id (const void *session_data) {

    // old parameters
    // i32 fd, const struct sockaddr_storage address

    // char *ipstr = sock_ip_to_string ((const struct sockaddr *) &address);
    // u16 port = sock_ip_port ((const struct sockaddr *) &address);

    // if (ipstr && (port > 0)) {
    //     // 24/11/2018 -- 22:14 -- testing a simple id - just ip + port
    //     char *connection_values = c_string_create ("%s-%i", ipstr, port);

    //     uint8_t hash[32];
    //     char hash_string[65];

    //     sha_256_calc (hash, connection_values, strlen (connection_values));
    //     sha_256_hash_to_string (hash_string, hash);

    //     char *retval = c_string_create ("%s", hash_string);

    //     return retval;
    // }

    return NULL;

}