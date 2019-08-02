#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <time.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/cerver.h"
#include "cerver/auth.h"
#include "cerver/handler.h"
#include "cerver/client.h"
#include "cerver/packets.h"

#include "cerver/collections/avl.h"
#include "cerver/collections/dllist.h"

#include "cerver/utils/log.h"
#include "cerver/utils/utils.h"

static u64 next_client_id = 0;

void client_connection_end (Connection *connection);
void client_connection_get_values (Connection *connection);

#pragma region Client Connection

static ConnectionStats *connection_stats_new (void) {

    ConnectionStats *stats = (ConnectionStats *) malloc (sizeof (ConnectionStats));
    if (stats) {
        memset (stats, 0, sizeof (ConnectionStats));
        stats->received_packets = packets_per_type_new ();
        stats->sent_packets = packets_per_type_new ();
    } 

    return stats;

}

static inline void connection_stats_delete (ConnectionStats *stats) { 
    
    if (stats) {
        packets_per_type_delete (stats->received_packets);
        packets_per_type_delete (stats->sent_packets);

        free (stats); 
    } 
    
}

Connection *client_connection_new (void) {

    Connection *connection = (Connection *) malloc (sizeof (Connection));
    if (connection) {
        memset (connection, 0, sizeof (Connection));
        connection->ip = NULL;
        connection->active = false;
        connection->auth_tries = DEFAULT_AUTH_TRIES;
        connection->stats = NULL;   
    }

    return connection;

}

Connection *client_connection_create (const i32 sock_fd, const struct sockaddr_storage address,
    Protocol protocol) {

    Connection *connection = client_connection_new ();
    if (connection) {
        connection->sock_fd = sock_fd;
        time (&connection->timestamp);
        memcpy (&connection->address, &address, sizeof (struct sockaddr_storage));
        client_connection_get_values (connection);
        connection->protocol = protocol;
        connection->stats = connection_stats_new ();
    }

    return connection;

}

void client_connection_delete (void *ptr) {

    if (ptr) {
        Connection *connection = (Connection *) ptr;

        if (connection->active) client_connection_end (connection);

        str_delete (connection->ip);
        connection_stats_delete (connection->stats);

        free (connection);
    }

}

// compare two connections by their socket fds
int client_connection_comparator (const void *a, const void *b) {

    if (a && b) {
        Connection *con_a = (Connection *) a;
        Connection *con_b = (Connection *) b;

        if (con_a->sock_fd < con_b->sock_fd) return -1;
        else if (con_a->sock_fd == con_b->sock_fd) return 0;
        else return 1; 
    }

    return 0;

}

// get from where the client is connecting
void client_connection_get_values (Connection *connection) {

    if (connection) {
        connection->ip = str_new (sock_ip_to_string ((const struct sockaddr *) &connection->address));
        connection->port = sock_ip_port ((const struct sockaddr *) &connection->address);
    }

}

// ends a client connection
void client_connection_end (Connection *connection) {

    if (connection) {
        close (connection->sock_fd);
        connection->sock_fd = -1;
        connection->active = false;
    }

}

// gets the connection from the on hold connections map in cerver
Connection *client_connection_get_by_sock_fd_from_on_hold (Cerver *cerver, i32 sock_fd) {

    Connection *connection = NULL;

    if (cerver) {
        const i32 *key = &sock_fd;
        void *connection_data = htab_get_data (cerver->on_hold_connection_sock_fd_map,
            key, sizeof (i32));
        if (connection_data) connection = (Connection *) connection_data;
    }

    return connection;

}

// gets the connection from the client by its sock fd
Connection *client_connection_get_by_sock_fd_from_client (Client *client, i32 sock_fd) {

    if (client) {
        Connection *query = client_connection_new ();
        if (query) {
            query->sock_fd = sock_fd;
            void *connection_data = dlist_search (client->connections, query);
            client_connection_delete (query);
            return (connection_data ? (Connection *) connection_data : NULL);
        }
    }

    return NULL;

}

// checks if the connection belongs to the client
bool client_connection_check_owner (Client *client, Connection *connection) {

    if (client && connection) {
        for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
            if (connection->sock_fd == ((Connection *) le->data)->sock_fd) return true;
        }
    }

    return false;

}

// registers a new connection to a client without adding it to the cerver poll
// returns 0 on success, 1 on error
u8 client_connection_register_to_client (Client *client, Connection *connection) {

    u8 retval = 1;

    if (client && connection) {
        if (dlist_insert_after (client->connections, dlist_end (client->connections), connection)) {
            #ifdef CERVER_DEBUG
            if (client->session_id) {
                cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, 
                    c_string_create ("Registered a new connection to client with session id: %s",
                    client->session_id->str));
            }
            #endif

            retval = 0;
        }

        // for wahtever reason failed to register the connection to the client
        // we close the connection, as it can not be a free connection
        else {
            client_connection_end (connection);
            client_connection_delete (connection);
        }
    }

    return retval;

}

// unregisters a connection from a client, if the connection is active, it is stopped and removed 
// from the cerver poll as there can't be a free connection withput client
// returns 0 on success, 1 on error
u8 client_connection_unregister_from_client (Cerver *cerver, Client *client, Connection *connection) {

    u8 retval = 1;

    if (client && connection) {
        if (client->connections->size > 1) {
            Connection *con = NULL;
            for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
                con = (Connection *) le->data;
                if (connection->sock_fd == con->sock_fd) {
                    // unmap the client connection from the cerver poll
                    cerver_poll_unregister_connection (cerver, client, connection);

                    client_connection_end (connection);
                    client_connection_delete (dlist_remove_element (client->connections, le));

                    retval = 0;
                    
                    break;
                }
            }
        }

        else {
            cerver_poll_unregister_connection (cerver, client, connection);

            client_connection_end (connection);
            client_connection_delete (dlist_remove_element (client->connections, NULL));

            retval = 0;
        }
    }

    return retval;

}

// wrapper function for easy access
// registers a client connection to the cerver and maps the sock fd to the client
u8 client_connection_register_to_cerver (Cerver *cerver, Client *client, Connection *connection) {

    if (!cerver_poll_register_connection (cerver, client, connection)) {
        connection->active = true;
        return 0;
    }

    return 1;

}

// wrapper function for easy access
// unregisters a client connection from the cerver and unmaps the sock fd from the client
u8 client_connection_unregister_from_cerver (Cerver *cerver, Client *client, Connection *connection) {

    if (!cerver_poll_unregister_connection (cerver, client, connection)) {
        connection->active = false;
        return 0;
    }

    return 1;

}

#pragma endregion

#pragma region Client

static ClientStats *client_stats_new (void) {

    ClientStats *client_stats = (ClientStats *) malloc (sizeof (ClientStats));
    if (client_stats) {
        memset (client_stats, 0, sizeof (ClientStats));
        client_stats->received_packets = packets_per_type_new ();
        client_stats->sent_packets = packets_per_type_new ();
    } 

    return client_stats;

}

static inline void client_stats_delete (ClientStats *client_stats) { 
    
    if (client_stats) {
        packets_per_type_delete (client_stats->received_packets);
        packets_per_type_delete (client_stats->sent_packets);

        free (client_stats); 
    } 
    
}

Client *client_new (void) {

    Client *client = (Client *) malloc (sizeof (Client));
    if (client) {
        memset (client, 0, sizeof (Client));

        client->session_id = NULL;

        client->connections = NULL;

        client->drop_client = false;

        client->data = NULL;
        client->delete_data = NULL;

        client->stats = NULL;   
    }

    return client;

}

void client_delete (void *ptr) {

    if (ptr) {
        Client *client = (Client *) ptr;

        str_delete (client->session_id);

        dlist_destroy (client->connections);

        if (client->data) {
            if (client->delete_data) client->delete_data (client->data);
            else free (client->data);
        }

        client_stats_delete (client->stats);

        free (client);
    }

}

// creates a new client and inits its values
Client *client_create (void) {

    Client *client = client_new ();
    if (client) {
        // init client values
        client->id = next_client_id;
        next_client_id += 1;
        time (&client->connected_timestamp);
        client->connections = dlist_init (client_connection_delete, client_connection_comparator);
        client->stats = client_stats_new ();
    }

    return client;

}

// creates a new client and registers a new connection
Client *client_create_with_connection (Cerver *cerver, 
    const i32 sock_fd, const struct sockaddr_storage address) {

    Client *client = client_create ();
    if (client) {
        Connection *connection = client_connection_create (sock_fd, address, cerver->protocol);
        if (connection) client_connection_register_to_client (client, connection);
        else {
            // failed to create a new connection
            client_delete (client);
            client = NULL;
        }
    }

    return client;

}

// sets the client's session id
void client_set_session_id (Client *client, const char *session_id) {

    if (client) {
        if (client->session_id) str_delete (client->session_id);
        client->session_id = str_new (session_id);
    }

}

// returns the client's app data
void *client_get_data (Client *client) { return (client ? client->data : NULL); }

// sets client's data and a way to destroy it
// deletes the previous data of the client
void client_set_data (Client *client, void *data, Action delete_data) {

    if (client) {
        if (client->data) {
            if (client->delete_data) client->delete_data (client->data);
            else free (client->data);
        }

        client->data = data;
        client->delete_data = delete_data;
    }

}

// compare clients based on their client ids
int client_comparator_client_id (const void *a, const void *b) {

    if (a && b) {
        Client *client_a = (Client *) a;
        Client *client_b = (Client *) b;

        if (client_a->id < client_b->id) return -1;
        else if (client_a->id == client_b->id) return 0;
        else return 1;
    }

    return 0;

}

// compare clients based on their session ids
int client_comparator_session_id (const void *a, const void *b) {

    if (a && b) return str_compare (((Client *) a)->session_id, ((Client *) b)->session_id);
    return 0;

}

// closes all client connections
u8 client_disconnect (Client *client) {

    if (client) {
        Connection *connection = NULL;
        for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
            connection = (Connection *) le->data;
            client_connection_end (connection);
        }

        return 0;
    }

    return 1;

}

// drops a client form the cerver
// unregisters the client from the cerver and the deletes him
void client_drop (Cerver *cerver, Client *client) {

    if (cerver && client) {
        client_unregister_from_cerver (cerver, client);
        client_delete (client);
    }

}

// removes the connection from the client
// and also checks if there is another active connection in the client, if not it will be dropped
// returns 0 on success, 1 on error
u8 client_remove_connection (Cerver *cerver, Client *client, Connection *connection) {

    u8 retval = 1;

    if (cerver && client && connection) {
        // check if the connection actually belongs to the client
        if (client_connection_check_owner (client, connection)) {
            client_connection_unregister_from_client (cerver, client, connection);

            // if the client does not have any active connection, drop it
            if (client->connections->size <= 0) client_drop (cerver, client);

            retval = 0;
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_CLIENT, 
                c_string_create ("client_remove_connection () - Client with id" 
                "%ld does not have a connection related to sock fd %d",
                client->id, connection->sock_fd));
            #endif
        }
    }

    return retval;

}

// removes the connection from the client referred to by the sock fd
// and also checks if there is another active connection in the client, if not it will be dropped
// returns 0 on success, 1 on error
u8 client_remove_connection_by_sock_fd (Cerver *cerver, Client *client, i32 sock_fd) {

    u8 retval = 1;

    if (cerver && client) {
        // remove the connection from the cerver and from the client
        Connection *connection = client_connection_get_by_sock_fd_from_client (client, sock_fd);
        if (connection) {
            client_connection_unregister_from_client (cerver, client, connection);

            // if the client does not have any active connection, drop it
            if (client->connections->size <= 0) client_drop (cerver, client);

            retval = 0;
        }

        else {
            // the connection may not belong to this client
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_CLIENT, 
                c_string_create ("client_remove_connection_by_sock_fd () - Client with id" 
                "%ld does not have a connection related to sock fd %d",
                client->id, sock_fd));
            #endif
        }
    }

    return retval;

}

// register all the active connections from a client to a cerver
// returns 0 on success, 1 if one or more connections failed to register
u8 client_register_connections_to_cerver (Cerver *cerver, Client *client) {

    u8 retval = 1;

    if (cerver && client) {
        u8 n_failed = 0;          // n connections that failed to be registered

        // register all the client connections to the cerver poll
        Connection *connection = NULL;
        for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
            connection = (Connection *) le->data;
            if (client_connection_register_to_cerver (cerver, client, connection))
                n_failed++;
        }

        // check how many connections have failed
        if (n_failed == client->connections->size) {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, 
                c_string_create ("Failed to register all the connections for client %ld (id) to cerver %s",
                client->id, cerver->info->name->str));
            #endif

            client_drop (cerver, client);       // drop the client ---> no active connections
        }

        else retval = 0;        // at least one connection is active
    }

    return retval;

}

// registers a client to the cerver --> add it to cerver's structures
// registers all of the current active client connections to the cerver poll
// returns 0 on success, 1 on error
u8 client_register_to_cerver (Cerver *cerver, Client *client) {

    u8 retval = 1;

    if (cerver && client) {
        if (!client_register_connections_to_cerver (cerver, client)) {
            // register the client to the cerver client's
            avl_insert_node (cerver->clients, client);

            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, 
                c_string_create ("Registered a new client to cerver %s.", cerver->info->name->str));
            #endif
            
            cerver->stats->total_n_clients++;
            cerver->stats->current_n_connected_clients++;
            #ifdef CERVER_STATS
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
                c_string_create ("Connected clients to cerver %s: %i.", 
                cerver->info->name->str, cerver->stats->current_n_connected_clients));
            #endif

            retval = 0;
        }
    }

    return retval;

}

// unregisters a client from a cerver -- removes it from cerver's structures
Client *client_unregister_from_cerver (Cerver *cerver, Client *client) {

    Client *retval = NULL;

    if (cerver && client) {
        // unregister all the client connections from the cerver
        Connection *connection = NULL;
        for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
            connection = (Connection *) le->data;
            cerver_poll_unregister_connection (cerver, client, connection);
        }

        // remove the client from the cerver's clients
        void *client_data = avl_remove_node (cerver->clients, client);
        if (client_data) {
            retval = (Client *) client_data;

            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, 
                c_string_create ("Unregistered a client from cerver %s.", cerver->info->name->str));
            #endif

            cerver->stats->current_n_connected_clients--;
            #ifdef CERVER_STATS
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
                c_string_create ("Connected clients to cerver %s: %i.", 
                cerver->info->name->str, cerver->stats->current_n_connected_clients));
            #endif
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER,
                c_string_create ("Received NULL ptr when attempting to remove a client from cerver's %s client tree.", 
                cerver->info->name->str));
            #endif
        }
    }

    return retval;

}

// gets the client associated with a sock fd using the client-sock fd map
Client *client_get_by_sock_fd (Cerver *cerver, i32 sock_fd) {

    Client *client = NULL;

    if (cerver) {
        const i32 *key = &sock_fd;
        void *client_data = htab_get_data (cerver->client_sock_fd_map, 
            key, sizeof (i32));
        if (client_data) client = (Client *) client_data;
    }

    return client;

}

// searches the avl tree to get the client associated with the session id
// the cerver must support sessions
Client *client_get_by_session_id (Cerver *cerver, char *session_id) {

    Client *client = NULL;

    if (session_id) {
        // create our search query
        Client *client_query = client_new ();
        if (client_query) {
            client_set_session_id (client_query, session_id);

            void *data = avl_get_node_data (cerver->clients, client_query);
            if (data) client = (Client *) data;     // found

            client_delete (client_query);
        }
    }

    return client;

}

#pragma endregion