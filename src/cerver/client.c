#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/cerver.h"
#include "cerver/client.h"

#include "cerver/collections/avl.h"

#include "cerver/utils/log.h"
#include "cerver/utils/utils.h"

void client_connection_get_values (Connection *connection);

Connection *client_connection_new (void) {

    Connection *connection = (Connection *) malloc (sizeof (Connection));
    if (connection) {
        memset (connection, 0, sizeof (Connection));
        connection->ip = NULL;
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

// FIXME:
// TODO: create connected time stamp
Client *client_create (const i32 sock_fd, const struct sockaddr_storage address) {

    Client *client = client_new ();
    if (client) {

    }

}

Client *newClient (Cerver *cerver, i32 clientSock, struct sockaddr_storage address,
    char *connection_values) {

    Client *client = NULL;

    // printf ("address ip: %s\n", sock_ip_to_string ((const struct sockaddr *) &address));
    // printf ("client address ip: %s\n", sock_ip_to_string ((const struct sockaddr *) & client->address));

    if (connection_values) {
        if (client->clientID) free (client->clientID);

        client->clientID = c_string_create ("%s", connection_values);
        free (connection_values);
    }

    client->sessionID = NULL;

    if (cerver->authRequired) {
        client->authTries = cerver->auth.maxAuthTries;
        client->dropClient = false;
    } 

    client->n_active_cons = 0;
    if (client->active_connections) 
        for (u8 i = 0; i < DEFAULT_CLIENT_MAX_CONNECTS; i++)
            client->active_connections[i] = -1;

    else {
        client->active_connections = (i32 *) calloc (DEFAULT_CLIENT_MAX_CONNECTS, sizeof (i32));
        for (u8 i = 0; i < DEFAULT_CLIENT_MAX_CONNECTS; i++)
            client->active_connections[i] = -1;
    }

    client->curr_max_cons = DEFAULT_CLIENT_MAX_CONNECTS;
    
    // add the fd to the active connections
    if (client->active_connections) {
        client->active_connections[client->n_active_cons] = clientSock;
        printf ("client->active_connections[%i] = %i\n", client->n_active_cons, clientSock);
        client->n_active_cons++;
    } 

    return client;

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

// the client made a new connection to the cerver
u8 client_registerNewConnection (Client *client, i32 socket_fd) {

    if (client) {
        if (client->active_connections) {
            u8 new_active_cons = client->n_active_cons + 1;

            if (new_active_cons <= client->curr_max_cons) {
                // search for a free spot
                for (int i = 0; i < client->curr_max_cons; i++) {
                    if (client->active_connections[i] == -1) {
                        client->active_connections[i] = socket_fd;
                        printf ("client->active_connections[%i] = %i\n", client->n_active_cons, socket_fd);
                        client->n_active_cons++;
                        printf ("client->n_active_cons: %i\n", client->n_active_cons);
                        return 0;
                    }
                }
            }

            // we need to add more space
            else {
                client->active_connections = (i32 *) realloc (client->active_connections, 
                    client->n_active_cons * sizeof (i32));

                // add the connection at the end
                client->active_connections[new_active_cons] = socket_fd;
                client->n_active_cons++;

                client->curr_max_cons++;
                
                return 0;
            }
        }
    }

    return 1;

}

// removes an active connection from a client
u8 client_unregisterConnection (Client *client, i32 socket_fd) {

    if (client) {
        if (client->active_connections) {
            if (client->active_connections > 0) {
                // search the socket fd
                for (u8 i = 0; i < client->n_active_cons; i++) {
                    if (client->active_connections[i] == socket_fd) {
                        client->active_connections[i] = -1;
                        client->n_active_cons--;
                        return 0;
                    }
                }
            }
        }
    }

    return 1;

}

// FIXME: what is the max number of clients that a cerver can handle?
// registers a NEW client to the cerver
void client_registerToServer (Cerver *cerver, Client *client, i32 newfd) {

    if (cerver && client) {
        // add the new sock fd to the cerver main poll
        i32 idx = getFreePollSpot (cerver);
        if (idx > 0) {
            cerver->fds[idx].fd = newfd;
            cerver->fds[idx].events = POLLIN;
            cerver->nfds++;

            printf ("client_registerToServer () - idx: %i\n", idx);

            // insert the new client into the cerver's clients
            avl_insert_node (cerver->clients, client);

            cerver->connectedClients++;

            #ifdef CERVER_STATS
                cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, 
                c_string_create ("New client registered to cerver. Connected clients: %i.", 
                cerver->connectedClients));
            #endif
        }

        // TODO: how to better handle this error?
        else {
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, 
                "Failed to get a free main poll idx. Is the cerver full?");
            // just drop the client connection
            close (newfd);

            // if it was the only client connection, drop it
            if (client->n_active_cons <= 1) client_closeConnection (cerver, client);
        } 
    }

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