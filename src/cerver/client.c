#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/cerver.h"
#include "cerver/handler.h"
#include "cerver/client.h"

#include "cerver/collections/avl.h"
#include "cerver/collections/dllist.h"

#include "cerver/utils/log.h"
#include "cerver/utils/utils.h"

void client_connection_get_values (Connection *connection);

Connection *client_connection_new (void) {

    Connection *connection = (Connection *) malloc (sizeof (Connection));
    if (connection) {
        memset (connection, 0, sizeof (Connection));
        connection->ip = NULL;
        connection->active = false;
    }

    return connection;

}

// FIXME: pass time stamp
Connection *client_connection_create (const i32 sock_fd, const struct sockaddr_storage address) {

    Connection *connection = client_connection_new ();
    if (connection) {
        memcpy (&connection->address, &address, sizeof (struct sockaddr_storage));
        client_connection_get_values (connection);
    }

}

void client_connection_delete (void *ptr) {

    if (ptr) {
        Connection *connection = (Connection *) ptr;
        str_delete (connection->ip);
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

// registers a new connection to a client
// returns 0 on success, 1 on error
u8 client_connection_register (Client *client, Connection *connection) {

    u8 retval = 1;

    if (client && connection) {
        if (dlist_insert_after (client->connections, dlist_end (client->connections), connection)) {
            connection->active = true;
            retval = 0;
        }

        // failed to register connection
        else {
            client_connection_end (connection);
            client_connection_delete (connection);
        }
    }

    return retval;

}

// unregisters a connection from a client and stops and deletes it
// returns 0 on success, 1 on error
u8 client_unregister_connection (Client *client, Connection *connection) {

    u8 retval = 1;

    if (client && connection) {
        Connection *con = NULL;
        for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
            con = (Connection *) le->data;
            if (connection->sock_fd == con->sock_fd) {
                client_connection_end (connection);
                client_connection_delete (dlist_remove_element (client->connections, le));

                retval = 0;
                break;
            }
        }
    }

    return retval;

}

Client *client_new (void) {

    Client *client = (Client *) malloc (sizeof (Client));
    if (client) {
        memset (client, 0, sizeof (Client));

        client->client_id = NULL;
        client->session_id = NULL;

        client->connections = dlist_init (client_connection_delete, client_connection_comparator);

        client->drop_client = false;

        client->data = NULL;
        client->delete_data = NULL;
    }

    return client;

}

void client_delete (void *ptr) {

    if (ptr) {
        Client *client = (Client *) ptr;

        str_delete (client->client_id);
        str_delete (client->session_id);

        dlist_destroy (client->connections);

        if (client->data) {
            if (client->delete_data) client->delete_data (client->data);
            else free (client->data);
        }

        free (client);
    }

}

// TODO: create connected time stamp
// creates a new client and registers a new connection
Client *client_create (const i32 sock_fd, const struct sockaddr_storage address) {

    Client *client = client_new ();
    if (client) {
        Connection *connection = client_connection_create (sock_fd, address);
        if (connection) client_connection_register (client, connection);
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

// sets client's data and a way to destroy it
void client_set_data (Client *client, void *data, Action delete_data) {

    if (client) {
        client->data = data;
        client->delete_data = delete_data;
    }

}

// compare clients based on their client ids
int client_comparator_client_id (const void *a, const void *b) {

    if (a && b) 
        return str_compare (((Client *) a)->client_id, ((Client *) b)->client_id);

}

// compare clients based on their session ids
int client_comparator_session_id (const void *a, const void *b) {

    if (a && b) 
        return str_compare (((Client *) a)->session_id, ((Client *) b)->session_id);

}

// TODO: realloc cerver main poll if full!!
// registers a client to the cerver
u8 client_register_to_cerver (Cerver *cerver, Client *client) {

    u8 retval = 1;

    if (cerver && client) {
        // register all the client connections to the cerver poll
        Connection *connection = NULL;
        for (ListElement *le = dlist_start (client->connections); le; le = le->next) {
            connection = (Connection *) le->data;

            i32 idx = cerver_poll_get_free_idx (cerver);
            if (idx > 0) {
                cerver->fds[idx].fd = connection->sock_fd;
                cerver->fds[idx].events = POLLIN;
                cerver->current_n_fds++;

                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, 
                    c_string_create ("Added new sock fd to cerver %s main poll, idx: %i", 
                    cerver->name->str, idx));
                #endif
            }

            else {
                // FIXME: realloc cerver poll
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, 
                    c_string_create ("Cerver %s main poll is full!", cerver->name->str));
                #endif
            }
        }

        // register the client to the cerver client's
        avl_insert_node (cerver->clients, client);

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, 
            c_string_create ("Registered a new client to cerver %s.", cerver->name->str));
        #endif
        
        cerver->n_connected_clients++;
        #ifdef CERVER_STATS
        cerver_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, 
            c_string_create ("Connected clients to cerver %s: %i.", 
            cerver->name->str, cerver->n_connected_clients));
        #endif
    }

    return retval;

}

// recursively get the client associated with the socket
Client *getClientBySocket (AVLNode *node, i32 socket_fd) {

    if (node) {
        Client *client = NULL;

        client = getClientBySocket (node->right, socket_fd);

        if (!client) {
            if (node->id) {
                client = (Client *) node->id;
                
                // search the socket fd in the clients active connections
                for (int i = 0; i < client->n_active_cons; i++)
                    if (socket_fd == client->active_connections[i])
                        return client;

            }
        }

        if (!client) client = getClientBySocket (node->left, socket_fd);

        return client;
    }

    return NULL;

}

Client *getClientBySession (AVLTree *clients, char *sessionID) {

    if (clients && sessionID) {
        Client temp;
        temp.sessionID = c_string_create ("%s", sessionID);
        
        void *data = avl_get_node_data (clients, &temp);
        if (data) return (Client *) data;
        else 
            cerver_log_msg (stderr, LOG_WARNING, LOG_CERVER, 
                c_string_create ("Couldn't find a client associated with the session ID: %s.", 
                sessionID));
    }

    return NULL;

}

// removes a client form a cerver's main poll structures
Client *client_unregisterFromServer (Cerver *cerver, Client *client) {

    if (cerver && client) {
        Client *c = avl_remove_node (cerver->clients, client);
        if (c) {
            if (client->active_connections) {
                for (u8 i = 0; i < client->n_active_cons; i++) {
                    for (u8 j = 0; j < poll_n_fds; j++) {
                        if (cerver->fds[j].fd == client->active_connections[i]) {
                            cerver->fds[j].fd = -1;
                            cerver->fds[j].events = -1;
                        }
                    }
                }
            }

            #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Unregistered a client from the sever");
            #endif

            cerver->connectedClients--;

            return c;
        }

        else cerver_log_msg (stdout, LOG_WARNING, LOG_CLIENT, "The client wasn't registered in the cerver.");
    }

    return NULL;

}

// disconnect a client from the cerver and take it out from the cerver's clients and 
void client_closeConnection (Cerver *cerver, Client *client) {

    if (cerver && client) {
        if (client->active_connections) {
            // close all the client's active connections
            for (u8 i = 0; i < client->n_active_cons; i++)
                close (client->active_connections[i]);

            free (client->active_connections);
            client->active_connections = NULL;
        }

        // remove it from the cerver structures
        client_unregisterFromServer (cerver, client);

        #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, 
                c_string_create ("Disconnected a client from the cerver.\
                \nConnected clients remainning: %i.", cerver->connectedClients));
        #endif
    }

}

// disconnect the client from the cerver by a socket -- usefull for http servers
int client_disconnect_by_socket (Cerver *cerver, const int sock_fd) {
    
    int retval = 1;

    if (cerver) {
        Client *c = getClientBySocket (cerver->clients->root, sock_fd);
        if (c) {
            if (c->n_active_cons <= 1) 
                client_closeConnection (cerver, c);

            // FIXME: check for client_CloseConnection retval to be sure!!
            retval  = 0;
        }
            
        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, 
                "Couldn't find an active client with the requested socket!");
            #endif
        }
    }

    return retval;

}

// TODO: used to check for client timeouts in any type of cerver
void client_checkTimeouts (Cerver *cerver) {}