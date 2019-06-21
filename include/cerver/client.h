#ifndef _CERVER_CLIENT_H_
#define _CERVER_CLIENT_H_

#include "types/types.h"

#include "network.h"
#include "cerver.h"
#include "client.h"

#include "collections/avl.h"

struct _Server;

#define DEFAULT_CLIENT_MAX_CONNECTS         3

// anyone that connects to the server
struct _Client {

    // 25/11/2018 - 16:00 - using connection values as the client id
    char *clientID;
    struct sockaddr_storage address;

    u8 authTries;           // remaining attemps to authenticate
    bool dropClient;        // client failed to authenticate

    char *sessionID;            // generated by the server for each client
    // a client may need to have multiple connections at the same time...
    u8 n_active_cons;
    u8 curr_max_cons;
    i32 *active_connections;   

};

typedef struct _Client Client;

extern Client *newClient (struct _Server *server, i32 clientSock, struct sockaddr_storage address,
    char *connection_values);
extern void destroyClient (void *data);
extern void client_delete_data (Client *client);

extern int client_comparator_client_id (const void *a, const void *b);
extern int client_comparator_session_id (const void *a, const void *b);

extern u8 client_registerNewConnection (Client *client, i32 socket_fd);
extern u8 client_unregisterConnection (Client *client, i32 socket_fd);

extern char *client_getConnectionValues (i32 fd, const struct sockaddr_storage address);

extern void client_set_sessionID (Client *client, const char *sessionID);

extern Client *getClientBySocket (AVLNode *node, i32 socket_fd);
extern Client *getClientBySession (AVLTree *clients, char *sessionID);

extern void client_registerToServer (struct _Server *server, Client *client, i32);
extern Client *client_unregisterFromServer (struct _Server *server, Client *client);

extern void client_closeConnection (struct _Server *server, Client *client);

// disconnect the client from the server by a socket -- usefull for http servers
extern int client_disconnect_by_socket (struct _Server *server, const int sock_fd);

extern void client_checkTimeouts (struct _Server *server);

#endif