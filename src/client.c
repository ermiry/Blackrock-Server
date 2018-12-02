#include "myTypes.h"

#include "network.h"
#include "cerver.h"
#include "client.h"

#include "utils/avl.h"

#include "utils/log.h"
#include "utils/myUtils.h"

/*** CLIENTS ***/

// FIXME: return the data from the avl when we remove it!!!

#pragma region CLIENTS

// get from where the client is connecting
char *client_getConnectionValues (i32 fd, const struct sockaddr_storage address) {

    char *connectionValues = NULL;

    char *ipstr = sock_ip_to_string ((const struct sockaddr *) &address);
    u16 port = sock_ip_port ((const struct sockaddr *) &address);

    if (ipstr && (port > 0)) 
        connectionValues = createString ("%s-%i", ipstr, port);

    return connectionValues;

}

void client_set_sessionID (Client *client, char *sessionID) {

    if (client && sessionID) {
        if (client->sessionID) free (client->sessionID);
        client->sessionID = sessionID;
    }

}

Client *newClient (Server *server, i32 clientSock, struct sockaddr_storage address,
    char *connection_values) {

    Client *client = NULL;

    client = (Client *) malloc (sizeof (Client));
    client->clientID = NULL;
    client->sessionID = NULL;
    // client->active_connections = NULL;

    // if (server->clientsPool) client = pool_pop (server->clientsPool);

    // if (!client) {
    //     client = (Client *) malloc (sizeof (Client));
    //     client->sessionID = NULL;
    //     client->active_connections = NULL;
    // } 

    // 25/11/2018 - 16:00 - using connection values as the client id
    if (connection_values) {
        if (client->clientID) free (client->clientID);
        client->clientID = createString ("%s", connection_values);
        free (connection_values);
    }

    if (server->authRequired) {
        client->authTries = server->auth.maxAuthTries;
        client->dropClient = false;
    } 

    client->n_active_cons = 0;
    if (client->active_connections) free (client->active_connections);
    client->active_connections = (i32 *) calloc (DEFAULT_CLIENT_MAX_CONNECTS, sizeof (i32));

    // add the fd to the active connections
    if (client->active_connections) {
        client->active_connections[client->n_active_cons] = clientSock;
        client->n_active_cons++;
    } 

    client->address = address;

    return client;

}

// FIXME: unregister connections!!
// deletes a client forever
void destroyClient (void *data) {

    if (data) {
        Client *client = (Client *) data;

        if (client->active_connections) {
            // close all the client's active connections
            for (u8 i = 0; i < client->n_active_cons; i++)
                close (client->active_connections[i]);

            free (client->active_connections);
        }

        if (client->clientID) free (client->clientID);
        if (client->sessionID) free (client->sessionID);

        free (client);
    }

}

// compare clients based on their client ids
int client_comparator_clientID (const void *a, const void *b) {

    if (a && b) {
        Client *client_a = (Client *) a;
        Client *client_b = (Client *) b;

        return strcmp (client_a->clientID, client_b->clientID);
    }

}

// compare clients based on their session ids
int client_comparator_sessionID (const void *a, const void *b) {

    if (a && b) {
        Client *client_a = (Client *) a;
        Client *client_b = (Client *) b;

        return strcmp (client_a->sessionID, client_b->sessionID);
    }

}

// TODO: we can make this more efficient...
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

    if (clients) {
        Client temp;
        temp.sessionID = createString ("%s", sessionID);
        
        void *data = avl_getNodeData (clients, &temp);
        if (data) return (Client *) data;
        else 
            logMsg (stderr, ERROR, SERVER, 
                createString ("Couldn't find a client associated with the session ID: %s.", 
                sessionID));
    }

    return NULL;

}

// FIXME:
// the client made a new connection to the server
void client_registerNewConnection (Client *client, i32 socket_fd) {

    if (client) {
        // if (client->active_connections) {
        //     client->n_active_cons++;
        //     client->active_connections = (i32 *) realloc (client->active_connections, 
        //         client->n_active_cons * sizeof (i32));

        //     // add the connection at the end
        //     client->active_connections[client->n_active_cons - 1] = socket_fd;
        // }

        client->active_connections[client->n_active_cons] = socket_fd;
    }

}

// FIXME: set to -1 the socket_fd place in the correct poll structure!!
void client_unregisterConnection (Client *client, i32 socket_fd) {

    if (client) {
        // if (client->active_connections) {
        //     if (client->active_connections > 0) {
        //         // search the socket fd
        //         u8 i = 0;
        //         for (; i < client->n_active_cons; i++)
        //             if (client->active_connections[i] == socket_fd)
        //                 break;

        //         // found
        //         if (i < client->n_active_cons) {
        //             client->n_active_cons--;

        //             if (client->active_connections > 0 ) {
        //                 client->active_connections = (i32 *) realloc (client->active_connections, 
        //                 client->n_active_cons * sizeof (i32));
        //             }
        //         }
        //     }
        // }
    }

}

void dropClient (Server *server, Client *client);

// FIXME: what is the max number of clients that a server can handle?
// FIXME: use sessions -- also generate a session id for the client
// registers a NEW client to the server
void client_registerToServer (Server *server, Client *client, int newfd) {

    if (server && client) {
        Client *c = NULL;

        // if the client was previously in the on hold structure...
        if (server->authRequired) {
            // FIXME:
            // remove the client from the on hold structures
        }

        else c = client;

        // add the new sock fd to the server main poll
        i32 idx = getFreePollSpot (server);
        if (idx > 0) {
            server->fds[idx].fd = newfd;
            server->fds[idx].events = POLLIN;
            server->nfds++;

            // insert the new client into the server's clients
            avl_insertNode (server->clients, c);

            server->connectedClients++;

            #ifdef CERVER_STATS
                logMsg (stdout, SERVER, NO_TYPE, 
                createString ("New client registered to server. Connected clients: %i.", 
                server->connectedClients));
            #endif
        }

        // TODO: how to better handle this error?
        else {
            logMsg (stderr, ERROR, SERVER, 
                "Failed to get a free main poll idx. Is the server full?");
            // just drop the client...
            // FIXME:
        } 
    }

}

// FIXME:
// removes a client form a server's main poll structures
void client_unregisterFromServer (Server *server, Client *client)  {

    if (server && client) {
        // server->fds[client->clientID].fd = -1;
        avl_removeNode (server->clients, client);
    
        #ifdef CERVER_DEBUG
            logMsg (stdout, DEBUG_MSG, SERVER, "Unregistered a client from the sever");
        #endif
    }

}

// FIXME:
// disconnect a client from the server and take it out from the server's clients and 
void client_closeConnection (Server *server, Client *client) {

    if (server && client) {
        if (client->active_connections) {
            // close all the client's active connections
            for (u8 i = 0; i < client->n_active_cons; i++)
                close (client->active_connections[i]);

            free (client->active_connections);
            // client->active_connections = NULL;
        }

        // remove it from the server structures
        client_unregisterFromServer (server, client);

        server->connectedClients--;

        #ifdef CERVER_DEBUG
            logMsg (stdout, DEBUG_MSG, CLIENT, 
                createString ("Disconnected a client from the server.\
                \nConnected clients remainning: %i.", server->connectedClients));
        #endif
    }

}

// TODO: used to check for client timeouts in any type of server
void client_checkTimeouts (Server *server) {}

#pragma endregion