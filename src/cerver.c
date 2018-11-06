#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <sys/ioctl.h>      // to set server socket to nonblocking mode

#include <pthread.h>

#include <poll.h>
#include <errno.h>

#include "network.h"

#include "game.h"

#include "utils/thpool.h"
#include "utils/log.h"
#include "utils/vector.h"
#include "utils/config.h"
#include "utils/myUtils.h"

#include "utils/avl.h"      // 02/11/2018 -- using an avl tree to handle clients

/*** VALUES ***/

// these 2 are used to manage the packets
ProtocolId PROTOCOL_ID = 0x4CA140FF; // randomly chosen
Version PROTOCOL_VERSION = { 1, 1 };

// TODO: better id handling and management
u16 nextPlayerId = 0;

/*** SERIALIZATION ***/

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

// TODO:
#pragma region SERIALIZATION 

typedef int32_t RelativePtr;

typedef struct SArray {

	u32 n_elems;
	RelativePtr begin;

} SArray;

void s_ptr_to_relative (void *relativePtr, void *ptr) {

	RelativePtr result = (char *) ptr - (char *) relativePtr;
	memcpy (relativePtr, &result, sizeof (RelativePtr));

}

void s_array_init (void *array, void *begin, size_t n_elems) {

	SArray result;
	result.n_elems = n_elems;
	memcpy (array, &result, sizeof (SArray));
	s_ptr_to_relative ((char *) array + offsetof (SArray, begin), begin);

}

#pragma endregion

/*** PACKETS ***/

#pragma region PACKETS

PacketInfo *newPacketInfo (Server *server, Client *client, char *packetData, size_t packetSize) {

    PacketInfo *new = NULL;

    if (server->packetPool) {
        if (POOL_SIZE (server->packetPool) > 0) {
            new = pool_pop (server->packetPool);
            if (!new) new = (PacketInfo *) malloc (sizeof (PacketInfo));
        }
    }

    else new = (PacketInfo *) malloc (sizeof (PacketInfo));

    new->server = server;
    new->client = client;
    new->packetSize = packetSize;

    // copy the contents from the entry buffer to the packet info
    strcpy (new->packetData, packetData);

    return new;

}

// FIXME: used to destroy remaining packet info in the pools
void destroyPacketInfo (void *data) {}

// check for packets with bad size, protocol, version, etc
u8 checkPacket (size_t packetSize, char *packetData, PacketType expectedType) {

    if (packetSize < sizeof (PacketHeader)) {
        #ifdef CERVER_DEBUG
        logMsg (stderr, WARNING, PACKET, "Recieved a to small packet!");
        #endif
        return 1;
    } 

    PacketHeader *header = (PacketHeader *) packetData;

    if (header->protocolID != PROTOCOL_ID) {
        #ifdef CERVER_DEBUG
        logMsg (stdout, WARNING, PACKET, "Packet with unknown protocol ID.");
        #endif
        return 1;
    }

    Version version = header->protocolVersion;
    if (version.major != PROTOCOL_VERSION.major) {
        #ifdef CERVER_DEBUG
        logMsg (stdout, WARNING, PACKET, "Packet with incompatible version.");
        #endif
        return 1;
    }

    // compare the size we got from recv () against what is the expected packet size
    // that the client created 
    if (packetSize != header->packetSize) {
        #ifdef CERVER_DEBUG
        logMsg (stdout, WARNING, PACKET, "Recv packet size doesn't match header size.");
        #endif
        return 1;
    }

    if (expectedType != DONT_CHECK_TYPE) {
        // check if the packet is of the expected type
        if (header->packetType != expectedType) {
            #ifdef CERVER_DEBUG
            logMsg (stdout, WARNING, PACKET, "Packet doesn't match expected type.");
            #endif
            return 1;
        }
    }

    return 0;   // packet is fine

}

// THIS IS ONLY FOR UDP
// FIXME: when the owner of the lobby has quit the game, we ned to stop listenning for that lobby
    // also destroy the lobby and all its mememory
// Also check if there are no players left in the lobby for any reason
// this is used to recieve packets when in a lobby/game
void recievePackets (void) {

    /*unsigned char packetData[MAX_UDP_PACKET_SIZE];

    struct sockaddr_storage from;
    socklen_t fromSize = sizeof (struct sockaddr_storage);
    ssize_t packetSize;

    bool recieve = true;

    while (recieve) {
        packetSize = recvfrom (server, (char *) packetData, MAX_UDP_PACKET_SIZE, 
            0, (struct sockaddr *) &from, &fromSize);

        // no more packets to process
        if (packetSize < 0) recieve = false;

        // process packets
        else {
            // just continue to the next packet if we have a bad one...
            if (checkPacket (packetSize, packetData, PLAYER_INPUT_TYPE)) continue;

            // if the packet passes all the checks, we can use it safely
            else {
                PlayerInputPacket *playerInput = (PlayerInputPacket *) (packetData + sizeof (PacketHeader));
                handlePlayerInputPacket (from, playerInput);
            }

        }        
    } */ 

}

void initPacketHeader (void *header, PacketType type, u32 packetSize) {

    PacketHeader h;
    h.protocolID = PROTOCOL_ID;
    h.protocolVersion = PROTOCOL_VERSION;
    h.packetType = type;
    h.packetSize = packetSize;

    memcpy (header, &h, sizeof (PacketHeader));

}

// generates a generic packet with the specified packet type
void *generatePacket (PacketType packetType, size_t packetSize) {

    size_t packet_size;
    if (packetSize > 0) packet_size = packetSize;
    else packet_size = sizeof (PacketHeader);

    PacketHeader *header = (PacketHeader *) malloc (packet_size);
    initPacketHeader (header, packetType, packet_size); 

    return header;

}

// TODO: 06/11/2018 -- test this!
i8 udp_sendPacket (Server *server, const void *begin, size_t packetSize, 
    const struct sockaddr_storage address) {

    ssize_t sent;
    const void *p = begin;
    while (packetSize > 0) {
        sent = sendto (server->serverSock, begin, packetSize, 0, 
            (const struct sockaddr *) &address, sizeof (struct sockaddr_storage));
        if (sent <= 0) return -1;
        p += sent;
        packetSize -= sent;
    }

    return 0;

}

i8 tcp_sendPacket (i32 socket_fd, const void *begin, size_t packetSize, int flags) {

    ssize_t sent;
    const void *p = begin;
    while (packetSize > 0) {
        sent = send (socket, p, packetSize, flags);
        if (sent <= 0) return -1;
        p += sent;
        packetSize -= sent;
    }

    return 0;

}

// creates and sends an error packet
u8 sendErrorPacket (Server *server, Client *client, ErrorType type, char *msg) {

    size_t packetSize = sizeof (PacketHeader) + sizeof (ErrorData);
    
    void *begin = generatePacket (ERROR_PACKET, packetSize);
    char *end = begin + sizeof (PacketHeader); 

    ErrorData *edata = (ErrorData *) end;
    edata->type = type;
    if (msg) {
        if (strlen (msg) > sizeof (edata->msg)) {
            // clamp the value to fit inside edata->msg
            u16 i = 0;
            while (i < sizeof (edata->msg) - 1) {
                edata->msg[i] = msg[i];
                i++;
            }

            edata->msg[i] = '\0';
        }

        else strcpy (edata->msg, msg);
    }

    // send the packet to the client
    tcp_sendPacket (client->clientSock, begin, packetSize, 0);

    free (begin);

    return 0;

}

// FIXME: handle the players inside the lobby
// creates a lobby packet with the passed lobby info
void *createLobbyPacket (PacketType packetType, Lobby *lobby, size_t packetSize) {

    /* void *packetBuffer = malloc (packetSize);
    void *begin = packetBuffer;
    char *end = begin; 

    PacketHeader *header = (PacketHeader *) end;
    end += sizeof (PacketHeader);
    initPacketHeader (header, packetType, packetSize); 

    // serialized lobby data
    SLobby *slobby = (SLobby *) end;
    end += sizeof (SLobby);
    slobby->settings.fps = lobby->settings->fps;
    slobby->settings.minPlayers = lobby->settings->minPlayers;
    slobby->settings.maxPlayers = lobby->settings->maxPlayers;
    slobby->settings.playerTimeout = lobby->settings->playerTimeout;

    slobby->inGame = lobby->inGame;

    // update the players inside the lobby
    // FIXME: get owner info and the vector ot players

    return begin; */

}

void *createClientAuthReqPacket (void) {

    size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);
    void *begin = generatePacket (AUTHENTICATION, packetSize);
    char *end = begin + sizeof (PacketHeader);

    RequestData *request = (RequestData *) end;
    request->type = REQ_AUTH_CLIENT;

    return begin;

}

// broadcast a packet/msg to all clients inside a client's tree
void broadcastToAllClients (AVLNode *node, Server *server, void *packet, size_t packetSize) {

    if (node && server && packet && (packetSize > 0)) {
        broadcastToAllClients (node->right, server, packet, packetSize);

        // send packet to current node
        if (node->id) {
            Client *client = (Client *) node->id;
            if (client) 
                server->protocol == IPPROTO_TCP ? 
                    tcp_sendPacket (client->clientSock, packet, packetSize, 0) : 
                    udp_sendPacket (server, packet, packetSize, client->address);
        }

        broadcastToAllClients (node->left, server, packet, packetSize);
    }

}

#pragma endregion

/*** CLIENTS ***/

#pragma region CLIENTS

// get a client id based on the poll fd structure
i32 getNewClientId (Server *server) {

    i32 retval = -1;

    // search for a marked index in the array
    for (u16 i = 0; i < poll_n_fds; i++) {
        if (server->fds[i].fd == -1) {
            // this id n. is available
            retval = i;
            break;
        }
    }

    return retval;

}

i32 getHoldClientId (Server *server) {

    i32 retval = -1;

    // search for a marked index in the array
    for (u16 i = 0; i < poll_n_fds; i++) {
        if (server->hold_fds[i].fd == -1) {
            // this id n. is available
            retval = i;
            break;
        }
    }

    return retval;

}

Client *newClient (Server *server, i32 clientSock, struct sockaddr_storage address, bool onHold) {

    Client *new = NULL;

    if (server->clientsPool) {
        if (POOL_SIZE (server->clientsPool) > 0) {
            new = pool_pop (server->clientsPool);
            if (!new) new = (Client *) malloc (sizeof (Client));
        }
    }

    else new = (Client *) malloc (sizeof (Client));
    
    new->clientSock = clientSock;
    new->address = address;
 
    // if the client is on hold, we don't need to assign him a client id
    // until he authenticates and is sent to the main server poll
    if (onHold) {
        new->clientID = getHoldClientId (server);
        // if (new->clientID < 0) FIXME: we didn't find a new client id, is the server full?
    } 

    else {
        new->clientID = getNewClientId (server);
        // if (new->clientID < 0) FIXME: we didn't find a new client id, is the server full?
    }

    if (server->authRequired) {
        new->authTries = server->auth.maxAuthTries;
        new->dropClient = false;
    }

    return new;

}

// deletes a client forever
void destroyClient (void *data) {

    if (data) {
        Client *client = (Client *) data;

        close (client->clientSock);

        free (client);
    }

}

// comparator for client's avl tree
int clientComparator (void *a, void *b) {

    if (a && b) {
        Client *client_a = (Client *) a;
        Client *client_b = (Client *) b;

        if (client_a->clientSock > client_b->clientSock) return 1;
        else if (client_a->clientSock == client_b->clientSock) return 0;
        else return -1;

        // if (*((int *) a) >  *((int *)b)) return 1;
        // else if (*((int *) a) == *((int *)b)) return 0;
        // else return -1;
    }

}

// search a client in the server's clients avl by his sock's fd
Client *getClientBySock (AVLTree *clients, i32 fd) {

    if (clients) {
        Client temp = { .clientSock = fd };
        void *data = avl_getNodeData (clients, &temp);
        if (data) return (Client *) data;
        else {
            logMsg (stderr, ERROR, SERVER, 
                createString ("Couldn't find a client associated with the passed fd: %i.", fd));
            return NULL;
        } 
    }

    return NULL;

}

// 13/10/2018 - I think register a client only works for tcp? but i might be wrong...

// TODO: hanlde a max number of clients connected to a server at the same time?
// maybe this can be handled before this call by the load balancer
void registerClient (Server *server, Client *client) {

    Client *c = NULL;

    // at this point we assume the client was al ready authenticated
    if (server->authRequired) {
        // 02/11/2018 -- create a new client structure with the same values
        c = newClient (server, client->clientSock, client->address, false);

        // this destroys the client structure stored in the tree
        avl_removeNode (server->onHoldClients, client);

        // FIXME: get the correct values
        // server->hold_fds[c->clientSock].fd = -1;

        // TODO: do we want to do something similar?
        // 04/11/2018 -- 23:45 - we use the index in the poll array for new client ids
        // compress the hold fds array in the next poll loop
        // server->compress_hold_clients = true;
    }

    else c = client;

    // 04/11/2018 -- 23:45 - FIXME: this doesn't work anymore
    // send the client to the server's active clients structures
    /* server->fds[server->nfds].fd = c->clientSock;
    server->fds[server->nfds].events = POLLIN;
    server->nfds++; */

    avl_insertNode (server->clients, c);

    #ifdef CERVER_DEBUG
    logMsg (stdout, DEBUG_MSG, SERVER, "Registered a client to the server");
    #endif

}

// sync disconnection
// when a clients wants to disconnect, it should send us a request to disconnect
// so that we can clean the correct structures

// if there is an async disconnection from the client, we need to have a time out
// that automatically clean up the clients if we do not get any request or input from them

// FIXME: don't forget to call this function in the server teardown to clean up our structures?
// FIXME: where do we want to disconnect the client?
// removes a client from the server 
void unregisterClient (Server *server, Client *client) {

    // take out the client from the active clients structres, avl and poll in the server
    server->fds[client->clientID].fd = -1;

    // 04/11/2018 -- 23:45 - we use the index in the poll array for new client ids
    // compress the fds array in the next poll loop
    // server->compress_clients = true;

    avl_removeNode (server->clients, client);
    
    #ifdef CERVER_DEBUG
    logMsg (stdout, DEBUG_MSG, SERVER, "Unregistered a client from the sever");
    #endif

}

// TODO: used to check for client timeouts in any type of server
void checkClientTimeout (Server *server) {

}

#pragma endregion

/*** CLIENT AUTHENTICATION ***/

#pragma region CLIENT AUTHENTICATION

// drops a client from the on hold structure because he was unable to authenticate
void dropClient (Server *server, Client *client) {

    if (server && client) {
        // drop the client from the on hold structure
        server->hold_fds[client->clientSock].fd = -1;
        server->compress_hold_clients = true;
        // the client socket is closed when we call destroyClient () here
        avl_removeNode (server->onHoldClients, client);

        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, "Client removed from on hold structure. Failed to authenticate");
        #endif
    }   

}

// cerver default client authentication method
u8 defaultAuthMethod (void *data) {

    if (data) {
        PacketInfo *packet = (PacketInfo *) data;

        if (packet->client->authTries > 0) {
            DefAuthData *authData = 
                (DefAuthData *) packet->packetData + sizeof (PacketHeader) + sizeof (RequestData);
        
            // success authentication
            if (authData->code == DEFAULT_AUTH_CODE) return 0;
            else {
                packet->client->authTries--;
                if (packet->client->authTries <= 0) packet->client->dropClient = true;
                return 1;
            }
        }

        else {
            packet->client->dropClient = true;
            return 1;
        }       
    }

}

// TODO: send feedback to the client that he is conncted to the server
// 03/11/2018 - the admin is able to set a ptr to a custom authentication method
// handles the authentication data that the client sent
void authenticateClient (void *data) {

    if (data) {
        PacketInfo *packet = (PacketInfo *) data;

        if (!checkPacket (packet->packetSize, packet->packetData, AUTHENTICATION)) {            
            if (packet->server->auth.authenticate) {
                // successful authentication
                if (!packet->server->auth.authenticate (packet)) {
                    registerClient (packet->server, packet->client);
                    // TODO: send feedback to the client that he is conncted to the server
                    #ifdef CERVER_DEBUG
                    logMsg (stdout, DEBUG_MSG, SERVER, "Client authenticated correctly.");
                    #endif 
                }

                else {
                    // send feedback to the client
                    sendErrorPacket (packet->server, packet->client, ERR_FAILED_AUTH, "Wrong credentials!");
                    if (packet->client->dropClient) dropClient (packet->server, packet->client);
                }
            }

            // no authentication method -- clients are not able to interact to the server!
            else {
                logMsg (stderr, ERROR, SERVER, "Server doesn't have an authenticate method!");
                logMsg (stderr, ERROR, SERVER, "Clients are not able to interact with the server!");
                // just drop the client, the server admin must fix this!
                dropClient (packet->server, packet->client);
            } 
        }

        // dispose the packet -> send to packet pool
        pool_push (packet->server->packetPool, packet);
    }

}

// TODO: 03/11/2018 -- what happens when we have a load balancer?
// if the server requires authentication, we send the newly connected clients to an on hold
// structure until they authenticate, if not, they are just dropped by the server
void onHoldClient (Server *server, Client *client) {

    // add the client to the on hold structres -> avl and poll
    if (server && client) {
        if (server->onHoldClients) {
            server->hold_fds[server->hold_nfds].fd = client->clientSock;
            server->hold_fds[server->hold_nfds].events = POLLIN;
            server->hold_nfds++;

            avl_insertNode (server->onHoldClients, client);

            #ifdef CERVER_DEBUG
            logMsg (stdout, DEBUG_MSG, SERVER, "Added a new client to the on hold structures.");
            #endif

            // send the authentication request
            if (server->auth.reqAuthPacket) 
                sendPacket (server, server->auth.reqAuthPacket, server->auth.authPacketSize,
                client->address);
            else logMsg (stdout, WARNING, PACKET, "Server does not have a request auth packet.");
        }
    }

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

// handles packets from the on hold clients until they authenticate
u8 handleOnHoldClients (void *data) {

    if (!data) {
        logMsg (stderr, ERROR, SERVER, "Can't handle on hold clients on a NULL server!");
        return 1;
    }

    Server *server = (Server *) data;

    ssize_t rc;                                  // retval from recv -> size of buffer
    char packetBuffer[MAX_UDP_PACKET_SIZE];      // buffer for data recieved from fd
    PacketInfo *info = NULL;

    #ifdef CERVER_DEBUG
    logMsg (stdout, SUCCESS, SERVER, "On hold client poll has started!");
    #endif

    int poll_retval;    // ret val from poll function
    int currfds;        // copy of n active server poll fds
    while (server->isRunning) {
        poll_retval = poll (server->hold_fds, server->hold_nfds, server->pollTimeout);

        // poll failed
        if (poll_retval < 0) {
            logMsg (stderr, ERROR, SERVER, "On hold poll failed!");
            perror ("Error");
            server->isRunning = false;
            break;
        }

        // if poll has timed out, just continue to the next loop... 
        if (poll_retval == 0) {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, SERVER, "On hold poll timeout.");
            #endif
            continue;
        }

        // one or more fd(s) are readable, need to determine which ones they are
        currfds = server->hold_nfds;
        for (u8 i = 0; i < currfds; i++) {
            if (server->hold_fds[i].revents == 0) continue;

            // FIXME: how to hanlde an unexpected result??
            if (server->hold_fds[i].revents != POLLIN) {
                // TODO: log more detailed info about the fd, or client, etc
                // printf("  Error! revents = %d\n", fds[i].revents);
                logMsg (stderr, ERROR, SERVER, "Unexpected poll result!");
            }

            // recive all incoming data from this socket
            do {
                rc = recv (server->hold_fds[i].fd, packetBuffer, sizeof (packetBuffer), 0);
                
                if (rc < 0) {
                    if (errno != EWOULDBLOCK) {
                        logMsg (stderr, ERROR, SERVER, "On hold recv failed!");
                        perror ("Error:");
                    }

                    break;  // there is no more data to handle
                }

                if (rc == 0) break;

                info = newPacketInfo (server, 
                    getClientBySock (server->onHoldClients, server->hold_fds[i].fd), packetBuffer, rc);

                // we have a dedicated function to authenticate the clients
                thpool_add_work (server->thpool, (void *) authenticateClient, info);
            } while (true);
        }

        if (server->compress_hold_clients) compressHoldClients (server);
    } 

}

#pragma endregion

/*** CONNECTION HANDLER ***/

#pragma region CONNECTION HANDLER

// 01/11/2018 -- called with the th pool to handle a new packet
void handlePacket (void *data) {

    if (!data) {
        #ifdef CERVER_DEBUG
        logMsg (stdout, WARNING, PACKET, "Can't handle NULL packet data.");
        #endif
        return;
    }

    PacketInfo *packet = (PacketInfo *) data;

    if (!checkPacket (packet->packetSize, packet->packetData, DONT_CHECK_TYPE))  {
        PacketHeader *header = (PacketHeader *) packet->packetData;

        switch (header->packetType) {
            // handles an error from the client
            case ERROR_PACKET: break;

            // a client is trying to authenticate himself
            case AUTHENTICATION: break;

            // handles a request made from the client
            case REQUEST: break;

            // handle a game packet sent from a client
            case GAME_PACKET: gameServer_handlePacket (packet); break;

            case TEST_PACKET: 
                logMsg (stdout, TEST, NO_TYPE, "Got a successful test packet!"); 
                pool_push (packet->server->packetPool, packet);
                break;

            default: 
                logMsg (stderr, WARNING, PACKET, "Got a packet of incompatible type."); 
                pool_push (packet->server->packetPool, packet);
                break;
        }
    }

}

// we remove any fd that was set to -1 for what ever reason
void compressClients (Server *server) {

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

// TODO: add support for handling large files transmissions
// TODO: 02/11/2018 -- this is only intended for file and game servers only,
// maybe a web server works different when accesing from the browser
// TODO: 02/11/2018 -- also THIS poll function only works with a tcp connection because we are
// accepting each new connection, but for udp might be the same
// FIXME: we need to signal this process to end when we teardown the server!!!
// server poll loop to handle events in the registered socket's fds
u8 serverPoll (Server *server) {

    if (!server) {
        logMsg (stderr, ERROR, SERVER, "Can't listen for connections on a NULL server!");
        return 1;
    }

    Client *client = NULL;
    i32 clientSocket;
	struct sockaddr_storage clientAddress;
	memset (&clientAddress, 0, sizeof (struct sockaddr_storage));
    socklen_t sockLen = sizeof (struct sockaddr_storage);

    ssize_t rc;                                   // retval from recv -> size of buffer
    char packetBuffer[MAX_UDP_PACKET_SIZE];       // buffer for data recieved from fd
    PacketInfo *info = NULL;

    int poll_retval;    // ret val from poll function
    int currfds;        // copy of n active server poll fds

    int newfd;          // fd of new connection

    logMsg (stdout, SUCCESS, SERVER, "Server has started!");
    logMsg (stdout, DEBUG_MSG, SERVER, "Waiting for connections...");
    while (server->isRunning) {
        poll_retval = poll (server->fds, server->nfds, server->pollTimeout);

        // poll failed
        if (poll_retval < 0) {
            logMsg (stderr, ERROR, SERVER, "Poll failed!");
            perror ("Error");
            server->isRunning = false;
            break;
        }

        // if poll has timed out, just continue to the next loop... 
        if (poll_retval == 0) {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, SERVER, "Poll timeout.");
            #endif
            continue;
        }

        // one or more fd(s) are readable, need to determine which ones they are
        currfds = server->nfds;
        for (u8 i = 0; i < currfds; i++) {
            if (server->fds[i].revents == 0) continue;

            // FIXME: how to hanlde an unexpected result??
            if (server->fds[i].revents != POLLIN) {
                // TODO: log more detailed info about the fd, or client, etc
                // printf("  Error! revents = %d\n", fds[i].revents);
                logMsg (stderr, ERROR, SERVER, "Unexpected poll result!");
            }

            // listening fd is readable (sever socket)
            if (server->fds[i].fd == server->serverSock) {
                // accept incoming connections that are queued
                do {
                    newfd = accept (server->serverSock, (struct sockaddr *) &clientAddress, &sockLen);

                    // if we get EWOULDBLOCK, we have accepted all connections
                    if (newfd < 0)
                        if (errno != EWOULDBLOCK) logMsg (stderr, ERROR, SERVER, "Accept failed!");

                    // hold the client until he authenticates
                    if (server->authRequired) {
                        client = newClient (server, newfd, clientAddress, true);
                        onHoldClient (server, client);
                    } 

                    else {
                        client = newClient (server, newfd, clientAddress, false);
                        registerClient (server, client);  
                    } 
                } while (newfd != -1);
            }

            // not the server socket, so a connection fd must be readable
            else {
                // recive all incoming data from this socket
                // TODO: 03/11/2018 - add support for multiple reads to the socket
                // what happens if my buffer isn't enough, for example a larger file?
                do {
                    rc = recv (server->fds[i].fd, packetBuffer, sizeof (packetBuffer), 0);
                    
                    // recv error - no more data to read
                    if (rc < 0) {
                        if (errno != EWOULDBLOCK) {
                            logMsg (stderr, ERROR, SERVER, "Recv failed!");
                            perror ("Error:");
                        }

                        break;  // there is no more data to handle
                    }

                    if (rc == 0) {
                        // man recv -> steam socket perfomed an orderly shutdown
                        // but in dgram it might mean something?
                        // 03/11/2018 -- we just ignore the packet or whatever
                        break;
                    }

                    info = newPacketInfo (server, 
                        getClientBySock (server->clients, server->fds[i].fd), packetBuffer, rc);

                    thpool_add_work (server->thpool, (void *) handlePacket, info);
                } while (true);
            }

        }

        if (server->compress_clients) compressClients (server);
    }

}

#pragma endregion

/*** SERVER ***/

// TODO: handle ipv6 configuration
// TODO: create some helpful timestamps
// TODO: add the option to log our output to a .log file

// Here we manage the creation and destruction of servers 
#pragma region SERVER LOGIC

const char welcome[64] = "Welcome to cerver!";

// init server's data structures
void initServerDS (Server *server, ServerType type) {

    server->clients = avl_init (clientComparator, destroyClient);
    server->onHoldClients = avl_init (clientComparator, destroyClient);
    server->clientsPool = pool_init (destroyClient);

    server->packetPool = pool_init (destroyPacketInfo);
    // by default init the packet pool with some members
    for (u8 i = 0; i < 3; i++) pool_push (server->packetPool, malloc (sizeof (PacketInfo)));

    // initialize pollfd structures
    memset (server->fds, 0, sizeof (server->fds));
    server->nfds = 0;
    server->compress_clients = false;

    // set all fds as available spaces
    for (u16 i = 0; i < poll_n_fds; i++) server->fds[i].fd = -1;

    if (server->authRequired) {
        memset (server->hold_fds, 0, sizeof (server->fds));
        server->hold_nfds = 0;
        server->compress_hold_clients = false;
    }

    // initialize server's own thread pool
    server->thpool = thpool_init (DEFAULT_TH_POOL_INIT);
    if (!server->thpool) logMsg (stderr, ERROR, SERVER, "Failed to init server's thread pool!");

    switch (type) {
        case FILE_SERVER: break;
        case WEB_SERVER: break;
        case GAME_SERVER: {
            GameServerData *gameData = (GameServerData *) malloc (sizeof (GameServerData));

            // init the lobbys with n inactive in the pool
            game_initLobbys (gameData, GS_LOBBY_POOL_INIT);
            game_initPlayers (gameData, GS_PLAYER_POOL_INT);

            server->serverData = gameData;
        } break;
        default: break;
    }

}

// depending on the type of server, we need to init some const values
void initServerValues (Server *server, ServerType type) {

    if (server->authRequired) {
        server->auth.reqAuthPacket = generateClientAuthPacket ();
        server->auth.authPacketSize = sizeof (PacketHeader) + sizeof (RequestData);
    }

    else {
        server->auth.reqAuthPacket = NULL;
        server->auth.authPacketSize = 0;
    }

    switch (type) {
        case FILE_SERVER: break;
        case WEB_SERVER: break;
        case GAME_SERVER: {
            GameServerData *data = (GameServerData *) server->serverData;

            // get game modes info from a config file
            data->gameSettingsConfig  = parseConfigFile ("./config/gameSettings.cfg");
            if (!data->gameSettingsConfig) {
                logMsg (stderr, ERROR, GAME, "Problems loading game settings config!");
                return NULL;
            } 
        } break;
        default: break;
    }

}

u8 getServerCfgValues (Server *server, ConfigEntity *cfgEntity) {

    char *ipv6 = getEntityValue (cfgEntity, "ipv6");
    if (ipv6) {
        server->useIpv6 = atoi (ipv6);
        // if we have got an invalid value, the default is not to use ipv6
        if (server->useIpv6 != 0 || server->useIpv6 != 1) server->useIpv6 = 0;
    } 
    // if we do not have a value, use the default
    else server->useIpv6 = DEFAULT_USE_IPV6;

    #ifdef CERVER_DEBUG
    logMsg (stdout, DEBUG_MSG, SERVER, createString ("Use IPv6: %i", server->useIpv6));
    #endif

    char *tcp = getEntityValue (cfgEntity, "tcp");
    if (tcp) {
        u8 usetcp = atoi (tcp);
        if (usetcp < 0 || usetcp > 1) {
            logMsg (stdout, WARNING, SERVER, "Unknown protocol. Using default: tcp protocol");
            usetcp = 1;
        }

        if (usetcp) server->protocol = IPPROTO_TCP;
        else server->protocol = IPPROTO_UDP;

    }
    // set to default (tcp) if we don't found a value
    else {
        server->protocol = IPPROTO_TCP;
        logMsg (stdout, WARNING, SERVER, "No protocol found. Using default: tcp protocol");
    }

    char *port = getEntityValue (cfgEntity, "port");
    if (port) {
        server->port = atoi (port);
        // check that we have a valid range, if not, set to default port
        if (server->port <= 0 || server->port >= MAX_PORT_NUM) {
            logMsg (stdout, WARNING, SERVER, 
                createString ("Invalid port number. Setting port to default value: %i", DEFAULT_PORT));
            server->port = DEFAULT_PORT;
        }

        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Listening on port: %i", server->port));
        #endif
    }
    // set to default port
    else {
        server->port = DEFAULT_PORT;
        logMsg (stdout, WARNING, SERVER, 
            createString ("No port found. Setting port to default value: %i", DEFAULT_PORT));
    } 

    char *queue = getEntityValue (cfgEntity, "queue");
    if (queue) {
        server->connectionQueue = atoi (queue);
        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Connection queue: %i", server->connectionQueue));
        #endif
    } 
    else {
        server->connectionQueue = DEFAULT_CONNECTION_QUEUE;
        logMsg (stdout, WARNING, SERVER, 
            createString ("Connection queue no specified. Setting it to default: %i", DEFAULT_CONNECTION_QUEUE));
    }

    char *timeout = getEntityValue (cfgEntity, "timeout");
    if (timeout) {
        server->pollTimeout = atoi (timeout);
        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Server poll timeout: %i", server->pollTimeout));
        #endif
    }
    else {
        server->pollTimeout = DEFAULT_POLL_TIMEOUT;
        logMsg (stdout, WARNING, SERVER, 
            createString ("Poll timeout no specified. Setting it to default: %i", DEFAULT_POLL_TIMEOUT));
    }

    char *auth = getEntityValue (cfgEntity, "authentication");
    if (auth) {
        server->authRequired = atoi (auth);
        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, server->authRequired = 1 ? 
            "Server requires client authentication" : "Server does not requires client authentication");
        #endif
    }
    else {
        server->authRequired = DEFAULT_REQUIRE_AUTH;
        logMsg (stdout, WARNING, SERVER, "No auth option found. No authentication required by default.");
    }

    if (server->authRequired) {
        char *tries = getEntityValue (cfgEntity, "authTries");
        if (tries) {
            server->auth.maxAuthTries = atoi (tries);
            #ifdef CERVER_DEBUG
            logMsg (stdout, DEBUG_MSG, SERVER, createString ("Max auth tries set to: %i.", server->auth.maxAuthTries));
            #endif
        }
        else {
            server->auth.maxAuthTries = DEFAULT_AUTH_TRIES;
            logMsg (stdout, WARNING, SERVER, createString ("Max auth tries set to default: %i.", DEFAULT_AUTH_TRIES));
        }
    }

    return 0;

}

// TODO: 29/10/2018 - do we want to mark the sock fd as reusable as in the ibm example?
// init a server of a given type
u8 initServer (Server *server, Config *cfg, ServerType type) {

    if (!server) {
        logMsg (stderr, ERROR, SERVER, "Can't init a NULL server!");
        return 1;
    }

    #ifdef CERVER_DEBUG
    logMsg (stdout, DEBUG_MSG, SERVER, "Initializing server...");
    #endif

    if (cfg) {
        ConfigEntity *cfgEntity = getEntityWithId (cfg, type);
        if (!cfgEntity) {
            logMsg (stderr, ERROR, SERVER, "Problems with server config!");
            return 1;
        } 

        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, "Using config entity to set server values...");
        #endif

        if (!getServerCfgValues (server, cfgEntity)) 
            logMsg (stdout, SUCCESS, SERVER, "Done getting cfg server values");
    }

    // log server values
    else {
        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Use IPv6: %i", server->useIpv6));
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Listening on port: %i", server->port));
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Connection queue: %i", server->connectionQueue));
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Server poll timeout: %i", server->pollTimeout));
        logMsg (stdout, DEBUG_MSG, SERVER, server->authRequired = 1 ? 
            "Server requires client authentication" : "Server does not requires client authentication");

        if (server->authRequired) 
            logMsg (stdout, DEBUG_MSG, SERVER, createString ("Max auth tries set to: %i.", server->auth.maxAuthTries));
        #endif
    }

    // init the server with the selected protocol
    switch (server->protocol) {
        case IPPROTO_TCP: 
            server->serverSock = socket ((server->useIpv6 == 1 ? AF_INET6 : AF_INET), SOCK_STREAM, 0);
            break;
        case IPPROTO_UDP:
            server->serverSock = socket ((server->useIpv6 == 1 ? AF_INET6 : AF_INET), SOCK_DGRAM, 0);
            break;

        default: logMsg (stderr, ERROR, SERVER, "Unkonw protocol type!"); return 1;
    }
    
    if (server->serverSock < 0) {
        logMsg (stderr, ERROR, SERVER, "Failed to create server socket!");
        return 1;
    }

    #ifdef CERVER_DEBUG
    logMsg (stdout, DEBUG_MSG, SERVER, "Created server socket");
    #endif

    // 29/10/2018 -- we do this to allow polling
    // set the socket to non blocking mode
    if (!sock_setBlocking (server->serverSock, server->blocking)) {
        logMsg (stderr, ERROR, SERVER, "Failed to set server socket to non blocking mode!");
        // perror ("Error");
        close (server->serverSock);
        return 1;
    }

    // TODO: how to check that the socket is actually non blocking?

    else {
        server->blocking = false;
        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, "Server socket set to non blocking mode.");
        #endif
    }

    struct sockaddr_storage address;
	memset (&address, 0, sizeof (struct sockaddr_storage));

	if (server->useIpv6) {
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &address;
		addr->sin6_family = AF_INET6;
		addr->sin6_addr = in6addr_any;
		addr->sin6_port = htons (server->port);
	} 

    else {
		struct sockaddr_in *addr = (struct sockaddr_in *) &address;
		addr->sin_family = AF_INET;
		addr->sin_addr.s_addr = INADDR_ANY;
		addr->sin_port = htons (server->port);
	}

    if ((bind (server->serverSock, (const struct sockaddr *) &address, sizeof (struct sockaddr_storage))) < 0) {
        logMsg (stderr, ERROR, SERVER, "Failed to bind server socket!");
        return 1;
    }   

    initServerDS (server, type);
    initServerValues (server, type);
    #ifdef CERVER_DEBUG
    logMsg (stdout, DEBUG_MSG, SERVER, "Done creating server data structures...");
    #endif

    server->type = type;

	return 0;   // success

}

// the server constructor
Server *newServer (Server *server) {

    Server *new = (Server *) malloc (sizeof (Server));

    // init with some values
    if (server) {
        new->useIpv6 = server->useIpv6;
        new->protocol = server->protocol;
        new->port = server->port;
        new->connectionQueue = server->connectionQueue;
        new->pollTimeout = server->pollTimeout;
        new->authRequired = server->authRequired;
        new->type = server->type;
    }

    // by default the socket is assumed to be a blocking socket
    new->blocking = true;
    
    new->isRunning = false;

    return new;

}

Server *cerver_createServer (Server *server, ServerType type, Action destroyServerData) {

    // create a server with the requested parameters
    if (server) {
        Server *s = newServer (server);
        if (!initServer (s, NULL, type)) {
            s->destroyServerData = destroyServerData;
            logMsg (stdout, SUCCESS, SERVER, "Created a new server!");
            return s;
        }

        else {
            logMsg (stderr, ERROR, SERVER, "Failed to init the server!");
            free (s);   // delete the failed server...
            return NULL;
        }
    }

    // create the server from the default config file
    else {
        Config *serverConfig = parseConfigFile ("./config/server.cfg");
        if (!serverConfig) {
            logMsg (stderr, ERROR, NO_TYPE, "Problems loading server config!\n");
            return NULL;
        } 

        else {
            Server *s = newServer (NULL);
            if (!initServer (s, serverConfig, type)) {
                s->destroyServerData = destroyServerData;
                log_newServer (server);
                // we don't need the server config anymore
                clearConfig (serverConfig);
                return s;
            }

            else {
                logMsg (stderr, ERROR, SERVER, "Failed to init the server!");
                clearConfig (serverConfig);
                free (s);   // delete the failed server...
                return NULL;
            }
        }
    }

}

// FIXME: make sure to destroy our previos poll structure and create a new one -> reset respective values
// teardowns the server and creates a fresh new one with the same parameters
Server *cerver_restartServer (Server *server) {

    if (server) {
        logMsg (stdout, SERVER, NO_TYPE, "Restarting the server...");

        Server temp = { 
            .useIpv6 = server->useIpv6, .protocol = server->protocol, .port = server->port,
            .connectionQueue = server->connectionQueue, .type = server->type,
            .pollTimeout = server->pollTimeout, .authRequired = server->authRequired };

        if (!cerver_teardown (server)) logMsg (stdout, SUCCESS, SERVER, "Done with server teardown");
        else logMsg (stderr, ERROR, SERVER, "Failed to teardown the server!");

        // what ever the output, create a new server --> restart
        Server *retServer = newServer (&temp);
        if (!initServer (retServer, NULL, temp.type)) {
            logMsg (stdout, SUCCESS, SERVER, "Server has restarted!");
            return retServer;
        }
        else {
            logMsg (stderr, ERROR, SERVER, "Unable to retstart the server!");
            return NULL;
        }
    }

    else {
        logMsg (stdout, WARNING, SERVER, "Can't restart a NULL server!");
        return NULL;
    }

}

// TODO: handle logic when we have a load balancer --> that will be the one in charge to listen for
// connections and accept them -> then it will send them to the correct server
// TODO: 13/10/2018 -- we can only handle a tcp server
// depending on the protocol, the logic of each server might change...
u8 cerver_startServer (Server *server) {

    if (server->isRunning) {
        logMsg (stdout, WARNING, SERVER, "The server is already running!");
        return 1;
    }

    u8 retval = 1;
    switch (server->protocol) {
        case IPPROTO_TCP: {
            // 28/10/2018 -- taking into account ibm poll example
            // we expect the socket to be already in non blocking mode
            if (server->blocking == false) {
                if (!listen (server->serverSock, server->connectionQueue)) {
                    // set up the initial listening socket     
                    server->fds[server->nfds].fd = server->serverSock;
                    server->fds[server->nfds].events = POLLIN;
                    server->nfds++;

                    server->isRunning = true;

                    // 04//11/2018 - start separate on hold poll loop from the server th pool
                    if (server->authRequired) 
                        thpool_add_work (server->thpool, (void *) handleOnHoldClients, server);

                    // 04/11/2018 -- keep in mind this is intended for one server at a time,
                    // with a load balancer or diffrent physical servers, it must be different...
                    // main thread handles main poll function
                    serverPoll (server);

                    retval = 0;
                }

                else {
                    logMsg (stderr, ERROR, SERVER, "Failed to listen in server socket!");
                    close (server->serverSock);
                    retval = 1;
                }
            }

            else {
                logMsg (stderr, ERROR, SERVER, "Server socket is not set to non blocking!");
                retval = 1;
            }
            
        } break;

        case IPPROTO_UDP: /* TODO: */ break;

        default: 
            logMsg (stderr, ERROR, SERVER, "Cant't start server! Unknown protocol!");
            retval = 1;
            break;
    }

    return retval;

}

// TODO: what other logic will we need to handle? -> how to handle players / clients timeouts?
// what happens with the current lobbys or on going games??
// disable socket I/O in both ways

// FIXME: make sure the poll loop is not running
// FIXME: getting transport endpoint is not connected if we do not have any clients connected!
void cerver_shutdownServer (Server *server) {

    /* if (server->isRunning) {
        if (shutdown (server->serverSock, SHUT_WR) < 0) {
            logMsg (stderr, ERROR, SERVER, "Failed to shutdown the server!");
            perror ("");
        }
            

        else server->isRunning = false;
    } */

}

// FIXME: !!!!!!
// TODO: do we want this here? or in the game src file?
// cleans up all the game structs like lobbys and in game structures like maps
// if there are players connected, it sends a req to disconnect 
void destroyGameServer (void *data) {

    if (!data) return;      // just to be save

    Server *server = (Server *) data;
    GameServerData *gameData = (GameServerData *) server->serverData;

    // FIXME: clean on going games
    // clean any on going game...
    /* if (LIST_SIZE (gameData->currentLobbys) > 0) {
        Lobby *lobby = NULL;
        Player *tempPlayer = NULL;

        for (ListElement *e = LIST_START (gameData->currentLobbys); e != NULL; e = e->next) {
            lobby = (Lobby *) e->data;
            if (lobby->players.elements > 0) {
                // if we have players inside the lobby, send them a msg so that they can handle
                // their own logic to leave the logic

                // 27/10/2018 -- we are just sending a generic packet to the player and 
                // he must handle his own logic
                size_t packetSize = sizeof (PacketHeader);
                void *packet = generatePacket (SERVER_TEARDOWN, packetSize);
                if (packet) {
                    for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
                        tempPlayer = vector_get (&lobby->players, i_player);
                        if (tempPlayer) sendPacket (server, packet, packetSize, tempPlayer->client->address);
                        else logMsg (stderr, ERROR, GAME, "Got a NULL player inside a lobby player's vector!");
                    }
                }

                else logMsg (stderr, ERROR, PACKET, "Failed to generate server teardown packet!");

                // send the players back to the server's players list
                while (lobby->players.elements > 0) {
                    tempPlayer = vector_get (&lobby->players, 0);
                    removePlayerFromLobby (gameData, lobby->players, 0, tempPlayer);
                }

                // FIXME:
                // handle if the lobby has an in game going
                if (lobby->inGame) {
                    // stop the game
                    // clean necesarry game data
                    // players scores and anything else will not be registered
                }
            }
        }
    }

    // we can now safely delete each lobby structure
    destroyList (gameData->currentLobbys);
    pool_clear (gameData->lobbyPool);
    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Lobby list and pool have been cleared.");
    #endif

    // not all the players were inside a lobby, so...
    // send a msg to any player in the server structure, so that they can handle
    // their own logic to disconnect from the server
    if (LIST_SIZE (gameData->players) > 0) {
        // we have players connected to the server
        Player *tempPlayer = NULL;

        size_t packetSize = sizeof (PacketHeader);
        void *packet = generatePacket (SERVER_TEARDOWN, packetSize);
        if (!packet) logMsg (stderr, ERROR, PACKET, "Failed to generate server teardown packet!");

        for (ListElement *e = NULL; e != NULL; e = e->next) {
            tempPlayer = (Player *) e->data;
            if (tempPlayer) 
                if (packet) sendPacket (server, packet, packetSize, tempPlayer->client->address);
        }
    }

    // clean the players list and pool
    destroyList (gameData->players);
    pool_clear (gameData->playersPool);
    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Player list and pool have been cleared.");
    #endif

    // clean any other game server values
    clearConfig (gameData->gameSettingsConfig);     // clean game modes info
    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Done clearing game settings config.");
    #endif */
    
}

// TODO: what happens with the client descriptors and poll array?
// TODO: 03/11/2018 -- clean up client trees and pool
// cleans up the client's structures in the current server
// if ther are clients connected, we send a server teardown packet
void cleanUpClients (Server *server) {

    // create server teardown packet
    size_t packetSize = sizeof (PacketHeader);
    void *packet = generatePacket (SERVER_TEARDOWN, packetSize);

    // send a packet to any active client
    broadcastToAll (server->clients->root, server, packet, packetSize);
    // destroy the active clients tree
    avl_clearTree (&server->clients->root, server->clients->destroy);

    // also send a packet to on hold clients
    broadcastToAll (server->onHoldClients->root, server, packet, packetSize);
    // destroy the tree
    avl_clearTree (&server->onHoldClients->root, server->onHoldClients->destroy);

    // the pool has only "empty" clients
    pool_clear (server->clientsPool);

    free (packet);

}

// FIXME: be sure to free the server data

// 03/11/2018 -- TODO: we need to signal the poll loops to stop inmediatly
// TODO: stop the start server function 

// FIXME: correctly clean up the poll structures!!!
// FIXME: we need to join the ongoing threads... 
// teardown a server -> stop the server and clean all of its data
u8 cerver_teardown (Server *server) {

    server->isRunning = false;  // don't accept connections anymore

    if (!server) {
        logMsg (stdout, ERROR, SERVER, "Can't destroy a NULL server!");
        return 1;
    }

    logMsg (stdout, SERVER, NO_TYPE, "Init server teardown...");

    // disable socket I/O in both ways
    cerver_shutdownServer (server);

    // clean independent server type structs
    if (server->destroyServerData) server->destroyServerData (server);

    // clean common server structs
    cleanUpClients (server);

    // close the server socket
    if (close (server->serverSock) < 0) {
        logMsg (stdout, ERROR, SERVER, "Failed to close server socket!");
        free (server);
        return 1;
    }

    #ifdef CERVER_DEBUG
    logMsg (stdout, DEBUG_MSG, SERVER, "The server socket has been closed.");
    #endif

    // destroy any other server data structures
    pool_clear (server->packetPool);

    free (server);

    logMsg (stdout, SUCCESS, NO_TYPE, "Server teardown was successfull!");

    return 0;   // teardown was successfull

}

#pragma endregion

/*** LOAD BALANCER ***/

#pragma region LOAD BALANCER

// TODO:

#pragma endregion

/*** GAME SERVER ***/

#pragma region GAME SERVER

// TODO:
// this is used to clean disconnected players inside a lobby
// if we haven't recieved any kind of input from a player, disconnect it 
void checkPlayerTimeouts (void) {
}

// when a client creates a lobby or joins one, it becomes a player in that lobby
void tcpAddPlayer (Server *server) {
}

// TODO: as of 22/10/2018 -- we only have support for a tcp connection
// when we recieve a packet from a player that is not in the lobby, we add it to the game session
void udpAddPlayer () {
}

// FIXME: handle a limit of players!!
void addPlayer (struct sockaddr_storage address) {

    // TODO: handle ipv6 ips
    char addrStr[IP_TO_STR_LEN];
    sock_ip_to_string ((struct sockaddr *) &address, addrStr, sizeof (addrStr));
    logMsg (stdout, SERVER, PLAYER, createString ("New player connected from ip: %s @ port: %d.\n", 
        addrStr, sock_ip_port ((struct sockaddr *) &address)));

    // TODO: init other necessarry game values
    // add the new player to the game
    // Player newPlayer;
    // newPlayer.id = nextPlayerId;
    // newPlayer.address = address;

    // vector_push (&players, &newPlayer);

    // FIXME: this is temporary
    // spawnPlayer (&newPlayer);

}

// FIXME:
void handlePlayerInputPacket (struct sockaddr_storage from, PlayerInputPacket *playerInput) {

    // TODO: maybe we can have a better way for searching players??
    // check if we have the player already registerd
    Player *player = NULL;
    // for (size_t p = 0; p < players.elements; p++) {
    //     player = vector_get (&players, p);
    //     if (player != NULL) {
    //         if (sock_ip_equal ((struct sockaddr *) &player->address, ((struct sockaddr *) &from)))
    //             break;  // we found a match!
    //     }
    // }
    
    // TODO: add players only from the game lobby before the game inits!!
    // if not, add it to the game

    // handle the player input
    // player->input = packet->input;   // FIXME:
    // player->inputSequenceNum = packet->sequenceNum;
    // player->lastInputTime = getTimeSpec ();

}

// FIXME:
// TODO: maybe we can clean and refactor this?
// creates and sends game packets
void sendGamePackets (Server *server, int to) {

    // Player *destPlayer = vector_get (&players, to);

    // first we need to prepare the packet...

    // TODO: clean this a little, but don't forget this can be dynamic!!
	// size_t packetSize = packetHeaderSize + updatedGamePacketSize +
	// 	players.elements * sizeof (Player);

	// // buffer for packets, extend if necessary...
	// static void *packetBuffer = NULL;
	// static size_t packetBufferSize = 0;
	// if (packetSize > packetBufferSize) {
	// 	packetBufferSize = packetSize;
	// 	free (packetBuffer);
	// 	packetBuffer = malloc (packetBufferSize);
	// }

	// void *begin = packetBuffer;
	// char *end = begin; 

	// // packet header
	// PacketHeader *header = (PacketHeader *) end;
    // end += sizeof (PacketHeader);
    // initPacketHeader (header, GAME_UPDATE_TYPE);

    // // TODO: do we need to send the game settings each time?
	// // game settings and other non-array data
    // UpdatedGamePacket *gameUpdate = (UpdatedGamePacket *) end;
    // end += updatedGamePacketSize;
    // // gameUpdate->sequenceNum = currentTick;  // FIXME:
	// // tick_packet->ack_input_sequence_num = dest_player->input_sequence_num;  // FIXME:
    // gameUpdate->playerId = destPlayer->id;
    // gameUpdate->gameSettings.playerTimeout = 30;    // FIXME:
    // gameUpdate->gameSettings.fps = 20;  // FIXME:

    // TODO: maybe add here some map elements, dropped items and enemies??
    // such as the players or explosions?
	// arrays headers --- 01/10/2018 -- we only have the players array
    // void *playersArray = (char *) gameUpdate + offsetof (UpdatedGamePacket, players);
    // FIXME:
	// void *playersArray = (char *) gameUpdate + offsetof (gameUpdate, players);

	// send player info ---> TODO: what player do we want to send?
    /* s_array_init (playersArray, end, players.elements);

    Player *player = NULL;
    for (size_t p = 0; p < players.elements; p++) {
        player = vector_get (&players, p);

        // FIXME: do we really need to serialize the packets??
        // FIXME: do we really need to have serializable data structs?

		// SPlayer *s_player = (SPlayer *) packet_end;
		// packet_end += sizeof(*s_player);
		// s_player->id = player->id;
		// s_player->alive = !!player->alive;
		// s_player->position = player->position;
		// s_player->heading = player->heading;
		// s_player->score = player->score;
		// s_player->color.red = player->color.red;
		// s_player->color.green = player->color.green;
		// s_player->color.blue = player->color.blue;
    }

    // FIXME: better eroror handling!!
    // assert (end == (char *) begin + packetSize); */

    // after the pakcet has been prepare, send it to the dest player...
    // sendPacket (server, begin, packetSize, destPlayer->address);

}

// FIXME:
// request from a from a tcp connected client to create a new lobby 
// in the server he is actually connected to
void createLobby (Server *server, Client *client, GameType gameType) {

    // TODO: how do we check if the current client isn't associated with a player?

    // we need to treat the client as a player now
    // TODO: 22/10/2018 -- maybe this logic can change if we have a load balancer?
    Player *owner = NULL;

    GameServerData *gameData = (GameServerData *) server->serverData;
    if (gameData->playersPool) {
        if (POOL_SIZE (gameData->playersPool) > 0) {
            owner = (Player *) pop (gameData->playersPool);
            if (!owner) owner = (Player *) malloc (sizeof (Player));
        }
    }

    else {
        logMsg (stderr, WARNING, SERVER, "Game server has no refrence to a players pool!");
        owner = (Player *) malloc (sizeof (Player));
    }

    owner->id = nextPlayerId;
    nextPlayerId++;

    owner->client = client;     // reference to the client network data

    Lobby *lobby = newLobby (server, owner, gameType);
    if (lobby) {
        #ifdef DEBUG
        logMsg (stdout, GAME, NO_TYPE, "New lobby created.");
        #endif  

        // send the lobby info to the owner -- we only have one player in the lobby vector
        size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + sizeof (SPlayer);
        void *lobbyPacket = createLobbyPacket (LOBBY_CREATE, lobby, packetSize);
        if (lobbyPacket) {
            sendPacket (server, lobbyPacket, packetSize, owner->client->address);
            free (lobbyPacket);
        }

        else logMsg (stderr, ERROR, PACKET, "Failed to create lobby packet!");

        // TODO: do we want to do this using a request?
        // FIXME: we need to wait for an ack of the ownwer and then we can do this...
        // the ack is when the player is ready in its lobby screen, and only then we can
        // handle requests from other players to join

        // if the owner send us an ack packet of the lobby, is because the client is the lobby screen
        // and the lobby is ready to recieve more players... but that is handled from other client reqs
    }

    else {
        logMsg (stderr, ERROR, GAME, "Failed to create a new game lobby.");

        // 24/10/2018 -- newLobby () sends an error message to the player...
    } 

}

#pragma endregion