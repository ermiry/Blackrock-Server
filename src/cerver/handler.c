#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cerver/types/types.h"

#include "cerver/cerver.h"
#include "cerver/handler.h"
#include "cerver/auth.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

static RecvdBufferData *rcvd_buffer_data_new (Cerver *server, i32 socket_fd, char *packet_buffer, size_t total_size, bool on_hold) {

    RecvdBufferData *data = (RecvdBufferData *) malloc (sizeof (RecvdBufferData));
    if (data) {
        data->server = server;
        data->sock_fd = socket_fd;

        data->buffer = (char *) calloc (total_size, sizeof (char));
        memcpy (data->buffer, packet_buffer, total_size);
        data->total_size = total_size;

        data->onHold = on_hold;
    }

    return data;

}

void rcvd_buffer_data_delete (RecvdBufferData *data) {

    if (data) {
        if (data->buffer) free (data->buffer);
        free (data);
    }

}

i32 getFreePollSpot (Cerver *server) {

    if (server) 
        for (u32 i = 0; i < poll_n_fds; i++)
            if (server->fds[i].fd == -1) return i;

    return -1;

}

// we remove any fd that was set to -1 for what ever reason
static void compressClients (Cerver *server) {

    if (server) {
        server->compress_clients = false;

        for (u16 i = 0; i < server->nfds; i++) {
            if (server->fds[i].fd == -1) {
                for (u16 j = i; j < server->nfds; j++) 
                    server->fds[j].fd = server->fds[j + 1].fd;

                i--;
                server->nfds--;
            }
        }
    }  

}

// called with the th pool to handle a new packet
void handlePacket (void *data) {

    if (data) {
        PacketInfo *packet = (PacketInfo *) data;

        if (!checkPacket (packet->packetSize, packet->packetData, DONT_CHECK_TYPE))  {
            PacketHeader *header = (PacketHeader *) packet->packetData;

            switch (header->packetType) {
                case CLIENT_PACKET: {
                    RequestData *reqdata = (RequestData *) (packet->packetData + sizeof (PacketHeader));
                    
                    switch (reqdata->type) {
                        /* case CLIENT_DISCONNET: 
                            cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, "Ending client connection - client_disconnect ()");
                            client_closeConnection (packet->server, packet->client); 
                            break; */
                        default: break;
                    }
                }

                // handles an error from the client
                case ERROR_PACKET: break;

                // a client is trying to authenticate himself
                case AUTHENTICATION: break;

                // handles a request made from the client
                case REQUEST: break;

                // FIXME:
                // handle a game packet sent from a client
                // case GAME_PACKET: gs_handlePacket (packet); break;

                case TEST_PACKET: 
                    cerver_log_msg (stdout, LOG_TEST, LOG_NO_TYPE, "Got a successful test packet!"); 
                    // send a test packet back to the client
                    if (!sendTestPacket (packet->server, packet->clientSock, packet->client->address))
                        cerver_log_msg (stdout, LOG_DEBUG, LOG_PACKET, "LOG_SUCCESS answering the test packet.");

                    else cerver_log_msg (stderr, LOG_ERROR, LOG_PACKET, "Failed to answer test packet!");
                    break;

                default: 
                    cerver_log_msg (stderr, LOG_WARNING, LOG_PACKET, "Got a packet of incompatible type."); 
                    break;
            }
        }

        // FIXME: 22/11/2018 - error with packet pool!!! seg fault always the second time 
        // a client sends a packet!!
        // no matter what, we send the packet to the pool after using it
        // pool_push (packet->server->packetPool, data);
        destroyPacketInfo (packet);
    }

}

// split the entry buffer in packets of the correct size
void default_handle_recieved_buffer (void *rcvd_buffer_data) {

    if (rcvd_buffer_data) {
        RecvdBufferData *data = (RecvdBufferData *) rcvd_buffer_data;

        if (data->buffer && (data->total_size > 0)) {
            u32 buffer_idx = 0;
            char *end = data->buffer;

            PacketHeader *header = NULL;
            u32 packet_size;
            char *packet_data = NULL;

            PacketInfo *info = NULL;

            while (buffer_idx < data->total_size) {
                header = (PacketHeader *) end;

                // check the packet size
                packet_size = header->packetSize;
                if (packet_size > 0) {
                    // copy the content of the packet from the buffer
                    packet_data = (char *) calloc (packet_size, sizeof (char));
                    for (u32 i = 0; i < packet_size; i++, buffer_idx++) 
                        packet_data[i] = data->buffer[buffer_idx];

                    Client *c = data->onHold ? getClientBySocket (data->server->onHoldClients->root, data->sock_fd) :
                        getClientBySocket (data->server->clients->root, data->sock_fd);

                    if (!c) {
                        cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to get client by socket!");
                        return;
                    }

                    info = newPacketInfo (data->server, c, data->sock_fd, packet_data, packet_size);

                    if (info)
                        thpool_add_work (data->server->thpool, data->onHold ?
                            (void *) handleOnHoldPacket : (void *) handlePacket, info);

                    else {
                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stderr, LOG_ERROR, LOG_PACKET, "Failed to create packet info!");
                        #endif
                    }

                    end += packet_size;
                }

                else break;
            }
        }
    }

}

// TODO: add support for handling large files transmissions
// what happens if my buffer isn't enough, for example a larger file?
// recive all incoming data from the socket
static void cerver_recieve (Cerver *server, i32 socket_fd, bool onHold) {

    // if (onHold) cerver_log_msg (stdout, LOG_SUCCESS, LOG_PACKET, "cerver_recieve () - on hold client!");
    // else cerver_log_msg (stdout, LOG_SUCCESS, LOG_PACKET, "cerver_recieve () - normal client!");

    ssize_t rc;
    char packetBuffer[MAX_UDP_PACKET_SIZE];
    memset (packetBuffer, 0, MAX_UDP_PACKET_SIZE);

    // do {
        rc = recv (socket_fd, packetBuffer, sizeof (packetBuffer), 0);
        
        if (rc < 0) {
            if (errno != EWOULDBLOCK) {     // no more data to read 
                // as of 02/11/2018 -- we juts close the socket and if the client is hanging
                // it will be removed with the client timeout function 
                // this is to prevent an extra client_count -= 1
                cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, "cerver_recieve () - rc < 0");

                close (socket_fd);  // close the client socket
            }

            // break;
        }

        else if (rc == 0) {
            // man recv -> steam socket perfomed an orderly shutdown
            // but in dgram it might mean something?
            perror ("Error:");
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, 
                    "Ending client connection - cerver_recieve () - rc == 0");

            close (socket_fd);  // close the client socket

            if (onHold) 
                 removeOnHoldClient (server, 
                    getClientBySocket (server->onHoldClients->root, socket_fd), socket_fd);  

            else {
                // remove the socket from the main poll
                for (u32 i = 0; i < poll_n_fds; i++) {
                    if (server->fds[i].fd == socket_fd) {
                        server->fds[i].fd = -1;
                        server->fds[i].events = -1;
                    }
                } 

                Client *c = getClientBySocket (server->clients->root, socket_fd);
                if (c) 
                    if (c->n_active_cons <= 1) 
                        client_closeConnection (server, c);

                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, 
                        "Couldn't find an active client with the requested socket!");
                    #endif
                }
            }

            // break;
        }

        else {
            // pass the necessary data to the server buffer handler
            RecvdBufferData *data = rcvd_buffer_data_new (server, socket_fd, packetBuffer, rc, onHold);
            server->handle_recieved_buffer (data);
        }
    // } while (true);

}

// FIXME: move this from here!!

static i32 cerver_accept (Cerver *server) {

    Client *client = NULL;

	struct sockaddr_storage clientAddress;
	memset (&clientAddress, 0, sizeof (struct sockaddr_storage));
    socklen_t socklen = sizeof (struct sockaddr_storage);

    i32 newfd = accept (server->serverSock, (struct sockaddr *) &clientAddress, &socklen);

    if (newfd < 0) {
        // if we get EWOULDBLOCK, we have accepted all connections
        if (errno != EWOULDBLOCK) cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Accept failed!");
        return -1;
    }

    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, "Accepted a new client connection.");
    #endif

    // get client values to use as default id in avls
    char *connection_values = client_getConnectionValues (newfd, clientAddress);
    if (!connection_values) {
        cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to get client connection values.");
        close (newfd);
        return -1;
    }

    else {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT,
            c_string_create ("Connection values: %s", connection_values));
        #endif
    } 

    client = newClient (server, newfd, clientAddress, connection_values);

    // if we requiere authentication, we send the client to on hold structure
    if (server->authRequired) onHoldClient (server, client, newfd);

    // if not, just register to the server as new client
    else {
        // if we need to generate session ids...
        if (server->useSessions) {
            // FIXME: change to the custom generate session id action in server->generateSessionID
            char *session_id = session_default_generate_id (newfd, clientAddress);
            if (session_id) {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, 
                    c_string_create ("Generated client session id: %s", session_id));
                #endif

                client_set_sessionID (client, session_id);
            }
            
            else cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to generate session id!");
        }

        client_registerToServer (server, client, newfd);
    } 

    // FIXME: add the option to select which connection values to send!!!
    // if (server->type != WEB_SERVER) sendServerInfo (server, newfd, clientAddress);
   
    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "A new client connected to the server!");
    #endif

    return newfd;

}

// server poll loop to handle events in the registered socket's fds
u8 cerver_poll (Cerver *cerver) {

    u8 retval = 1;

    if (cerver) {
        cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, 
            c_string_create ("Cerver %s main handler has started!", cerver->name->str));
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Waiting for connections...");
        #endif

        int poll_retval = 0;

        while (cerver->isRunning) {
            poll_retval = poll (cerver->fds, poll_n_fds, cerver->poll_timeout);

            // poll failed
            if (poll_retval < 0) {
                cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, 
                    c_string_create ("Cerver %s main poll has failed!", cerver->name->str));
                perror ("Error");
                cerver->isRunning = false;
                break;
            }

            // if poll has timed out, just continue to the next loop... 
            if (poll_retval == 0) {
                // #ifdef CERVER_DEBUG
                // cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
                //    c_string_create ("Cerver %s poll timeout", cerver->name->str));
                // #endif
                continue;
            }

            // one or more fd(s) are readable, need to determine which ones they are
            for (u32 i = 0; i < poll_n_fds; i++) {
                if (cerver->fds[i].revents == 0) continue;
                if (cerver->fds[i].revents != POLLIN) continue;

                // accept incoming connections that are queued
                if (cerver->fds[i].fd == cerver->sock) {
                    if (cerver_accept (cerver)) {
                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, "LOG_SUCCESS accepting a new client!");
                        #endif
                    }
                    else {
                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to accept a new client!");
                        #endif
                    } 
                }

                // TODO: maybe later add this directly to the thread pool
                // not the server socket, so a connection fd must be readable
                else cerver_recieve (cerver, cerver->fds[i].fd, false);
            }

            // if (server->compress_clients) compressClients (server);
        }

        #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, 
                c_string_create ("Cerver %s main poll has stopped!", cerver->name->str));
        #endif

        retval = 0;
    }

    else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Can't listen for connections on a NULL server!");

    return retval;

}