#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <pthread.h>

#include <poll.h>
#include <errno.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/packets.h"
#include "cerver/cerver.h"
#include "cerver/game/game.h"

#include "cerver/collections/avl.h" 
#include "cerver/collections/vector.h"

#include "cerver/utils/thpool.h"
#include "cerver/utils/log.h"
#include "cerver/utils/config.h"
#include "cerver/utils/utils.h"
#include "cerver/utils/sha-256.h"

/*** PACKETS ***/

#pragma region PACKETS

// FIXME: object pool!!
PacketInfo *newPacketInfo (Server *server, Client *client, i32 clientSock,
    char *packetData, size_t packetSize) {

    PacketInfo *p = (PacketInfo *) malloc (sizeof (PacketInfo));

    // if (server->packetPool) {
    //     if (POOL_SIZE (server->packetPool) > 0) {
    //         p = pool_pop (server->packetPool);
    //         if (!p) p = (PacketInfo *) malloc (sizeof (PacketInfo));
    //         else if (p->packetData) free (p->packetData);
    //     }
    // }

    // else {
    //     p = (PacketInfo *) malloc (sizeof (PacketInfo));
    //     p->packetData = NULL;
    // } 

    if (p) {
        p->packetData = NULL;

        p->server = server;

        if (!client) printf ("pack info - got a NULL client!\n");

        p->client = client;
        p->clientSock = clientSock;
        p->packetSize = packetSize;
        
        // copy the contents from the entry buffer to the packet info
        if (!p->packetData)
            p->packetData = (char *) calloc (MAX_UDP_PACKET_SIZE, sizeof (char));

        memcpy (p->packetData, packetData, MAX_UDP_PACKET_SIZE);
    }

    return p;

}

// used to destroy remaining packet info in the pools
void destroyPacketInfo (void *data) {

    if (data) {
        PacketInfo *packet = (PacketInfo *) data;
        packet->server= NULL;
        packet->client = NULL;
        if (packet->packetData) free (packet->packetData);
    }

}

// check for packets with bad size, protocol, version, etc
u8 checkPacket (size_t packetSize, char *packetData, PacketType expectedType) {

    if (packetSize < sizeof (PacketHeader)) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stderr, WARNING, PACKET, "Recieved a to small packet!");
        #endif
        return 1;
    } 

    // our clients must send us a packet with the cerver;s packet header to be valid
    PacketHeader *header = (PacketHeader *) packetData;

    if (header->protocolID != PROTOCOL_ID) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, WARNING, PACKET, "Packet with unknown protocol ID.");
        #endif
        return 1;
    }

    // FIXME: error when sending auth packet from client!
    // Version version = header->protocolVersion;
    // if (version.major != PROTOCOL_VERSION.major) {
    //     #ifdef CERVER_DEBUG
    //     cerver_log_msg (stdout, WARNING, PACKET, "Packet with incompatible version.");
    //     #endif
    //     return 1;
    // }

    // compare the size we got from recv () against what is the expected packet size
    // that the client created 
    if ((u32) packetSize != header->packetSize) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, WARNING, PACKET, "Recv packet size doesn't match header size.");
        cerver_log_msg (stdout, DEBUG_MSG, PACKET, 
            c_string_create ("Recieved size: %i - Expected size %i", packetSize, header->packetSize));
        #endif
        return 1;
    } 

    if (expectedType != DONT_CHECK_TYPE) {
        // check if the packet is of the expected type
        if (header->packetType != expectedType) {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, WARNING, PACKET, "Packet doesn't match expected type.");
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
        sent = send (socket_fd, p, packetSize, flags);
        if (sent <= 0) return -1;
        p += sent;
        packetSize -= sent;
    }

    return 0;

}

// handles the correct logic for sending a packet, depending on the supported protocols
u8 server_sendPacket (Server *server, i32 socket_fd, struct sockaddr_storage address, 
    const void *packet, size_t packetSize) {

    if (server) {
        switch (server->protocol) {
            case IPPROTO_TCP: 
                if (tcp_sendPacket (socket_fd, packet, packetSize, 0) >= 0)
                    return 0;
                break;
            case IPPROTO_UDP:
                if (udp_sendPacket (server, packet, packetSize, address) >= 0)
                    return 0;
                break;
            default: break;
        }
    }

    return 1;

}

// just creates an erro packet -> used directly when broadcasting to many players
void *generateErrorPacket (ErrorType type, const char *msg) {

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

    return begin;

}

// creates and sends an error packet
u8 sendErrorPacket (Server *server, i32 sock_fd, struct sockaddr_storage address,
    ErrorType type, const char *msg) {

    size_t packetSize = sizeof (PacketHeader) + sizeof (ErrorData);
    void *errpacket = generateErrorPacket (type, msg);

    if (errpacket) {
        server_sendPacket (server, sock_fd, address, errpacket, packetSize);

        free (errpacket);
    }

    return 0;

}

u8 sendTestPacket (Server *server, i32 sock_fd, struct sockaddr_storage address) {

    if (server) {
        size_t packetSize = sizeof (PacketHeader);
        void *packet = generatePacket (TEST_PACKET, packetSize);
        if (packet) {
            u8 retval = server_sendPacket (server, sock_fd, address, packet, packetSize);
            free (packet);

            return retval;
        }
    }

    return 1;

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

        // send packet to current client
        if (node->id) {
            Client *client = (Client *) node->id;

            // 02/12/2018 -- send packet to the first active connection
            if (client) 
                server_sendPacket (server, client->active_connections[0], client->address,
                    packet, packetSize);
        }

        broadcastToAllClients (node->left, server, packet, packetSize);
    }

}

void *generateServerInfoPacket (Server *server) {

    if (server) {
        size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData) + sizeof (SServer);
        void *packet = generatePacket (SERVER_PACKET, packetSize);
         (packet + sizeof (RequestData));
        if (packet) {
            char *end = packet + sizeof (PacketHeader);
            RequestData *reqdata = (RequestData *) end;
            reqdata->type = SERVER_INFO;

            end += sizeof (RequestData);

            SServer *sinfo = (SServer *) end;
            sinfo->authRequired = server->authRequired;
            sinfo->port = server->port;
            sinfo->protocol = server->protocol;
            sinfo->type = server->type;
            sinfo->useIpv6 = server->useIpv6;

            return packet;
        }
    }

    return NULL;

}

// send useful server info to the client
void sendServerInfo (Server *server, i32 sock_fd, struct sockaddr_storage address) {

    if (server) {
        if (server->serverInfo) {
            size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData) + sizeof (SServer);

            if (!server_sendPacket (server, sock_fd, address, server->serverInfo, packetSize)) {
                #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Sent server info packet.");
                #endif
            }
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, ERROR, SERVER, "No server info to send to client!");
            #endif
        } 
    }

}

// FIXME:
// TODO: move this from here to the config section
// set an action to be executed when a client connects to the server
void server_set_send_server_info () {}

#pragma endregion

/*** SESSIONS ***/

#pragma region SESSIONS

// create a unique session id for each client based on connection values
char *session_default_generate_id (i32 fd, const struct sockaddr_storage address) {

    char *ipstr = sock_ip_to_string ((const struct sockaddr *) &address);
    u16 port = sock_ip_port ((const struct sockaddr *) &address);

    if (ipstr && (port > 0)) {
        #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, CLIENT,
                c_string_create ("Client connected form IP address: %s -- Port: %i", 
                ipstr, port));
        #endif

        // 24/11/2018 -- 22:14 -- testing a simple id - just ip + port
        char *connection_values = c_string_create ("%s-%i", ipstr, port);

        uint8_t hash[32];
        char hash_string[65];

        sha_256_calc (hash, connection_values, strlen (connection_values));
        sha_256_hash_to_string (hash_string, hash);

        char *retval = c_string_create ("%s", hash_string);

        return retval;
    }

    return NULL;

}

// the server admin can define a custom session id generator
void session_set_id_generator (Server *server, Action idGenerator) {

    if (server && idGenerator) {
        if (server->useSessions) 
            server->generateSessionID = idGenerator;

        else cerver_log_msg (stderr, ERROR, SERVER, "Server is not set to use sessions!");
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
u8 initServerDS (Server *server, ServerType type) {

    if (server->useSessions) server->clients = avl_init (client_comparator_sessionID, destroyClient);
    else server->clients = avl_init (client_comparator_clientID, destroyClient);

    server->clientsPool = pool_init (destroyClient);

    server->packetPool = pool_init (destroyPacketInfo);
    // by default init the packet pool with some members
    PacketInfo *info = NULL;
    for (u8 i = 0; i < 3; i++) {
        info = (PacketInfo *) malloc (sizeof (PacketInfo));
        info->packetData = NULL;
        pool_push (server->packetPool, info);
    } 

    // initialize pollfd structures
    memset (server->fds, 0, sizeof (server->fds));
    server->nfds = 0;
    server->compress_clients = false;

    // set all fds as available spaces
    for (u16 i = 0; i < poll_n_fds; i++) server->fds[i].fd = -1;

    if (server->authRequired) {
        server->onHoldClients = avl_init (client_comparator_clientID, destroyClient);

        memset (server->hold_fds, 0, sizeof (server->hold_fds));
        server->hold_nfds = 0;
        server->compress_hold_clients = false;

        server->n_hold_clients = 0;

        for (u16 i = 0; i < poll_n_fds; i++) server->hold_fds[i].fd = -1;
    }

    // initialize server's own thread pool
    server->thpool = thpool_init (DEFAULT_TH_POOL_INIT);
    if (!server->thpool) {
        cerver_log_msg (stderr, ERROR, SERVER, "Failed to init server's thread pool!");
        return 1;
    } 

    switch (type) {
        case FILE_SERVER: break;
        case WEB_SERVER: break;
        case GAME_SERVER: {
            // FIXME: correctly init the game server!!
            // GameServerData *gameData = (GameServerData *) malloc (sizeof (GameServerData));

            // // init the lobbys with n inactive in the pool
            // if (game_init_lobbys (gameData, GS_LOBBY_POOL_INIT)) {
            //     cerver_log_msg (stderr, ERROR, NO_TYPE, "Failed to init server lobbys!");
            //     return 1;
            // }

            // // init the players with n inactive in the pool
            // if (game_init_players (gameData, GS_PLAYER_POOL_INT)) {
            //     cerver_log_msg (stderr, ERROR, NO_TYPE, "Failed to init server players!");
            //     return 1;
            // }

            // server->serverData = gameData;
        } break;
        default: break;
    }

    return 0;

}

// depending on the type of server, we need to init some const values
void initServerValues (Server *server, ServerType type) {

    server->handle_recieved_buffer = default_handle_recieved_buffer;

    if (server->authRequired) {
        server->auth.reqAuthPacket = createClientAuthReqPacket ();
        server->auth.authPacketSize = sizeof (PacketHeader) + sizeof (RequestData);

        server->auth.authenticate = defaultAuthMethod;
    }

    else {
        server->auth.reqAuthPacket = NULL;
        server->auth.authPacketSize = 0;
    }

    if (server->useSessions) 
        server->generateSessionID = (void *) session_default_generate_id;

    server->serverInfo = generateServerInfoPacket (server);

    switch (type) {
        case FILE_SERVER: break;
        case WEB_SERVER: break;
        case GAME_SERVER: {
            GameServerData *data = (GameServerData *) server->serverData;

            // FIXME:
            // get game modes info from a config file
            // data->gameSettingsConfig = config_parse_file (GS_GAME_SETTINGS_CFG);
            // if (!data->gameSettingsConfig) 
            //     cerver_log_msg (stderr, ERROR, GAME, "Problems loading game settings config!");

            // data->n_gameInits = 0;
            // data->gameInitFuncs = NULL;
            // data->loadGameData = NULL;
        } break;
        default: break;
    }

    server->connectedClients = 0;

}

u8 getServerCfgValues (Server *server, ConfigEntity *cfgEntity) {

    char *ipv6 = config_get_entity_value (cfgEntity, "ipv6");
    if (ipv6) {
        server->useIpv6 = atoi (ipv6);
        // if we have got an invalid value, the default is not to use ipv6
        if (server->useIpv6 != 0 || server->useIpv6 != 1) server->useIpv6 = 0;

        free (ipv6);
    } 
    // if we do not have a value, use the default
    else server->useIpv6 = DEFAULT_USE_IPV6;

    #ifdef CERVER_DEBUG
    cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Use IPv6: %i", server->useIpv6));
    #endif

    char *tcp = config_get_entity_value (cfgEntity, "tcp");
    if (tcp) {
        u8 usetcp = atoi (tcp);
        if (usetcp < 0 || usetcp > 1) {
            cerver_log_msg (stdout, WARNING, SERVER, "Unknown protocol. Using default: tcp protocol");
            usetcp = 1;
        }

        if (usetcp) server->protocol = IPPROTO_TCP;
        else server->protocol = IPPROTO_UDP;

        free (tcp);

    }
    // set to default (tcp) if we don't found a value
    else {
        server->protocol = IPPROTO_TCP;
        cerver_log_msg (stdout, WARNING, SERVER, "No protocol found. Using default: tcp protocol");
    }

    char *port = config_get_entity_value (cfgEntity, "port");
    if (port) {
        server->port = atoi (port);
        // check that we have a valid range, if not, set to default port
        if (server->port <= 0 || server->port >= MAX_PORT_NUM) {
            cerver_log_msg (stdout, WARNING, SERVER, 
                c_string_create ("Invalid port number. Setting port to default value: %i", DEFAULT_PORT));
            server->port = DEFAULT_PORT;
        }

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Listening on port: %i", server->port));
        #endif
        free (port);
    }
    // set to default port
    else {
        server->port = DEFAULT_PORT;
        cerver_log_msg (stdout, WARNING, SERVER, 
            c_string_create ("No port found. Setting port to default value: %i", DEFAULT_PORT));
    } 

    char *queue = config_get_entity_value (cfgEntity, "queue");
    if (queue) {
        server->connectionQueue = atoi (queue);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
            c_string_create ("Connection queue: %i", server->connectionQueue));
        #endif
        free (queue);
    } 
    else {
        server->connectionQueue = DEFAULT_CONNECTION_QUEUE;
        cerver_log_msg (stdout, WARNING, SERVER, 
            c_string_create ("Connection queue no specified. Setting it to default: %i", 
                DEFAULT_CONNECTION_QUEUE));
    }

    char *timeout = config_get_entity_value (cfgEntity, "timeout");
    if (timeout) {
        server->pollTimeout = atoi (timeout);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
            c_string_create ("Server poll timeout: %i", server->pollTimeout));
        #endif
        free (timeout);
    }
    else {
        server->pollTimeout = DEFAULT_POLL_TIMEOUT;
        cerver_log_msg (stdout, WARNING, SERVER, 
            c_string_create ("Poll timeout no specified. Setting it to default: %i", 
                DEFAULT_POLL_TIMEOUT));
    }

    char *auth = config_get_entity_value (cfgEntity, "authentication");
    if (auth) {
        server->authRequired = atoi (auth);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, server->authRequired == 1 ? 
            "Server requires client authentication" : "Server does not requires client authentication");
        #endif
        free (auth);
    }
    else {
        server->authRequired = DEFAULT_REQUIRE_AUTH;
        cerver_log_msg (stdout, WARNING, SERVER, 
            "No auth option found. No authentication required by default.");
    }

    if (server->authRequired) {
        char *tries = config_get_entity_value (cfgEntity, "authTries");
        if (tries) {
            server->auth.maxAuthTries = atoi (tries);
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
                c_string_create ("Max auth tries set to: %i.", server->auth.maxAuthTries));
            #endif
            free (tries);
        }
        else {
            server->auth.maxAuthTries = DEFAULT_AUTH_TRIES;
            cerver_log_msg (stdout, WARNING, SERVER, 
                c_string_create ("Max auth tries set to default: %i.", DEFAULT_AUTH_TRIES));
        }
    }

    char *sessions = config_get_entity_value (cfgEntity, "sessions");
    if (sessions) {
        server->useSessions = atoi (sessions);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, server->useSessions == 1 ? 
            "Server supports client sessions." : "Server does not support client sessions.");
        #endif
        free (sessions);
    }
    else {
        server->useSessions = DEFAULT_USE_SESSIONS;
        cerver_log_msg (stdout, WARNING, SERVER, 
            "No sessions option found. No support for client sessions by default.");
    }

    return 0;

}

// inits a server of a given type
static u8 cerver_init (Server *server, Config *cfg, ServerType type) {

    int retval = 1;

    if (server) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Initializing server...");
        #endif

        if (cfg) {
            ConfigEntity *cfgEntity = config_get_entity_with_id (cfg, type);
            if (!cfgEntity) {
                cerver_log_msg (stderr, ERROR, SERVER, "Problems with server config!");
                return 1;
            } 

            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Using config entity to set server values...");
            #endif

            if (!getServerCfgValues (server, cfgEntity)) 
                cerver_log_msg (stdout, SUCCESS, SERVER, "Done getting cfg server values");
        }

        // log server values
        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Use IPv6: %i", server->useIpv6));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Listening on port: %i", server->port));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Connection queue: %i", server->connectionQueue));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Server poll timeout: %i", server->pollTimeout));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, server->authRequired == 1 ? 
                "Server requires client authentication" : "Server does not requires client authentication");
            if (server->authRequired) 
                cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
                c_string_create ("Max auth tries set to: %i.", server->auth.maxAuthTries));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, server->useSessions == 1 ? 
                "Server supports client sessions." : "Server does not support client sessions.");
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

            default: cerver_log_msg (stderr, ERROR, SERVER, "Unkonw protocol type!"); return 1;
        }
        
        if (server->serverSock < 0) {
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to create server socket!");
            return 1;
        }

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Created server socket");
        #endif

        // set the socket to non blocking mode
        if (!sock_setBlocking (server->serverSock, server->blocking)) {
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to set server socket to non blocking mode!");
            close (server->serverSock);
            return 1;
        }

        else {
            server->blocking = false;
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Server socket set to non blocking mode.");
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
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to bind server socket!");
            return 1;
        }   

        if (initServerDS (server, type))  {
            cerver_log_msg (stderr, ERROR, NO_TYPE, "Failed to init server data structures!");
            return 1;
        }

        server->type = type;
        initServerValues (server, server->type);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Done creating server data structures...");
        #endif
    }

    return retval;

}

// cerver constructor, with option to init with some values
Server *cerver_new (Server *cerver) {

    Server *c = (Server *) malloc (sizeof (Server));
    if (c) {
        memset (c, 0, sizeof (Server));

        // init with some values
        if (cerver) {
            c->useIpv6 = cerver->useIpv6;
            c->protocol = cerver->protocol;
            c->port = cerver->port;
            c->connectionQueue = cerver->connectionQueue;
            c->pollTimeout = cerver->pollTimeout;
            c->authRequired = cerver->authRequired;
            c->type = cerver->type;
        }

        c->name = NULL;
        c->destroyServerData = NULL;

        // by default the socket is assumed to be a blocking socket
        c->blocking = true;

        c->authRequired = DEFAULT_REQUIRE_AUTH;
        c->useSessions = DEFAULT_USE_SESSIONS;
        
        c->isRunning = false;
    }

    return c;

}

void cerver_delete (Server *cerver) {

    if (cerver) {
        // TODO: what else do we want to delete here?
        str_delete (cerver->name);

        free (cerver);
    }

}

// creates a new server of the specified type and with option for a custom name
// also has the option to take another cerver as a paramater
// if no cerver is passed, configuration will be read from config/server.cfg
Server *cerver_create (ServerType type, const char *name, Server *cerver) {

    Server *c = NULL;

    // create a server with the requested parameters
    if (cerver) {
        c = cerver_new (cerver);
        if (!cerver_init (c, NULL, type)) {
            if (name) c->name = str_new (name);
            log_server (c);
        }

        else {
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to init the server!");
            cerver_delete (c);   // delete the failed server...
        }
    }

    // create the server from the default config file
    else {
        Config *serverConfig = config_parse_file (SERVER_CFG);
        if (serverConfig) {
            c = cerver_new (NULL);
            if (!cerver_init (c, serverConfig, type)) {
                if (name) c->name = str_new (name);
                log_server (c);
                config_destroy (serverConfig);
            }

            else {
                cerver_log_msg (stderr, ERROR, SERVER, "Failed to init the server!");
                config_destroy (serverConfig);
                cerver_delete (c); 
            }
        } 

        else cerver_log_msg (stderr, ERROR, NO_TYPE, "Problems loading server config!\n");
    }

    return c;

}

// teardowns the server and creates a fresh new one with the same parameters
Server *cerver_restart (Server *server) {

    if (server) {
        cerver_log_msg (stdout, SERVER, NO_TYPE, "Restarting the server...");

        Server temp = { 
            .useIpv6 = server->useIpv6, .protocol = server->protocol, .port = server->port,
            .connectionQueue = server->connectionQueue, .type = server->type,
            .pollTimeout = server->pollTimeout, .authRequired = server->authRequired,
            .useSessions = server->useSessions };

        temp.name = str_new (server->name->str);

        if (!cerver_teardown (server)) cerver_log_msg (stdout, SUCCESS, SERVER, "Done with server teardown");
        else cerver_log_msg (stderr, ERROR, SERVER, "Failed to teardown the server!");

        // what ever the output, create a new server --> restart
        Server *retServer = cerver_new (&temp);
        if (!cerver_init (retServer, NULL, temp.type)) {
            cerver_log_msg (stdout, SUCCESS, SERVER, "Server has restarted!");
            return retServer;
        }

        else cerver_log_msg (stderr, ERROR, SERVER, "Unable to retstart the server!");
    }

    else cerver_log_msg (stdout, WARNING, SERVER, "Can't restart a NULL server!");
    
    return NULL;

}

// TODO: handle logic when we have a load balancer --> that will be the one in charge to listen for
// connections and accept them -> then it will send them to the correct server
// TODO: 13/10/2018 -- we can only handle a tcp server
// depending on the protocol, the logic of each server might change...
u8 cerver_start (Server *server) {

    if (server->isRunning) {
        cerver_log_msg (stdout, WARNING, SERVER, "The server is already running!");
        return 1;
    }

    // one time only inits
    // if we have a game server, we might wanna load game data -> set by server admin
    if (server->type == GAME_SERVER) {
        GameServerData *game_data = (GameServerData *) server->serverData;
        if (game_data && game_data->load_game_data) {
            game_data->load_game_data (NULL);
        }

        else cerver_log_msg (stdout, WARNING, GAME, "Game server doesn't have a reference to a game data!");
    }

    u8 retval = 1;
    switch (server->protocol) {
        case IPPROTO_TCP: {
            if (server->blocking == false) {
                if (!listen (server->serverSock, server->connectionQueue)) {
                    // set up the initial listening socket     
                    server->fds[server->nfds].fd = server->serverSock;
                    server->fds[server->nfds].events = POLLIN;
                    server->nfds++;

                    server->isRunning = true;

                    if (server->authRequired) server->holdingClients = false;

                    server_poll (server);

                    retval = 0;
                }

                else {
                    cerver_log_msg (stderr, ERROR, SERVER, "Failed to listen in server socket!");
                    close (server->serverSock);
                    retval = 1;
                }
            }

            else {
                cerver_log_msg (stderr, ERROR, SERVER, "Server socket is not set to non blocking!");
                retval = 1;
            }
            
        } break;

        case IPPROTO_UDP: /* TODO: */ break;

        default: 
            cerver_log_msg (stderr, ERROR, SERVER, "Cant't start server! Unknown protocol!");
            retval = 1;
            break;
    }

    return retval;

}

// disable socket I/O in both ways and stop any ongoing job
u8 cerver_shutdown (Server *server) {

    if (server->isRunning) {
        server->isRunning = false; 

        // close the server socket
        if (!close (server->serverSock)) {
            #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, DEBUG_MSG, SERVER, "The server socket has been closed.");
            #endif

            return 0;
        }

        else cerver_log_msg (stdout, ERROR, SERVER, "Failed to close server socket!");
    } 

    return 1;

}

// cleans up the client's structures in the current server
// if ther are clients connected, we send a server teardown packet
static void cerver_destroy_clients (Server *server) {

    // create server teardown packet
    size_t packetSize = sizeof (PacketHeader);
    void *packet = generatePacket (SERVER_TEARDOWN, packetSize);

    // send a packet to any active client
    broadcastToAllClients (server->clients->root, server, packet, packetSize);
    
    // clear the client's poll  -> to stop any connection
    // this may change to a for if we have a dynamic poll structure
    memset (server->fds, 0, sizeof (server->fds));

    // destroy the active clients tree
    // avl_clear_tree (&server->clients->root, server->clients->destroy);
    avl_delete (server->clients);

    if (server->authRequired) {
        // send a packet to on hold clients
        broadcastToAllClients (server->onHoldClients->root, server, packet, packetSize);
        // destroy the tree
        // avl_clear_tree (&server->onHoldClients->root, server->onHoldClients->destroy);
        avl_delete (server->onHoldClients);

        // clear the on hold client's poll
        // this may change to a for if we have a dynamic poll structure
        memset (server->hold_fds, 0, sizeof (server->fds));
    }

    // the pool has only "empty" clients
    pool_clear (server->clientsPool);

    free (packet);

}

// teardown a server -> stop the server and clean all of its data
u8 cerver_teardown (Server *server) {

    if (!server) {
        cerver_log_msg (stdout, ERROR, SERVER, "Can't destroy a NULL server!");
        return 1;
    }

    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, SERVER, NO_TYPE, "Init server teardown...");
    #endif

    // TODO: what happens if we have a custom auth method?
    // clean server auth data
    if (server->authRequired) 
        if (server->auth.reqAuthPacket) free (server->auth.reqAuthPacket);

    // clean independent server type structs
    // if the server admin added a destroy server data action...
    if (server->destroyServerData) server->destroyServerData (server);
    else {
        // FIXME:
        // we use the default destroy server data function
        // switch (server->type) {
        //     case GAME_SERVER: 
        //         if (!destroyGameServer (server))
        //             cerver_log_msg (stdout, SUCCESS, SERVER, "Done clearing game server data!"); 
        //         break;
        //     case FILE_SERVER: break;
        //     case WEB_SERVER: break;
        //     default: break; 
        // }
    }

    // clean common server structs
    cerver_destroy_clients (server);
    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Done cleaning up clients.");
    #endif

    // disable socket I/O in both ways and stop any ongoing job
    if (!cerver_shutdown (server))
        cerver_log_msg (stdout, SUCCESS, SERVER, "Server has been shutted down.");

    else cerver_log_msg (stderr, ERROR, SERVER, "Failed to shutdown server!");
    
    if (server->thpool) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
            c_string_create ("Server active thpool threads: %i", 
            thpool_num_threads_working (server->thpool)));
        #endif

        // FIXME: 04/12/2018 - 15:02 -- getting a segfault
        thpool_destroy (server->thpool);

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Destroyed server thpool!");
        #endif
    } 

    // destroy any other server data
    if (server->packetPool) pool_clear (server->packetPool);
    if (server->serverInfo) free (server->serverInfo);

    if (server->name) free (server->name);

    free (server);

    cerver_log_msg (stdout, SUCCESS, NO_TYPE, "Server teardown was successfull!");

    return 0;

}

#pragma endregion

/*** LOAD BALANCER ***/

#pragma region LOAD BALANCER

// TODO:

#pragma endregion