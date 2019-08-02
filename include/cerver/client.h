#ifndef _CERVER_CLIENT_H_
#define _CERVER_CLIENT_H_

#include <time.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/cerver.h"
#include "cerver/packets.h"

#include "cerver/collections/avl.h"
#include "cerver/collections/dllist.h"

struct _Cerver;
struct _Client;
struct _PacketsPerType;

struct _ConnectionStats {
    
    time_t connection_threshold_time;       // every time we want to reset the connection's stats
    u64 total_bytes_received;               // total amount of bytes received from this connection
    u64 total_bytes_sent;                   // total amount of bytes that have been sent to the connection
    u64 n_packets_received;                 // total number of packets received from this connection (packet header + data)
    u64 n_packets_sent;                     // total number of packets sent to this connection

    struct _PacketsPerType *received_packets;
    struct _PacketsPerType *sent_packets;

};

typedef struct _ConnectionStats ConnectionStats;

// a connection from a client
struct _Connection {

    // connection values
    i32 sock_fd;
    u16 port;
    String *ip;
    struct sockaddr_storage address;
    Protocol protocol;

    time_t timestamp;                       // connected timestamp

    // authentication
    u8 auth_tries;                          // remaining attemps to authenticate

    bool active;

    ConnectionStats *stats;

};

typedef struct _Connection Connection;

extern Connection *client_connection_new (void);
extern void client_connection_delete (void *ptr);

// creates a new lcient connection with the specified values
extern Connection *client_connection_create (const i32 sock_fd, const struct sockaddr_storage address,
    Protocol protocol);

// compare two connections by their socket fds
extern int client_connection_comparator (const void *a, const void *b);

// get from where the client is connecting
extern void client_connection_get_values (Connection *connection);

// ends a client connection
extern void client_connection_end (Connection *connection);

// gets the connection from the on hold connections map in cerver
extern Connection *client_connection_get_by_sock_fd_from_on_hold (struct _Cerver *cerver, i32 sock_fd);

// gets the connection from the client by its sock fd
extern Connection *client_connection_get_by_sock_fd_from_client (struct _Client *client, i32 sock_fd);

// checks if the connection belongs to the client
extern bool client_connection_check_owner (struct _Client *client, Connection *connection);

// registers a new connection to a client without adding it to the cerver poll
// returns 0 on success, 1 on error
extern u8 client_connection_register_to_client (struct _Client *client, Connection *connection);

// unregisters a connection from a client, if the connection is active, it is stopped and removed 
// from the cerver poll as there can't be a free connection withput client
// returns 0 on success, 1 on error
extern u8 client_connection_unregister_from_client (struct _Cerver *cerver, 
    struct _Client *client, Connection *connection);

// wrapper function for easy access
// registers a client connection to the cerver and maps the sock fd to the client
extern u8 client_connection_register_to_cerver (struct _Cerver *cerver, 
    struct _Client *client, Connection *connection);

// wrapper function for easy access
// unregisters a client connection from the cerver and unmaps the sock fd from the client
extern u8 client_connection_unregister_from_cerver (struct _Cerver *cerver, 
    struct _Client *client, Connection *connection);

struct _ClientStats {

    time_t client_threshold_time;           // every time we want to reset the client's stats
    u64 total_bytes_received;               // total amount of bytes received from this client
    u64 total_bytes_sent;                   // total amount of bytes that have been sent to the client (all of its connections)
    u64 n_packets_received;                 // total number of packets received from this client (packet header + data)
    u64 n_packets_send;                     // total number of packets sent to this client (all connections)

    struct _PacketsPerType *received_packets;
    struct _PacketsPerType *sent_packets;

};

typedef struct _ClientStats ClientStats;

// anyone that connects to the cerver
struct _Client {

    // generated using connection values
    u64 id;
    time_t connected_timestamp;

    DoubleList *connections;

    // multiple connections can be associated with the same client using the same session id
    String *session_id;

    bool drop_client;        // client failed to authenticate

    void *data;
    Action delete_data;

    ClientStats *stats;

};

typedef struct _Client Client;

extern Client *client_new (void);
extern void client_delete (void *ptr);

// creates a new client and inits its values
extern Client *client_create (void);

// creates a new client and registers a new connection
extern Client *client_create_with_connection (struct _Cerver *cerver, 
    const i32 sock_fd, const struct sockaddr_storage address);

// sets the client's session id
extern void client_set_session_id (Client *client, const char *session_id);

// returns the client's app data
extern void *client_get_data (Client *client);

// sets client's data and a way to destroy it
// deletes the previous data of the client
extern void client_set_data (Client *client, void *data, Action delete_data);

// compare clients based on their client ids
extern int client_comparator_client_id (const void *a, const void *b);

// compare clients based on their session ids
extern int client_comparator_session_id (const void *a, const void *b);

// closes all client connections
// returns 0 on success, 1 on error
extern u8 client_disconnect (Client *client);

// drops a client form the cerver
// unregisters the client from the cerver and the deletes him
extern void client_drop (struct _Cerver *cerver, Client *client);

// removes the connection from the client
// and also checks if there is another active connection in the client, if not it will be dropped
// returns 0 on success, 1 on error
extern u8 client_remove_connection (struct _Cerver *cerver, Client *client, Connection *connection);

// removes the connection from the client referred to by the sock fd
// and also checks if there is another active connection in the client, if not it will be dropped
// returns 0 on success, 1 on error
extern u8 client_remove_connection_by_sock_fd (struct _Cerver *cerver, Client *client, i32 sock_fd);

// register all the active connections from a client to a cerver
// returns 0 on success, 1 if one or more connections failed to register
extern u8 client_register_connections_to_cerver (struct _Cerver *cerver, Client *client);

// registers a client to the cerver --> add it to cerver's structures
// returns 0 on success, 1 on error
extern u8 client_register_to_cerver (struct _Cerver *cerver, Client *client);

// unregisters a client from a cerver -- removes it from cerver's structures
extern Client *client_unregister_from_cerver (struct _Cerver *cerver, Client *client);

// gets the client associated with a sock fd using the client-sock fd map
extern Client *client_get_by_sock_fd (struct _Cerver *cerver, i32 sock_fd);

// searches the avl tree to get the client associated with the session id
// the cerver must support sessions
extern Client *client_get_by_session_id (struct _Cerver *cerver, char *session_id);

#endif