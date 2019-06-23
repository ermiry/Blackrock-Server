#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/packets.h"
#include "cerver/errors.h"
#include "cerver/handler.h"
#include "cerver/sessions.h"
#include "cerver/cerver.h"
#include "cerver/client.h"
#include "cerver/auth.h"

#include "cerver/utils/thpool.h"
#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

static i32 on_hold_poll_get_idx_by_sock_fd (const Cerver *cerver, i32 sock_fd);
static void on_hold_connection_drop (Cerver *cerver, const i32 sock_fd);

static AuthData *auth_data_new (const char *token, void *data, size_t auth_data_size) {

    AuthData *auth_data = (AuthData *) malloc (sizeof (AuthData));
    if (auth_data) {
        auth_data->token = token ? str_new (token) : NULL;
        if (data) {
            auth_data->auth_data = malloc (sizeof (auth_data_size));
            if (auth_data->auth_data) {
                memcpy (auth_data->auth_data, data, auth_data_size);
                auth_data->auth_data_size = auth_data_size;
            }

            else {
                free (auth_data);
                auth_data = NULL;
            }
        }

        else {
            auth_data->auth_data = NULL;
            auth_data->auth_data_size = 0;
        }
    }

    return auth_data;

}

static void auth_data_delete (AuthData *auth_data) {

    if (auth_data) {
        str_delete (auth_data->token);
        if (auth_data->auth_data) free (auth_data->auth_data);
        free (auth_data);
    }

}

Auth *auth_new (void) {

    Auth *auth = (Auth *) malloc (sizeof (Auth));
    if (auth) {
        memset (auth, 0, sizeof (Auth));
        auth->auth_packet = NULL;
        auth->authenticate = NULL;
    }

    return auth;

}

void auth_delete (Auth *auth) {

    if (auth) {
        packet_delete (auth->auth_packet);
        free (auth);
    }

}

// generates an authentication packet with client auth request
Packet *auth_packet_generate (void) { 
    
    return packet_generate_request (AUTH_PACKET, REQ_AUTH_CLIENT, NULL, 0); 
    
}

Client *removeOnHoldClient (Cerver *cerver, Client *client, i32 socket_fd) {

    if (cerver) {
        Client *retval = NULL;

        if (client) {
            // remove the sock fd from the on hold structure
            for (u16 i = 0; i < poll_n_fds; i++) {
                if (cerver->hold_fds[i].fd == socket_fd) {
                    cerver->hold_fds[i].fd = -1;
                    cerver->current_on_hold_nfds--;
                }
            }

            retval = avl_remove_node (cerver->on_hold_clients, client);

            cerver->n_hold_clients--;
        }

        else {
            #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Could not find client associated with socket!");
            #endif

            // this migth be an unusual error
            cerver->n_hold_clients--;
        }

        // if we don't have on hold clients, end on hold tread
        if (cerver->n_hold_clients <= 0) cerver->holding_clients = false;

        #ifdef CERVER_STATS
            cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, 
                c_string_create ("On hold clients: %i.", cerver->n_hold_clients));
        #endif

        return retval;
    }

}

// cerver default client authentication method
// the session id is the same as the token that we requiere to access it
u8 defaultAuthMethod (void *data) {

    if (data) {
        PacketInfo *pack_info = (PacketInfo *) data;

        //check if the cerver supports sessions
        if (pack_info->cerver->useSessions) {
            // check if we recieve a token or auth values
            bool isToken;
            if (pack_info->packetSize > 
                (sizeof (PacketHeader) + sizeof (RequestData) + sizeof (DefAuthData)))
                    isToken = true;

            if (isToken) {
                char *end = pack_info->packetData;
                Token *tokenData = (Token *) (end + 
                    sizeof (PacketHeader) + sizeof (RequestData));

                // verify the token and search for a client with the session ID
                Client *c = getClientBySession (pack_info->cerver->clients, tokenData->token);
                // if we found a client, auth is LOG_SUCCESS
                if (c) {
                    client_set_sessionID (pack_info->client, tokenData->token);
                    return 0;
                } 

                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Wrong session id provided by client!");
                    #endif
                    return 1;      // the session id is wrong -> error!
                } 
            }

            // we recieve auth values, so validate the credentials
            else {
                char *end = pack_info->packetData;
                DefAuthData *authData = (DefAuthData *) (end + 
                    sizeof (PacketHeader) + sizeof (RequestData));

                // credentials are good
                if (authData->code == DEFAULT_AUTH_CODE) {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, 
                        c_string_create ("Default auth: client provided code: %i.", authData->code));
                    #endif

                    // FIXME: 19/april/2019 -- 19:08 -- dont we need to use cerver->generateSessionID?
                    char *sessionID = session_default_generate_id (pack_info->clientSock,
                        pack_info->client->address);

                    if (sessionID) {
                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, 
                            c_string_create ("Generated client session id: %s", sessionID));
                        #endif

                        client_set_sessionID (pack_info->client, sessionID);

                        return 0;
                    }
                    
                    else cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to generate session id!");
                } 
                // wrong credentials
                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                        c_string_create ("Default auth: %i is a wrong autentication code!", 
                        authData->code));
                    #endif
                    return 1;
                }     
            }
        }

        // if not, just check for client credentials
        else {
            DefAuthData *authData = (DefAuthData *) (pack_info->packetData + 
                sizeof (PacketHeader) + sizeof (RequestData));
            
            // credentials are good
            if (authData->code == DEFAULT_AUTH_CODE) {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, 
                    c_string_create ("Default auth: client provided code: %i.", authData->code));
                #endif
                return 0;
            } 
            // wrong credentials
            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                    c_string_create ("Default auth: %i is a wrong autentication code!", 
                    authData->code));
                #endif
            }     
        }
    }

    return 1;

}

// the admin is able to set a ptr to a custom authentication method
// handles the authentication data that the client sent
void authenticateClient (void *data) {

    if (data) {
        PacketInfo *pack_info = (PacketInfo *) data;

        if (pack_info->cerver->auth.authenticate) {
            // we expect the function to return us a 0 on LOG_SUCCESS
            if (!pack_info->cerver->auth.authenticate (pack_info)) {
                cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, "Client authenticated successfully!");

                if (pack_info->cerver->useSessions) {
                    // search for a client with a session id generated by the new credentials
                    Client *found_client = getClientBySession (pack_info->cerver->clients, 
                        pack_info->client->sessionID);

                    // if we found one, register the new connection to him
                    if (found_client) {
                        if (client_registerNewConnection (found_client, pack_info->clientSock))
                            cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to register new connection to client!");

                        i32 idx = getFreePollSpot (pack_info->cerver);
                        if (idx > 0) {
                            pack_info->cerver->fds[idx].fd = pack_info->clientSock;
                            pack_info->cerver->fds[idx].events = POLLIN;
                            pack_info->cerver->nfds++;
                        }
                        
                        // FIXME: how to better handle this error?
                        else cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to get new main poll idx!");

                        client_delete_data (removeOnHoldClient (pack_info->cerver, 
                            pack_info->client, pack_info->clientSock));

                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, 
                            c_string_create ("Registered a new connection to client with session id: %s",
                            pack_info->client->sessionID));
                        printf ("sessionId: %s\n", pack_info->client->sessionID);
                        #endif
                    }

                    // we have a new client
                    else {
                        Client *got_client = removeOnHoldClient (pack_info->cerver, 
                            pack_info->client, pack_info->clientSock);

                        // FIXME: better handle this error!
                        if (!got_client) {
                            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to get client avl node data!");
                            return;
                        }

                        else {
                            #ifdef CERVER_DEBUG
                            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
                                "Got client data when removing an on hold avl node!");
                            #endif
                        }

                        client_registerToServer (pack_info->cerver, got_client, pack_info->clientSock);

                        // send back the session id to the client
                        size_t packet_size = sizeof (PacketHeader) + sizeof (RequestData) + sizeof (Token);
                        void *session_packet = generatePacket (AUTHENTICATION, packet_size);
                        if (session_packet) {
                            char *end = session_packet;
                            RequestData *reqdata = (RequestData *) (end += sizeof (PacketHeader));
                            reqdata->type = CLIENT_AUTH_DATA;

                            Token *tokenData = (Token *) (end += sizeof (RequestData));
                            strcpy (tokenData->token, pack_info->client->sessionID);

                            if (server_sendPacket (pack_info->cerver, 
                                pack_info->clientSock, pack_info->client->address,
                                session_packet, packet_size))
                                    cerver_log_msg (stderr, LOG_ERROR, LOG_PACKET, "Failed to send session token!");

                            free (session_packet);
                        }
                    } 
                }

                // if not, just register the new client to the cerver and use the
                // connection values as the client's id
                else client_registerToServer (pack_info->cerver, pack_info->client,
                    pack_info->clientSock);

                // send a LOG_SUCCESS authentication packet to the client as feedback
                if (pack_info->cerver->type != WEB_SERVER) {
                    size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);
                    void *successPacket = generatePacket (AUTHENTICATION, packetSize);
                    if (successPacket) {
                        char *end = successPacket + sizeof (PacketHeader);
                        RequestData *reqdata = (RequestData *) end;
                        reqdata->type = SUCCESS_AUTH;

                        server_sendPacket (pack_info->cerver, pack_info->clientSock, 
                            pack_info->client->address, successPacket, packetSize);

                        free (successPacket);
                    }
                }
            }

            // failed to authenticate
            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Client failed to authenticate!");
                #endif

                // FIXME: this should only be used when using default authentication
                // sendErrorPacket (pack_info->cerver, pack_info->clientSock, 
                //     pack_info->client->address, ERR_FAILED_AUTH, "Wrong credentials!");

                pack_info->client->authTries--;

                if (pack_info->client->authTries <= 0)
                    dropClient (pack_info->cerver, pack_info->client);
            }
        }

        // no authentication method -- clients are not able to interact to the cerver!
        else {
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Cerver doesn't have an authenticate method!");
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Clients are unable to interact with the cerver!");

            // FIXME: correctly drop client and send error packet to the client!
            dropClient (pack_info->cerver, pack_info->client);
        }
    }

}

// FIXME:
// how to manage a successfull authentication to the cerver
static void auth_success (Packet *packet) {

    if (packet) {

    }

}

// how to manage a failed auth to the cerver
static void auth_failed (Packet *packet) {

    if (packet) {
        // send failed auth packet to client
        Packet *error_packet = error_packet_generate (ERR_FAILED_AUTH, "Failed to authenticate!");
        if (error_packet) {
            packet_set_network_values (error_packet, packet->sock_fd, packet->cerver->protocol);
            packet_send (error_packet, 0);
            packet_delete (error_packet);
        }

        // get the connection associated with the sock fd
        Connection *query = client_connection_new ();
        if (query) {
            query->sock_fd = packet->sock_fd;
            void *connection_data = avl_get_node_data (packet->cerver->on_hold_connections, query);
            if (connection_data) {
                Connection *connection = (Connection *) connection_data;
                connection->auth_tries--;
                if (connection->auth_tries <= 0) {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, 
                    "Connection reached max auth tries, dropping now...");
                    #endif
                    on_hold_connection_drop (packet->cerver, packet->sock_fd);
                }
            }

            else {
                cerver_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, 
                    c_string_create ("Failed to get on hold connection associated with sock: %i", 
                    packet->sock_fd));
                on_hold_connection_drop (packet->cerver, packet->sock_fd);
            }
        }

        else {
            // cerver error allocating memory -- this might not happen
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, 
                c_string_create ("Failed to create connection query in cerver %s.",
                packet->cerver->name));
            #endif

            // send a cerver error packet back to the client
            Packet *error_packet = error_packet_generate (ERR_SERVER_ERROR, "Failed to authenticate!");
            if (error_packet) {
                packet_set_network_values (error_packet, packet->sock_fd, packet->cerver->protocol);
                packet_send (error_packet, 0);
                packet_delete (error_packet);
            }

            // drop the connection as we are unable to authenticate it
            on_hold_connection_drop (packet->cerver, packet->sock_fd);
        }
    }

}

// strip out the auth data from the packet
static AuthData *auth_strip_auth_data (Packet *packet) {

    AuthData *auth_data = NULL;

    if (packet) {
        // check we have a big enough packet
        if (packet->packet_size > sizeof (PacketHeader)) {
            char *end = packet->packet;

            // check if we have a token
            if (packet->packet_size == (sizeof (PacketHeader) + sizeof (SToken))) {
                SToken *s_token = (SToken *) (end += sizeof (PacketHeader));
                auth_data = auth_data_new (s_token->token, NULL, 0);
            }

            // we have custom data credentials
            else {
                end += sizeof (PacketHeader);
                size_t data_size = packet->packet_size - sizeof (PacketHeader);
                auth_data = auth_data_new (NULL, end, data_size);
            }
        }
    }

    return auth_data;

}

// try to authenticate a connection using the values he sent to use
static void auth_try (Packet *packet) {

    if (packet) {
        if (packet->cerver->auth->authenticate) {
            // strip out the auth data from the packet
            AuthData *auth_data = auth_strip_auth_data (packet);
            if (auth_data) {
                // TODO: check if the cerver supports sessions
                if (auth_data->token) {
                    // if we get a token, we search for a client with the same token

                    // if we found a client, register the new connection to him

                    // if not, we have a new client, so we register it to the cerver
                }

                else {
                    // if not, we authenticate using the user defined method
                    if (!packet->cerver->auth->authenticate (auth_data)) {
                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, 
                            c_string_create ("Client authenticated successfully to cerver %s",
                            packet->cerver->name->str));
                        #endif

                        auth_success (packet);
                    }

                    else {
                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stderr, LOG_DEBUG, LOG_CLIENT, 
                            c_string_create ("Client failed to authenticate to cerver %s",
                            packet->cerver->name->str));
                        #endif

                        auth_failed (packet);
                    }
                }
            }

            // failed to get auth data form packet
            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, 
                    c_string_create ("Failed to get auth data from packet in cerver %s",
                    packet->cerver->name->str));
                #endif

                auth_failed (packet);
            }
        }

        // no authentication method -- clients are not able to interact to the cerver!
        else {
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, 
                c_string_create ("Cerver %s does not have an authenticate method!",
                packet->cerver->name->str));

            // close the on hold connection assocaited with sock fd 
            // and remove it from the cerver
            on_hold_connection_drop (packet->cerver, packet->sock_fd);
        }
    }

}

// handle an auth packet
static void cerver_auth_packet_handler (Packet *packet) {

    if (packet) {
        if (packet->packet_size >= (sizeof (PacketHeader) + sizeof (RequestData))) {
            char *end = packet->packet;
            RequestData *req = (RequestData *) (end += sizeof (PacketHeader));

            switch (req->type) {
                // the client sent use its data to authenticate itself
                case CLIENT_AUTH_DATA: auth_try (packet); break;

                default: break;
            }
        }
    }

}

// handles an packet from an on hold connection
void on_hold_packet_handler (void *ptr) {

    if (ptr) {
        Packet *packet = (Packet *) ptr;
        if (!packet_check (packet)) {
            switch (packet->header->packet_type) {
                // handles an error from the client
                case ERROR_PACKET: /* TODO: */ break;

                // handles authentication packets
                case AUTH_PACKET: cerver_auth_packet_handler (packet); break;

                // acknowledge the client we have received his test packet
                case TEST_PACKET: cerver_test_packet_handler (packet); break;

                default:
                    #ifdef CERVER_DEBUG
                    cengine_log_msg (stdout, LOG_WARNING, LOG_PACKET, 
                        c_string_create ("Got an on hold packet of unknown type in cerver %s.", 
                        packet->cerver->name->str));
                    #endif
                    break;
            }
        }

        packet_delete (packet);
    }

}

// reallocs on hold cerver poll fds
// returns 0 on success, 1 on error
static u8 cerver_realloc_on_hold_poll_fds (Cerver *cerver) {

    u8 retval = 1;

    if (cerver) {
        cerver->max_on_hold_connections = cerver->max_on_hold_connections * 2;
        cerver->hold_fds = realloc (cerver->hold_fds, 
            cerver->max_on_hold_connections * sizeof (struct pollfd));
        if (cerver->hold_fds) retval = 0;
    }

    return retval;

}

static i32 on_hold_get_free_idx (Cerver *cerver) {

    if (cerver) {
        for (u32 i = 0; i < cerver->max_on_hold_connections; i++)
            if (cerver->hold_fds[i].fd == -1) return i;
    }

    return -1;

}

static i32 on_hold_poll_get_idx_by_sock_fd (const Cerver *cerver, i32 sock_fd) {

    if (cerver) {
        for (u32 i = 0; i < cerver->max_on_hold_connections; i++)
            if (cerver->hold_fds[i].fd == sock_fd) return i;
    }

    return -1;

}

// we remove any fd that was set to -1 for what ever reason
static void cerver_on_hold_poll_compress (Cerver *cerver) {

    if (cerver) {
        cerver->compress_on_hold = false;

        for (u32 i = 0; i < cerver->max_on_hold_connections; i++) {
            if (cerver->hold_fds[i].fd == -1) {
                for (u32 j = i; j < cerver->max_on_hold_connections - 1; j++) 
                    cerver->hold_fds[j].fd = cerver->hold_fds[j + 1].fd;
                    
            }
        }
    }  

}

// handles packets from the on hold clients until they authenticate
static u8 on_hold_poll (void *ptr) {

    u8 retval = 1;

    if (ptr) {
        Cerver *cerver = (Cerver *) ptr;

        cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, 
            c_string_create ("Cerver %s on hold handler has started!", cerver->name->str));
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Waiting for connections to put on hold...");
        #endif

        int poll_retval = 0;
        while (cerver->isRunning && cerver->holding_connections) {
            poll_retval = poll (cerver->hold_fds, 
                cerver->max_on_hold_connections, cerver->poll_timeout);

            // poll failed
            if (poll_retval < 0) {
                cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, 
                    c_string_create ("Cerver %s on hold poll has failed!", cerver->name->str));
                perror ("Error");
                cerver->holding_connections = false;
                cerver->isRunning = false;
                break;
            }

            // if poll has timed out, just continue to the next loop... 
            if (poll_retval == 0) {
                // #ifdef CERVER_DEBUG
                // cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
                //    c_string_create ("Cerver %s on hold poll timeout", cerver->name->str));
                // #endif
                continue;
            }

            // one or more fd(s) are readable, need to determine which ones they are
            for (u16 i = 0; i < cerver->current_on_hold_nfds; i++) {
                if (cerver->hold_fds[i].revents == 0) continue;
                if (cerver->hold_fds[i].revents != POLLIN) continue;

                if (thpool_add_work (cerver->thpool, cerver_receive, 
                    cerver_receive_new (cerver, cerver->fds[i].fd, false))) {
                    cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                        c_string_create ("Failed to add cerver_receive () to cerver's %s thpool!", 
                        cerver->name->str));
                }
            }
        }

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, 
            c_string_create ("Cerver %s on hold poll has stopped!", cerver->name->str));
        #endif

        retval = 0;
    }

    else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Can't handle on hold clients on a NULL cerver!");

    return retval;

}

// if the cerver requires authentication, we put the connection on hold
// until it has a sucess authentication or it failed to, so it is dropped
// returns 0 on success, 1 on error
u8 on_hold_connection (Cerver *cerver, Connection *connection) {

    u8 retval = 1;

    if (cerver && connection) {
        if (cerver->on_hold_connections) {
            i32 idx = on_hold_get_free_idx (cerver);
            if (idx >= 0) {
                cerver->hold_fds[idx].fd = connection->sock_fd;
                cerver->hold_fds[idx].events = POLLIN;
                cerver->current_on_hold_nfds++; 

                cerver->n_hold_clients++;

                avl_insert_node (cerver->on_hold_connections, connection);

                if (cerver->holding_connections == false) {
                    cerver->holding_connections = true;
                    if (thpool_add_work (cerver->thpool, (void *) on_hold_poll, cerver)) {
                        cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                            c_string_create ("Failed to add on_hold_poll () to cerver's %s thpool!", 
                            cerver->name->str));
                        cerver->holding_connections = false;
                    }
                }

                #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER,
                        c_string_create ("Connection is on hold on cerver %s.",
                        cerver->name->str));
                #endif

                #ifdef CERVER_STATS
                    cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, 
                        c_string_create ("Cerver %s current on hold connections: %i.", 
                        cerver->name->str, cerver->n_hold_clients));
                #endif

                retval = 0;
            }

            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, 
                    c_string_create ("Cerver %s on hold poll is full -- we need to realloc...", 
                    cerver->name->str));
                #endif
                if (cerver_realloc_on_hold_poll_fds (cerver)) {
                    cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                        c_string_create ("Failed to realloc cerver %s on hold poll fds!", 
                        cerver->name->str));
                }

                else retval = on_hold_connection (cerver, connection);
            }
        }
    }

    return retval;

}

// close the on hold connection assocaited with sock fd 
// and remove it from the cerver
static void on_hold_connection_drop (Cerver *cerver, const i32 sock_fd) {

    // remove the connection associated to the sock fd
    Connection *query = client_connection_new ();
    query->sock_fd = sock_fd;
    void *connection_data = avl_remove_node (cerver->on_hold_connections, query);
    if (connection_data) {
        Connection *connection = (Connection *) connection_data;

        // close the connection socket
        client_connection_end (connection);

        // unregister the fd from the on hold structures
        i32 idx = on_hold_poll_get_idx_by_sock_fd (cerver, sock_fd);
        if (idx) {
            cerver->hold_fds[idx].fd = -1;
            cerver->hold_fds[idx].events = -1;
            cerver->current_on_hold_nfds--;

           #ifdef CERVER_STATS
                cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, 
                    c_string_create ("Cerver %s current on hold connections: %i.", 
                    cerver->name->str, cerver->n_hold_clients));
            #endif

            // check if we are holding any more connections, if not, we stop the on hold poll
            if (cerver->current_on_hold_nfds <= 0) {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER,
                    c_string_create ("Stoping cerver's %s on hold poll.",
                    cerver->name->str));
                #endif
                cerver->holding_connections = false;
            }
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, 
                c_string_create ("Couldn't find %i sock fd in cerver's %s on hold poll fds.",
                sock_fd, cerver->name->str));
            #endif
        }

        // we can now safely delete the connection
        client_connection_delete (connection);
    }

    else {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, 
            c_string_create ("Couldn't find a connection associated with %i sock fd in cerver's %s on hold connections tree.",
            sock_fd, cerver->name->str));
        #endif

        close (sock_fd);        // just close the socket
    }

}