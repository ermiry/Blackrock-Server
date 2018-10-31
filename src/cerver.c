#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <sys/ioctl.h>      // to set server socket to nonblocking mode

#include <pthread.h>

#include <poll.h>
#include <errno.h>

#include "cerver.h"
#include "network.h"

#include "game.h"

#include "utils/thpool.h"
#include "utils/log.h"
#include "utils/vector.h"
#include "utils/config.h"
#include "utils/myUtils.h"

/*** VALUES ***/

// TODO: maybe move these to the config file??
// these 2 are used to manage the packets
ProtocolId PROTOCOL_ID = 0x4CA140FF; // randomly chosen
Version PROTOCOL_VERSION = { 1, 1 };

// TODO: better id handling and management
u16 nextPlayerId = 0;

/*** SERIALIZATION ***/

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

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

ssize_t packetHeaderSize;
ssize_t requestPacketSize;
ssize_t lobbyPacketSize;
ssize_t updatedGamePacketSize;
ssize_t playerInputPacketSize;

// FIXME:
// check for packets with bad size, protocol, version, etc
u8 checkPacket (ssize_t packetSize, unsigned char *packetData, PacketType type) {

    if ((unsigned) packetSize < sizeof (PacketHeader)) {
        logMsg (stderr, WARNING, PACKET, "Recieved a to small packet!");
        return 1;
    } 

    PacketHeader *header = (PacketHeader *) packetData;

    if (header->protocolID != PROTOCOL_ID) {
        logMsg (stderr, WARNING, PACKET, "Packet with unknown protocol ID.");
        return 1;
    }

    Version version = header->protocolVersion;
    if (version.major != PROTOCOL_VERSION.major) {
        logMsg (stderr, WARNING, PACKET, "Packet with incompatible version.");
        return 1;
    }

    switch (type) {
        case REQUEST: 
            if (packetSize < requestPacketSize) {
                logMsg (stderr, WARNING, PACKET, "Received a too small request packet.");
                return 1;
            } break;
        case GAME_UPDATE_TYPE: 
            if (packetSize < updatedGamePacketSize) {
                logMsg (stderr, WARNING, PACKET, "Received a too small game update packet.");
                return 1;
            } break;
        case PLAYER_INPUT_TYPE: 
            if (packetSize < playerInputPacketSize) {
                logMsg (stderr, WARNING, PACKET, "Received a too small player input packet.");
                return 1;
            } break;

        // TODO:
        case TEST_PACKET_TYPE: break;
        
        default: logMsg (stderr, WARNING, PACKET, "Got a pakcet of incompatible type."); return 1;
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

void initPacketHeader (void *header, PacketType type) {

    PacketHeader h;
    h.protocolID = PROTOCOL_ID;
    h.protocolVersion = PROTOCOL_VERSION;
    h.packetType = type;

    memcpy (header, &h, sizeof (PacketHeader));

}

// generates a generic packet with the specified packet type
void *generatePacket (PacketType packetType) {

    size_t packetSize = sizeof (PacketHeader);

    void *packetBuffer = malloc (packetSize);
    void *begin = packetBuffer;
    char *end = begin; 

    PacketHeader *header = (PacketHeader *) end;
    end += sizeof (PacketHeader);
    initPacketHeader (header, packetType); 

    return begin;

}

// TODO: 22/10/2018 -- 22:57 - if we have a lb, does all the traffic goes out from there?
// sends a packet from a server to the client address
void sendPacket (Server *server, void *begin, size_t packetSize, struct sockaddr_storage address) {

    ssize_t sentBytes = sendto (server->serverSock, (const char *) begin, packetSize, 0,
		       (struct sockaddr *) &address, sizeof (struct sockaddr_storage));

	if (sentBytes < 0 || (unsigned) sentBytes != packetSize)
        logMsg (stderr, ERROR, NO_TYPE, "Failed to send packet!") ;

}

// creates and sends an error packet to the player to hanlde it
u8 sendErrorPacket (Server *server, Client *client, ErrorType type, char *msg) {

    // first create the packet with the necesarry info
    size_t packetSize = sizeof (PacketHeader) + sizeof (ErrorData);
    
    void *packetBuffer = malloc (packetSize);
    void *begin = packetBuffer;
    char *end = begin; 

    PacketHeader *header = (PacketHeader *) end;
    end += sizeof (PacketHeader);
    initPacketHeader (header, ERROR_PACKET);

    ErrorData *edata = (ErrorData *) end;
    end += sizeof (ErrorData);
    edata->type = type;
    if (msg) strcpy (edata->msg, msg);

    // send the packet to the client
    sendPacket (server, begin, packetSize, client->address);

    return 0;

}

// FIXME: handle the players inside the lobby
// creates a lobby packet with the passed lobby info
void *createLobbyPacket (PacketType packetType, Lobby *lobby, size_t packetSize) {

    void *packetBuffer = malloc (packetSize);
    void *begin = packetBuffer;
    char *end = begin; 

    PacketHeader *header = (PacketHeader *) end;
    end += sizeof (PacketHeader);
    initPacketHeader (header, packetType); 

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

    return begin;

}

#pragma endregion

/*** REQUESTS ***/

// All clients can make any request, but if they make a game request, 
// they are now treated as players...

#pragma region REQUESTS


#pragma endregion

/*** CLIENTS ***/

// TODO: create a pool of clients inside each server?
// or maybe if we have a load balancer, that logic can change a little bit?

#pragma region CLIENTS

// FIXME: we can't have infinite ids!!
u32 nextClientID = 0;

// FIXME: take into account the server pool
Client *newClient (i32 sock, struct sockaddr_storage address) {

    Client *new = (Client *) malloc (sizeof (Client));
    
    new->clientID = nextClientID;
    new->clientSock = sock;

    new->address = address;

    return new;

}

// 13/10/2018 - I think register a client only works for tcp? but i might be wrong...

// TODO: hanlde a max number of clients connected to a server at the same time?
// maybe this can be handled before this call by the load balancer
void registerClient (Server *server, Client *client) {

    vector_push (&server->clients, client);

}

// sync disconnection
// when a clients wants to disconnect, it should send us a request to disconnect
// so that we can clean the correct structures

// if there is an async disconnection from the client, we need to have a time out
// that automatically clean up the clients if we do not get any request or input from them

// FIXME: where do we want to disconnect the client?
// TODO: removes a client from the server 
void unregisterClient (Server *server, Client *client) {

}

// TODO: used to check for client timeouts in any type of server
void checkClientTimeout (Server *server) {

}

#pragma endregion

// Here goes the logic for handling new connections and manage client requests
// also here we manage some server logic and server responses
#pragma region CONNECTION HANDLER

// TODO: check again the recieve packets function --> do we need also to pass
    // the sockaddr? or hanlde just the socket?

// FIXME: don't forget to check that tha packet type is a request!!!
// TODO: how can we handle other parameters for requests?
/* void connectionHandler (Server *server, i32 client, struct sockaddr_storage clientAddres) {

	// send welcome message
	send (client, welcome, sizeof (welcome), 0);

    // recieving packets from a client
    unsigned char packetData[MAX_UDP_PACKET_SIZE];
    ssize_t packetSize;
        
    if ((packetSize = recv (client, packetData, sizeof (packetData), 0)) > 0) {
        logMsg (stdout, TEST, PACKET, createString ("Recieved request pakcet size: %ld.", packetSize));
        // check the packet
        if (!checkPacket (packetSize, packetData, REQUEST)) {
            // handle the request
            RequestData *reqData = (RequestData *) (packetData + sizeof (PacketHeader));
            switch (reqData->type) {
                case REQ_GET_FILE: logMsg (stdout, REQ, FILE_REQ, "Requested a file."); break;
                case POST_SEND_FILE: logMsg (stdout, REQ, FILE_REQ, "Client wants to send a file."); break;

                case REQ_CREATE_LOBBY: createLobby (server, client, clientAddres); break;

                case REQ_TEST:  logMsg (stdout, TEST, NO_TYPE, "Packet recieved correctly!!"); break;

                default: logMsg (stderr, ERROR, REQ, "Invalid request type!");
            }
        }
            
        else logMsg (stderr, ERROR, REQ, "Recieved an invalid request packet from the client!");
    }

    // FIXME: better error handling I guess...
    else logMsg (stderr, ERROR, REQ, "No client request!");

}*/

// this a multiplexor to handle data transmissios to clients
void *connectionHandler (void *data) {

    Server *server = (Server *) data;

    if (server->clients.elements > 0) {
        // TODO: handle client requests

        switch (server->type) {
            case FILE_SERVER: break;
            case WEB_SERVER: break;
            case GAME_SERVER: {
                GameServerData *data = (GameServerData *) server->serverData;

                // TODO: update each game lobby
                // if (data->lobbys.elements > 0) {

                // }
            } break;

            default: break;
        }
    } 

}

// 04/10/2018 -- 22:56
// TODO: maybe a way to handle multiple clients, is having an array of clients, and each time
// we get a new connection we add the client to the struct and assign a new connection handler to it
// NOTE: not all clients are players, so we can handle anyone that connects as a client, 
// and if they make a game request, we can now treat them as players...

// FIXME: try a similar like in the recieve packets function --> maybe with that,
// we can handle different client requests at the same time
// TODO: handle ipv6 configuration
// TODO: do we need to hanlde time here?

#pragma endregion

// FIXME:
/*** AUTHENTICATE THE CLIENT CONNECTION ***/

#pragma region AUTHENTICATE CLIENT

void handleOnHoldClients () {

    // if (onHoldClients > 0) {
    //     foreach client in onholdclients
    //     // for each client being on hold, send an authentication request
    //     // and handle the authentication    
    // }

    // check for the correct version and protocol id and any securtity stuff we need
    // if it is correct, pass the client to the server
    // if not, reject the connection to the client and disconnect it

    // register the client to correct server
    // registerClient (server, client);

    // TODO: log to which server it connects and maybe from where
    #ifdef CERVER_DEBUG
    logMsg (stdout, SERVER, NO_TYPE, "New client connected.");
    #endif

}

// 20/10/2018 -- we are managing only one server... so each one has his on hold clients
// hold the client until he authenticates 
void onHoldClient (Server *server, Client *client) {

    // adds a client to the on hold structure

}

// FIXME: move this form here if we are going to need it...
typedef struct {

    Server *server;
    Client *client;

} ServerClient;

// 22/10/2018 -- used to auth a new client in the game server
void *generateClientAuthPacket (void) {

    size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);

    // buffer for packets
    void *packetBuffer = malloc (packetSize);

    void *begin = packetBuffer;
    char *end = begin; 

    // packet header
    PacketHeader *header = (PacketHeader *) end;
    end += sizeof (PacketHeader);
    initPacketHeader (header, AUTHENTICATION);

    // request data
    RequestData *data = (RequestData *) end;
    end += sizeof (RequestData);
    data->type = REQ_AUTH_CLIENT;

    return begin;

}

// FIXME:
// check that we have got a valid client connected, if not, drop him
void authenticateClient (void *data) {

    if (!data) logMsg (stderr, ERROR, NO_TYPE, "Can't autenticate a NULL client!");
    else {
        ServerClient *sc = (ServerClient *) data;

        // send a req to the client to authenticate itself
        sendPacket (sc->server, authPacket, authPacketSize, sc->client->address);

        // we expect a response from the client...
        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, "Waiting for the client to authenticate...");
        #endif

        // if we have got a valid client...
        // register the client to the server

        // add the client to the poll fd array
        /* server->fds[server->nfds].fd = newfd;
        server->fds[server->nfds].events = POLLIN;
        server->nfds++;

        // TODO: send feedback to the client that he is conncted to the server

        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, "Accepted a new connection.");
        #endif */

        // if not, drop the client structure and send feedback to player that
        // he has been denied
    }

}

#pragma endregion

#pragma region LISTENING

// FIXME: maybe use poll or select with a non-blocking socekt to correctly exit from this infinite loop??

// FIXME: we need to signal this process to end when we teardown the server!!!
// tcp needs to accept a connection to communicate with it
// this is called from a separate thread for each server     20/10/2018
void tcpListenForConnections (void *data) {

    if (!data) {
        logMsg (stderr, ERROR, SERVER, "Can't listen for tcp connections on a NULL server!");
        return;
    }

    Server *server = (Server *) data;

    listen (server->serverSock, server->connectionQueue);

    Client *client = NULL;
    i32 clientSocket;
	struct sockaddr_storage clientAddress;
	memset (&clientAddress, 0, sizeof (struct sockaddr_storage));
    socklen_t sockLen = sizeof (struct sockaddr_storage);

    ServerClient *sc = (ServerClient *) malloc (sizeof (ServerClient));

    logMsg (stdout, DEBUG_MSG, SERVER, "Listening for new connections...");

    for (;;) {
        if (server->isRunning) {
            if (clientSocket = accept (server->serverSock, (struct sockaddr *) &clientAddress, &sockLen)) {
                // first we need to check that we have got a valid client
                // so, we first add the client to a temp client list, and we ask him to authenticate
                client = newClient (clientSocket, clientAddress);

                sc->server = server;
                sc->client = client;

                // TODO: send the client to the on hold structure for authentication....
                // onHoldClient (server, client);

                // authenticate the client before he can make requests
                thpool_add_work (thpool, (void *) authenticateClient, sc);

                // 20/10/2018 -- for now we just register the client to the correct server they reach
                registerClient (server, client);
            }
        }

        break;  // if we are not accepting clients anymore...
    }

    if (sc) free (sc);

}

#pragma endregion

/*** LOAD BALANCER ***/

#pragma region LOAD BALANCER

// TODO:

#pragma endregion

/*** SERVER ***/

// Here we manage the creation and destruction of servers 
#pragma region SERVER LOGIC

// TODO: do we want to set the sock to nonblocking to handle the game? remember that in space-shooters
// they use udp and they set the socket to non-blocking mode...
// 23/10/2018 -- what about using poll? or select?

const char welcome[64] = "Welcome to cerver!";

// init server type independent data structs
void initServerDS (Server *server, ServerType type) {

    // all the server types have a client's vector
    vector_init (&server->clients, sizeof (Client));

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

// TODO: 19/10/2018 -- do we need to have the expected size of each server type
// inside the server structure?
// depending on the type of server, we need to init some const values
void initServerValues (Server *server, ServerType type) {
    
    // this are used in all the types
    // FIXME:
    // packetHeaderSize = sizeof (PacketHeader);
    // requestPacketSize = sizeof (RequestData);

    server->nfds = 0;

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

            data->lobbyPacketSize = sizeof (lobbyPacketSize);
            data->updatedGamePacketSize = sizeof (UpdatedGamePacket);
            data->playerInputPacketSize = sizeof (PlayerInputPacket);
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
    else server->useIpv6 = 0;

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
        logMsg (stdout, WARNING, SERVER, "No protocol found. Using default: tcp protocol");
        server->protocol = IPPROTO_TCP;
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
        logMsg (stdout, WARNING, SERVER, 
            createString ("No port found. Setting port to default value: %i", DEFAULT_PORT));
        server->port = DEFAULT_PORT;
    } 

    char *queue = getEntityValue (cfgEntity, "queue");
    if (queue) {
        server->connectionQueue = atoi (queue);
        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Connection queue: %i", server->connectionQueue));
        #endif
    } 
    else {
        logMsg (stdout, WARNING, SERVER, 
            createString ("Connection queue no specified. Setting it to default: %i", DEFAULT_CONNECTION_QUEUE));
        server->connectionQueue = DEFAULT_CONNECTION_QUEUE;
    }

    char *timeout = getEntityValue (cfgEntity, "timeout");
    if (timeout) {
        server->pollTimeout = atoi (timeout);
        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, SERVER, createString ("Server poll timeout: %i", server->pollTimeout));
        #endif
    }
    else {
        logMsg (stdout, WARNING, SERVER, 
            createString ("Poll timeout no specified. Setting it to default: %i", DEFAULT_POLL_TIMEOUT));
        server->pollTimeout = DEFAULT_POLL_TIMEOUT;
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

    // init the server
    switch (server->protocol) {
        case IPPROTO_TCP: 
            server->serverSock = socket ((server->useIpv6 == 1 ? AF_INET6 : AF_INET), SOCK_STREAM, 0);
            break;
        case IPPROTO_UDP:
            server->serverSock = socket ((server->useIpv6 == 1 ? AF_INET6 : AF_INET), SOCK_DGRAM, 0);
            break;

        default:
            logMsg (stderr, ERROR, SERVER, "Unkonw protocol type!"); return 1;
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

    else {
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
    }

    new->isRunning = false;

    // by default the socket is assumed to be non blocking
    new->blocking = false;
    
    return new;

}

Server *cerver_createServer (Server *server, ServerType type, void (*destroyServerData) (void *data)) {

    // create a server with the request parameters
    if (server) {
        Server *s = newServer (server);
        if (!initServer (s, NULL, type)) {
            s->destroyServerData = destroyServerData;
            logMsg (stdout, SUCCESS, SERVER, "\nCreated a new server!\n");
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
                logMsg (stdout, SUCCESS, SERVER, "Created a new server!\n");
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

// FIXME: add debuging info about the data of the new server
// FIXME: make sure to destroy our previos poll structure and create a new one -> reset respective values
// teardowns the server and creates a fresh new one with the same parameters
Server *cerver_restartServer (Server *server) {

    if (server) {
        logMsg (stdout, SERVER, NO_TYPE, "Restarting the server...");

        Server temp = { 
            .useIpv6 = server->useIpv6, .protocol = server->protocol, .port = server->port,
            .connectionQueue = server->connectionQueue, .type = server->type, .pollTimeout = server->pollTimeout};

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

// FIXME:
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

    logMsg (stdout, SUCCESS, SERVER, "Server has started!");
    logMsg (stdout, DEBUG_MSG, SERVER, "Waiting for connections...");

    int poll_retval;    // ret val from poll function
    int currfds;        // copy of n active server poll fds

    int newfd;          // fd of new connection
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
                    if (newfd < 0) {
                        if (errno != EWOULDBLOCK) {
                            // if not, accept failed
                            // FIXME: how to handle this??
                            logMsg (stderr, ERROR, SERVER, "Accept failed!");
                        }
                    }

                    // we have a new connection from a new client
                    client = newClient (newfd, clientAddress);

                    // FIXME: send the client to the on hold structure 

                    /* sc->server = server;
                    sc->client = client; */

                    // FIXME: authenticate the client before he can start making requests
                    // thpool_add_work (thpool, (void *) authenticateClient, sc);
                } while (newfd != -1);
            }

            // TODO: do we want to hanlde this using a separte thread???
            // not the server socket, so a connection fd must be readable
            else {
                size_t rc;          // retval from recv
                char buffer[1024];  // buffer for data recieved from fd
                // TODO: better handlde the lenght of this buffer!

                // printf("  Descriptor %d is readable\n", fds[i].fd);
                // recive all incoming data from this socket
                do {
                    rc = recv (server->fds[i].fd, buffer, sizeof (buffer), 0);
                    
                    // recv error
                    if (rc < 0) {
                        if (errno != EWOULDBLOCK) {
                            // FIXME: better handle this error!
                            logMsg (stderr, ERROR, SERVER, "Recv failed!");
                            perror ("Error:");
                        }
                    }

                    // check if the connection has been closed by the client
                    if (rc == 0) {
                        #ifdef DEBUG
                        logMsg (stdout, DEBUG_MSG, SERVER, "Client closed the connection.");
                        #endif
                        // TODO: what to do next?
                    }


                    // FIXME: handle the request/packet from the client
                    // data was recieved
                    // len = rc;
                    // printf("  %d bytes received\n", len);
                } while (true);

                // FIXME: set the end connection flag in the recv loop
                // end the connection if we were indicated
                /* if (close_conn) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    compress_array = TRUE;
                } */
            }

        }

        // FIXME: if we removed a file descriptor, we need to compress the pollfd array

        // FIXME: when we close a connection, we need to delete it from the fd array
            // this happens when a client disconnects or we teardown the server...

    }

}

// TODO: handle logic when we have a load balancer --> that will be the one in charge to listen for
// connections and accept them -> then it will send them to the correct server
// TODO: 13/10/2018 -- we can only handle a tcp server
// depending on the protocol, the logic of each server might change...
u8 cerver_startServer (Server *server) {

    if (server->isRunning) {
        logMsg (stdout, WARNING, SERVER, "The server is already running.");
        return 1;
    }

    u8 retval = 1;

    switch (server->protocol) {
        case IPPROTO_TCP: {
            // 28/10/2018 -- taking into account ibm poll example
            // we expect the socket to be already in non blocking mode
            if (!listen (server->serverSock, server->connectionQueue)) {
                // initialize pollfd structure
                memset (server->fds, 0, sizeof (server->fds));

                // set up the initial listening socket     
                server->fds[0].fd = server->serverSock;
                server->fds[0].events = POLLIN;
                server->nfds++;

                server->isRunning = true;

                // the timeout is set when we create the socket
                // we are now ready to start the poll loop -> traverse the fds array
                serverPoll (server);

                retval = 0;
            }

            else {
                logMsg (stderr, ERROR, SERVER, "Failed to listen in server socket!");
                close (server->serverSock);
                retval = 1;
            }
        } break;
        case IPPROTO_UDP: break;

        default: break;
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

// FIXME: 
// TODO: do we want this here? or in the game src file?
// cleans up all the game structs like lobbys and in game structures like maps
// if there are players connected, it sends a req to disconnect 
void destroyGameServer (void *data) {

    if (!data) return;      // just to be save

    Server *server = (Server *) data;
    GameServerData *gameData = (GameServerData *) server->serverData;

    // FIXME: clean on going games
    // clean any on going game...
    if (LIST_SIZE (gameData->currentLobbys) > 0) {
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
                void *packet = generatePacket (SERVER_TEARDOWN);
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
        void *packet = generatePacket (SERVER_TEARDOWN);
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
    #endif
    
}

// FIXME:
// TODO: clean the on hold client structures
// cleans up the client's structure in the current server
// if ther are clients connected, it sends a req to disconnect
void cleanUpClients (Server *server) {

    if (server->clients.elements > 0) {
        Client *client = NULL;
        
        for (size_t i_client = 0; i_client < server->clients.elements; i_client++) {
            // TODO: correctly disconnect every client
        }

        // TODO: delete the client structs
        while (server->clients.elements > 0) {
            
        }
    }

    // TODO: make sure destroy the client vector

}

// FIXME: correctly clean up the poll structures!!!
// FIXME: we need to join the ongoing threads... 
// teardown a server
u8 cerver_teardown (Server *server) {

    server->isRunning = false;  // don't accept connections anymore

    if (!server) {
        logMsg (stdout, ERROR, SERVER, "Can't destroy a NULL server!");
        return 1;
    }

    logMsg (stdout, SERVER, NO_TYPE, "Init server teardown...");

    // disable socket I/O in both ways
    cerver_shutdownServer (server);

    // TODO: join the on going threads?? -> just the listen ones?? or also the ones handling the lobby?

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

    free (server);

    logMsg (stdout, SUCCESS, NO_TYPE, "Server teardown was successfull!");

    return 0;   // teardown was successfull

}

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