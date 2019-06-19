#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include "cerver/types/types.h"

#include "cerver/cerver.h"
#include "cerver/auth.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

Auth *auth_new (void) {

    Auth *auth = (Auth *) malloc (sizeof (Auth));
    if (auth) {
        memset (auth, 0, sizeof (Auth));
        auth->req_auth_packet = NULL;
        auth->authenticate = NULL;
    }

    return auth;

}

void auth_delete (Auth *auth) {

    if (auth) {
        if (auth->req_auth_packet) free (auth->req_auth_packet);
        free (auth);
    }

}


i32 getOnHoldIdx (Server *server) {

    if (server && server->authRequired) 
        for (u32 i = 0; i < poll_n_fds; i++)
            if (server->hold_fds[i].fd == -1) return i;

    return -1;

}

// we remove any fd that was set to -1 for what ever reason
void compressHoldClients (Server *server) {

    if (server) {
        server->compress_hold_clients = false;
        
        for (u16 i = 0; i < server->hold_nfds; i++) {
            if (server->hold_fds[i].fd == -1) {
                for (u16 j = i; j < server->hold_nfds; j++) 
                    server->hold_fds[j].fd = server->hold_fds[j + 1].fd;

                i--;
                server->hold_nfds--;
            }
        }
    }

}

static void server_recieve (Server *server, i32 fd, bool onHold);

// handles packets from the on hold clients until they authenticate
u8 handleOnHoldClients (void *data) {

    if (!data) {
        cerver_log_msg (stderr, ERROR, SERVER, "Can't handle on hold clients on a NULL server!");
        return 1;
    }

    Server *server = (Server *) data;

    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, SUCCESS, SERVER, "On hold client poll has started!");
    #endif

    int poll_retval;
    while (server->holdingClients) {
        poll_retval = poll (server->hold_fds, poll_n_fds, server->pollTimeout);
        
        // poll failed
        if (poll_retval < 0) {
            cerver_log_msg (stderr, ERROR, SERVER, "On hold poll failed!");
            server->holdingClients = false;
            break;
        }

        // if poll has timed out, just continue to the next loop... 
        if (poll_retval == 0) {
            // #ifdef CERVER_DEBUG
            // cerver_log_msg (stdout, DEBUG_MSG, SERVER, "On hold poll timeout.");
            // #endif
            continue;
        }

        // one or more fd(s) are readable, need to determine which ones they are
        for (u16 i = 0; i < poll_n_fds; i++) {
            if (server->hold_fds[i].revents == 0) continue;
            if (server->hold_fds[i].revents != POLLIN) continue;

            // TODO: maybe later add this directly to the thread pool
            server_recieve (server, server->hold_fds[i].fd, true);
        }
    } 

    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, SERVER, NO_TYPE, "Server on hold poll has stopped!");
    #endif

}

// if the server requires authentication, we send the newly connected clients to an on hold
// structure until they authenticate, if not, they are just dropped by the server
void onHoldClient (Server *server, Client *client, i32 fd) {

    // add the client to the on hold structres -> avl and poll
    if (server && client) {
        if (server->onHoldClients) {
            i32 idx = getOnHoldIdx (server);

            if (idx >= 0) {
                server->hold_fds[idx].fd = fd;
                server->hold_fds[idx].events = POLLIN;
                server->hold_nfds++; 

                server->n_hold_clients++;

                avl_insert_node (server->onHoldClients, client);

                if (server->holdingClients == false) {
                    thpool_add_work (server->thpool, (void *) handleOnHoldClients, server);
                    server->holdingClients = true;
                }          

                #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
                        "Added a new client to the on hold structures.");
                #endif

                #ifdef CERVER_STATS
                    cerver_log_msg (stdout, SERVER, NO_TYPE, 
                        c_string_create ("Current on hold clients: %i.", server->n_hold_clients));
                #endif
            }

            // FIXME: better handle this error!
            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, ERROR, SERVER, "New on hold idx = -1. Is the server full?");
                #endif
            }
        }
    }

}

// FIXME: when do we want to use this?
// FIXME: make available the on hold idx!!
// drops a client from the on hold structure because he was unable to authenticate
void dropClient (Server *server, Client *client) {

    if (server && client) {
        // destroy client should unregister the socket from the client
        // and from the on hold poll structure
        avl_remove_node (server->onHoldClients, client);

        // server->compress_hold_clients = true;

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
            "Client removed from on hold structure. Failed to authenticate");
        #endif
    }   

}

Client *removeOnHoldClient (Server *server, Client *client, i32 socket_fd) {

    if (server) {
        Client *retval = NULL;

        if (client) {
            // remove the sock fd from the on hold structure
            for (u16 i = 0; i < poll_n_fds; i++) {
                if (server->hold_fds[i].fd == socket_fd) {
                    server->hold_fds[i].fd = -1;
                    server->hold_nfds--;
                }
            }

            retval = avl_remove_node (server->onHoldClients, client);

            server->n_hold_clients--;
        }

        else {
            #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, ERROR, CLIENT, "Could not find client associated with socket!");
            #endif

            // this migth be an unusual error
            server->n_hold_clients--;
        }

        // if we don't have on hold clients, end on hold tread
        if (server->n_hold_clients <= 0) server->holdingClients = false;

        #ifdef CERVER_STATS
            cerver_log_msg (stdout, SERVER, NO_TYPE, 
            c_string_create ("On hold clients: %i.", server->n_hold_clients));
        #endif

        return retval;
    }

}

// cerver default client authentication method
// the session id is the same as the token that we requiere to access it
u8 defaultAuthMethod (void *data) {

    if (data) {
        PacketInfo *pack_info = (PacketInfo *) data;

        //check if the server supports sessions
        if (pack_info->server->useSessions) {
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
                Client *c = getClientBySession (pack_info->server->clients, tokenData->token);
                // if we found a client, auth is success
                if (c) {
                    client_set_sessionID (pack_info->client, tokenData->token);
                    return 0;
                } 

                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, ERROR, CLIENT, "Wrong session id provided by client!");
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
                    cerver_log_msg (stdout, DEBUG_MSG, NO_TYPE, 
                        c_string_create ("Default auth: client provided code: %i.", authData->code));
                    #endif

                    // FIXME: 19/april/2019 -- 19:08 -- dont we need to use server->generateSessionID?
                    char *sessionID = session_default_generate_id (pack_info->clientSock,
                        pack_info->client->address);

                    if (sessionID) {
                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stdout, DEBUG_MSG, CLIENT, 
                            c_string_create ("Generated client session id: %s", sessionID));
                        #endif

                        client_set_sessionID (pack_info->client, sessionID);

                        return 0;
                    }
                    
                    else cerver_log_msg (stderr, ERROR, CLIENT, "Failed to generate session id!");
                } 
                // wrong credentials
                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, ERROR, NO_TYPE, 
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
                cerver_log_msg (stdout, DEBUG_MSG, NO_TYPE, 
                    c_string_create ("Default auth: client provided code: %i.", authData->code));
                #endif
                return 0;
            } 
            // wrong credentials
            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, ERROR, NO_TYPE, 
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

        if (pack_info->server->auth.authenticate) {
            // we expect the function to return us a 0 on success
            if (!pack_info->server->auth.authenticate (pack_info)) {
                cerver_log_msg (stdout, SUCCESS, CLIENT, "Client authenticated successfully!");

                if (pack_info->server->useSessions) {
                    // search for a client with a session id generated by the new credentials
                    Client *found_client = getClientBySession (pack_info->server->clients, 
                        pack_info->client->sessionID);

                    // if we found one, register the new connection to him
                    if (found_client) {
                        if (client_registerNewConnection (found_client, pack_info->clientSock))
                            cerver_log_msg (stderr, ERROR, CLIENT, "Failed to register new connection to client!");

                        i32 idx = getFreePollSpot (pack_info->server);
                        if (idx > 0) {
                            pack_info->server->fds[idx].fd = pack_info->clientSock;
                            pack_info->server->fds[idx].events = POLLIN;
                            pack_info->server->nfds++;
                        }
                        
                        // FIXME: how to better handle this error?
                        else cerver_log_msg (stderr, ERROR, NO_TYPE, "Failed to get new main poll idx!");

                        client_delete_data (removeOnHoldClient (pack_info->server, 
                            pack_info->client, pack_info->clientSock));

                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stdout, DEBUG_MSG, CLIENT, 
                            c_string_create ("Registered a new connection to client with session id: %s",
                            pack_info->client->sessionID));
                        printf ("sessionId: %s\n", pack_info->client->sessionID);
                        #endif
                    }

                    // we have a new client
                    else {
                        Client *got_client = removeOnHoldClient (pack_info->server, 
                            pack_info->client, pack_info->clientSock);

                        // FIXME: better handle this error!
                        if (!got_client) {
                            cerver_log_msg (stderr, ERROR, SERVER, "Failed to get client avl node data!");
                            return;
                        }

                        else {
                            #ifdef CERVER_DEBUG
                            cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
                                "Got client data when removing an on hold avl node!");
                            #endif
                        }

                        client_registerToServer (pack_info->server, got_client, pack_info->clientSock);

                        // send back the session id to the client
                        size_t packet_size = sizeof (PacketHeader) + sizeof (RequestData) + sizeof (Token);
                        void *session_packet = generatePacket (AUTHENTICATION, packet_size);
                        if (session_packet) {
                            char *end = session_packet;
                            RequestData *reqdata = (RequestData *) (end += sizeof (PacketHeader));
                            reqdata->type = CLIENT_AUTH_DATA;

                            Token *tokenData = (Token *) (end += sizeof (RequestData));
                            strcpy (tokenData->token, pack_info->client->sessionID);

                            if (server_sendPacket (pack_info->server, 
                                pack_info->clientSock, pack_info->client->address,
                                session_packet, packet_size))
                                    cerver_log_msg (stderr, ERROR, PACKET, "Failed to send session token!");

                            free (session_packet);
                        }
                    } 
                }

                // if not, just register the new client to the server and use the
                // connection values as the client's id
                else client_registerToServer (pack_info->server, pack_info->client,
                    pack_info->clientSock);

                // send a success authentication packet to the client as feedback
                if (pack_info->server->type != WEB_SERVER) {
                    size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);
                    void *successPacket = generatePacket (AUTHENTICATION, packetSize);
                    if (successPacket) {
                        char *end = successPacket + sizeof (PacketHeader);
                        RequestData *reqdata = (RequestData *) end;
                        reqdata->type = SUCCESS_AUTH;

                        server_sendPacket (pack_info->server, pack_info->clientSock, 
                            pack_info->client->address, successPacket, packetSize);

                        free (successPacket);
                    }
                }
            }

            // failed to authenticate
            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, ERROR, CLIENT, "Client failed to authenticate!");
                #endif

                // FIXME: this should only be used when using default authentication
                // sendErrorPacket (pack_info->server, pack_info->clientSock, 
                //     pack_info->client->address, ERR_FAILED_AUTH, "Wrong credentials!");

                pack_info->client->authTries--;

                if (pack_info->client->authTries <= 0)
                    dropClient (pack_info->server, pack_info->client);
            }
        }

        // no authentication method -- clients are not able to interact to the server!
        else {
            cerver_log_msg (stderr, ERROR, SERVER, "Server doesn't have an authenticate method!");
            cerver_log_msg (stderr, ERROR, SERVER, "Clients are unable to interact with the server!");

            // FIXME: correctly drop client and send error packet to the client!
            dropClient (pack_info->server, pack_info->client);
        }
    }

}

// we don't want on hold clients to make any other request
void handleOnHoldPacket (void *data) {

    if (data) {
        PacketInfo *pack_info = (PacketInfo *) data;

        if (!checkPacket (pack_info->packetSize, pack_info->packetData, DONT_CHECK_TYPE))  {
            PacketHeader *header = (PacketHeader *) pack_info->packetData;

            switch (header->packetType) {
                // handles an error from the client
                case ERROR_PACKET: break;

                // a client is trying to authenticate himself
                case AUTHENTICATION: {
                    RequestData *reqdata = (RequestData *) (pack_info->packetData + sizeof (PacketHeader));

                    switch (reqdata->type) {
                        case CLIENT_AUTH_DATA: authenticateClient (pack_info); break;
                        default: break;
                    }
                } break;

                case TEST_PACKET: 
                    cerver_log_msg (stdout, TEST, NO_TYPE, "Got a successful test packet!"); 
                    if (!sendTestPacket (pack_info->server, pack_info->clientSock, pack_info->client->address))
                        cerver_log_msg (stdout, TEST, PACKET, "Success answering the test packet.");

                    else cerver_log_msg (stderr, ERROR, PACKET, "Failed to answer test packet!");
                    break;

                default: 
                    cerver_log_msg (stderr, WARNING, PACKET, "Got a packet of incompatible type."); 
                    break;
            }
        }

        // FIXME: send the packet to the pool
        destroyPacketInfo (pack_info);
    }

}

void cerver_set_auth_method (Server *server, delegate authMethod) {

    if (server) server->auth.authenticate = authMethod;

}