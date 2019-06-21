#ifndef _CERVER_CLIENT_H_
#define _CERVER_CLIENT_H_

#include <time.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "network.h"
#include "cerver.h"
#include "client.h"

#include "collections/avl.h"

struct _Cerver;

#define DEFAULT_CLIENT_MAX_CONNECTS         3

// anyone that connects to the cerver
struct _Client {

    // generated using connection values
    String *client_id;

    // connection values
    u16 port;
    String *ip;
    struct sockaddr_storage address;
    time_t connected_time_stamp;

    String *session_id;

    // FIXME: 20/06/19 --- how can we better manage these?
    // a client may need to have multiple connections at the same time...
    // client connections associated with the same session id
    u8 n_active_cons;
    u8 curr_max_cons;
    i32 *active_connections;

    // authentication
    u8 auth_tries;           // remaining attemps to authenticate
    bool drop_client;        // client failed to authenticate

};

typedef struct _Client Client;

extern Client *client_new (void);
extern void client_delete (void *ptr);

// get from where the client is connecting
extern void client_get_connection_values (Client *client);

// sets the client's session id
extern void client_set_session_id (Client *client, const char *session_id);

// compare clients based on their client ids
extern int client_comparator_client_id (const void *a, const void *b);

// compare clients based on their session ids
extern int client_comparator_session_id (const void *a, const void *b);


extern Client *newClient (struct _Cerver *cerver, i32 clientSock, struct sockaddr_storage address,
    char *connection_values);
extern void destroyClient (void *data);
extern void client_delete_data (Client *client);

extern u8 client_registerNewConnection (Client *client, i32 socket_fd);
extern u8 client_unregisterConnection (Client *client, i32 socket_fd);

extern char *client_getConnectionValues (i32 fd, const struct sockaddr_storage address);

extern void client_set_sessionID (Client *client, const char *sessionID);

extern Client *getClientBySocket (AVLNode *node, i32 socket_fd);
extern Client *getClientBySession (AVLTree *clients, char *sessionID);

extern void client_registerToServer (struct _Cerver *cerver, Client *client, i32);
extern Client *client_unregisterFromServer (struct _Cerver *cerver, Client *client);

extern void client_closeConnection (struct _Cerver *cerver, Client *client);

// disconnect the client from the cerver by a socket -- usefull for http servers
extern int client_disconnect_by_socket (struct _Cerver *cerver, const int sock_fd);

extern void client_checkTimeouts (struct _Cerver *cerver);

#endif