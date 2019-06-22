#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "cerver/types/types.h"

#include "cerver/cerver.h"
#include "cerver/client.h"
#include "cerver/handler.h"
#include "cerver/auth.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

// FIXME:
// handle packet based on type
static void cerver_packet_handler (void *ptr) {

    if (ptr) {
        Packet *packet = (Packet *) ptr;
        if (!packet_check (packet)) {
            switch (packet->header->packet_type) {
                // handles cerver type packets
                case SERVER_PACKET: cerver_packet_handler (packet); break;

                // handles an error from the server
                case ERROR_PACKET: error_packet_handler (packet); break;

                // handles authentication packets
                case AUTH_PACKET: auth_packet_handler (packet); break;

                // handles a request made from the server
                case REQUEST_PACKET: break;

                // handle a game packet sent from the server
                case GAME_PACKET: break;

                // FIXME:
                // user set handler to handle app specific errors
                case APP_ERROR_PACKET: 
                    // if (pack_info->client->appErrorHandler)  
                    //     pack_info->client->appErrorHandler (pack_info);
                    break;

                // FIXME:
                // user set handler to handler app specific packets
                case APP_PACKET:
                    // if (pack_info->client->appPacketHandler)
                    //     pack_info->client->appPacketHandler (pack_info);
                    break;

                // custom packet hanlder
                case CUSTOM_PACKET: break;

                // FIXME:
                // handles test packets
                case TEST_PACKET: break;

                default:
                    #ifdef CLIENT_DEBUG
                    cengine_log_msg (stdout, WARNING, NO_TYPE, "Got a packet of unknown type.");
                    #endif
                    break;
            }
        }

        packet_delete (packet);
    }

}

// FIXME: old!
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
                            client_closeConnection (packet->cerver, packet->client); 
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
                    if (!sendTestPacket (packet->cerver, packet->clientSock, packet->client->address))
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
        // pool_push (packet->cerver->packetPool, data);
        destroyPacketInfo (packet);
    }

}

// FIXME: old!
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

                    Client *c = data->onHold ? getClientBySocket (data->cerver->onHoldClients->root, data->sock_fd) :
                        getClientBySocket (data->cerver->clients->root, data->sock_fd);

                    if (!c) {
                        cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to get client by socket!");
                        return;
                    }

                    info = newPacketInfo (data->cerver, c, data->sock_fd, packet_data, packet_size);

                    if (info)
                        thpool_add_work (data->cerver->thpool, data->onHold ?
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

static void cerver_handle_receive_buffer (Cerver *cerver, i32 sock_fd, 
    char *buffer, size_t buffer_size) {

    if (buffer && (buffer_size > 0)) {
        u32 buffer_idx = 0;
        char *end = buffer;

        PacketHeader *header = NULL;
        u32 packet_size;
        char *packet_data = NULL;

        while (buffer_idx < buffer_size) {
            header = (PacketHeader *) end;

            // check the packet size
            packet_size = header->packet_size;
            if (packet_size > 0) {
                // copy the content of the packet from the buffer
                packet_data = (char *) calloc (packet_size, sizeof (char));
                for (u32 i = 0; i < packet_size; i++, buffer_idx++) 
                    packet_data[i] = buffer[buffer_idx];

                Client *client = client_get_by_sock_fd (cerver, sock_fd);
                if (client) {
                    Packet *packet = packet_new ();
                    if (packet) {
                        packet->cerver = cerver;
                        packet->client = client;
                        packet->header = header;
                        packet->packet_size = packet_size;
                        packet->packet = packet_data;

                        // FIXME: actual handle the packet depending if it is on hold or not!
                        // thpool_add_work (client->thpool, (void *) client_packet_handler, packet);
                    }
                }

                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, 
                        c_string_create ("Failed to get client associated with sock: %i.", sock_fd));
                    #endif
                    // TODO: no client - discard the data!
                }

                end += packet_size;
            }

            else break;
        }
    }

}

// FIXME: correctly end client connection on error
// TODO: add support for handling large files transmissions
// receive all incoming data from the socket
static void cerver_receive (Cerver *cerver, i32 sock_fd, bool on_hold) {

    ssize_t rc;
    char packet_buffer[MAX_UDP_PACKET_SIZE];
    memset (packet_buffer, 0, MAX_UDP_PACKET_SIZE);

    // do {
        rc = recv (sock_fd, packet_buffer, sizeof (packet_buffer), 0);
        
        if (rc < 0) {
            if (errno != EWOULDBLOCK) {     // no more data to read 
                #ifdef CERVER_DEBUG 
                cerver_log_msg (stderr, ERROR, NO_TYPE, "cerver_recieve () - rc < 0");
                perror ("Error ");
                #endif
            }

            // /break;
        }

        else if (rc == 0) {
            // man recv -> steam socket perfomed an orderly shutdown
            // but in dgram it might mean something?
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, NO_TYPE, "cerver_recieve () - rc == 0");
            #endif
            // break;
        }
        
        else {
            char *buffer_data = (char *) calloc (rc, sizeof (char));
            if (buffer_data) {
                memcpy (buffer_data, packet_buffer, rc);
                cerver_handle_receive_buffer (cerver, sock_fd, buffer_data, rc);
            }
        }
        
    // } while (true);

}

// FIXME: old!
static void cerver_recieve (Cerver *cerver, i32 socket_fd, bool onHold) {

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
                 removeOnHoldClient (cerver, 
                    getClientBySocket (cerver->onHoldClients->root, socket_fd), socket_fd);  

            else {
                // remove the socket from the main poll
                for (u32 i = 0; i < poll_n_fds; i++) {
                    if (cerver->fds[i].fd == socket_fd) {
                        cerver->fds[i].fd = -1;
                        cerver->fds[i].events = -1;
                    }
                } 

                Client *c = getClientBySocket (cerver->clients->root, socket_fd);
                if (c) 
                    if (c->n_active_cons <= 1) 
                        client_closeConnection (cerver, c);

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
            // pass the necessary data to the cerver buffer handler
            RecvdBufferData *data = rcvd_buffer_data_new (cerver, socket_fd, packetBuffer, rc, onHold);
            cerver->handle_recieved_buffer (data);
        }
    // } while (true);

}

static void *cerver_accept (void *ptr) {

    if (ptr) {
        Cerver *cerver = (Cerver *) ptr;

        // accept the new connection
        struct sockaddr_storage client_address;
        memset (&client_address, 0, sizeof (struct sockaddr_storage));
        socklen_t socklen = sizeof (struct sockaddr_storage);

        i32 new_fd = accept (cerver->sock, (struct sockaddr *) &client_address, &socklen);
        if (new_fd > 0) {
            #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT, "Accepted a new client connection.");
            #endif

            Client *client = client_create (new_fd, client_address);
            if (client) {
                bool failed = false;

                // if we requiere authentication, we send the client to the on hold structure
                if (cerver->auth_required) client_on_hold (cerver, client, new_fd);

                // if not, just register to the cerver as new client
                else {
                    // if we need to generate session ids...
                    if (cerver->use_sessions) {
                        char *session_id = (char *) cerver->session_id_generator (client);
                        if (session_id) {
                            #ifdef CERVER_DEBUG
                            cerver_log_msg (stdout, LOG_DEBUG, LOG_CLIENT,
                                c_string_create ("Generated client session id: %s", session_id));
                            #endif
                            client_set_session_id (client, session_id);

                            client_register_to_cerver (cerver, client);
                        }

                        else {
                            cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, 
                                "Failed to generate client session id!");
                            // TODO: do we want to drop the client connection?
                            failed = true;
                        }
                    }

                    // just register the client to the cerver
                    else client_register_to_cerver (cerver, client);
                } 

                if (!failed) {
                    // send cerver info packet
                    if (cerver->type != WEB_SERVER) {
                        if (packet_send (cerver->cerver_info_packet, 0)) {
                            cerver_log_msg (stderr, LOG_ERROR, LOG_PACKET, 
                                c_string_create ("Failed to send cerver %s info packet!", cerver->name->str));
                        }   
                    }
                    
                    // trigger cerver on client connected action
                    if (cerver->on_client_connected) 
                        cerver->on_client_connected (cerver->on_client_connected_data);
                        
                }

                #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, 
                        c_string_create ("A new client connected to cerver %s!", cerver->name->str));
                #endif
            }

            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_ERROR, LOG_CLIENT, "Failed to create a new client!");
                #endif
            }
        }

        else {
            // if we get EWOULDBLOCK, we have accepted all connections
            if (errno != EWOULDBLOCK) {
                cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Accept failed!");
                perror ("Error");
            } 
        }
    }

}

// get a free index in the main cerver poll strcuture
i32 cerver_poll_get_free_idx (Cerver *cerver) {

    if (cerver) {
        for (u32 i = 0; i < cerver->max_n_fds; i++)
            if (cerver->fds[i].fd == -1) return i;

        return 0;
    }

    return -1;

}

// we remove any fd that was set to -1 for what ever reason
static void cerver_poll_compress_clients (Cerver *cerver) {

    if (cerver) {
        cerver->compress_clients = false;

        for (u32 i = 0; i < cerver->max_n_fds; i++) {
            if (cerver->fds[i].fd == -1) {
                for (u32 j = i; j < cerver->max_n_fds - 1; j++) 
                    cerver->fds[j].fd = cerver->fds[j + 1].fd;
                    
            }
        }
    }  

}

// TODO: cerver_accept and cerver_recieve created in new threads
// cerver poll loop to handle events in the registered socket's fds
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
            poll_retval = poll (cerver->fds, cerver->current_n_fds, cerver->poll_timeout);

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
                        cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, "Accepted a new client!");
                        #endif
                    }
                    else {
                        #ifdef CERVER_DEBUG
                        cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to accept a new client!");
                        #endif
                    } 
                }

                // TODO: maybe later add this directly to the thread pool
                // not the cerver socket, so a connection fd must be readable
                else cerver_recieve (cerver, cerver->fds[i].fd, false);
            }

            // if (cerver->compress_clients) compressClients (cerver);
        }

        #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, 
                c_string_create ("Cerver %s main poll has stopped!", cerver->name->str));
        #endif

        retval = 0;
    }

    else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Can't listen for connections on a NULL cerver!");

    return retval;

}