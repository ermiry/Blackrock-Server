#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdint.h>

#include "network.h"

#include "utils/list.h"
#include "utils/config.h"
#include "utils/objectPool.h"
#include "utils/vector.h"

#define EXIT_FAILURE    1

#define THREAD_OK       0

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef unsigned char asciiChar;

#define DEFAULT_PROTOCOL                IPPROTO_TCP
#define DEFAULT_PORT                    7001
#define DEFAULT_CONNECTION_QUEUE        7
#define DEFAULT_POLL_TIMEOUT            180000      // 3 min in mili secs

#define MAX_PORT_NUM            65535
#define MAX_UDP_PACKET_SIZE     65515

#define poll_n_fds      100           // n of fds for the pollfd array

/*** CLIENT ***/

#pragma region CLIENT

// anyone that connects to the server
typedef struct Client {

    u32 clientID;
    i32 clientSock;
    struct sockaddr_storage address;

} Client;

#pragma endregion

/*** SEVER ***/

#pragma region SERVER

typedef enum ServerType {

    FILE_SERVER = 1,
    WEB_SERVER, 
    GAME_SERVER

} ServerType;

#define GS_LOBBY_POOL_INIT      1   // n lobbys to init the lobby pool with
#define GS_PLAYER_POOL_INT      2   // n players to init the player pool with

typedef struct GameServerData {

    Config *gameSettingsConfig;     // stores game modes info

    // TODO: do we want these here?
    // const packet sizes
    size_t lobbyPacketSize;
    size_t updatedGamePacketSize;
    size_t playerInputPacketSize;

    Pool *lobbyPool;        // 21/10/2018 -- 22:04 -- each game server has its own pool
    List *currentLobbys;    // a list of the current lobbys

    Pool *playersPool;      // 22/10/2018 -- each server has its own player's pool
    List *players;          // players connected to the server, but outside a lobby -> 24/10/2018

} GameServerData;

// this is the generic server struct, used to create different server types
typedef struct Server {

    i32 serverSock;         // server socket
    u8 useIpv6;  
    u8 protocol;            // 12/10/2018 - we only support either tcp or udp
    u16 port; 
    u16 connectionQueue;    // each server can handle connection differently

    bool isRunning;         // 19/10/2018 - the server is recieving and/or sending packetss
    bool blocking;          // 29/10/2018 - sokcet fd is blocking?

    // 28/10/2018 -- poll test
    struct pollfd fds[poll_n_fds];      // TODO: add the n_fds option in the cfg file
    u16 nfds;                           // n of active fds in the pollfd array
    u32 pollTimeout;                    

    ServerType type;
    void *serverData;
    void (*destroyServerData) (void *data);

    // 01/11/2018 - lets try this
    // do web servers need this?
    List *clients;              // connected clients
    Pool *clientsPool;
    List *onHoldClients;        // hold on the clients until they authenticate

    // 02/11/2018 - packet info
    Pool *packetPool;

} Server;

extern Server *cerver_createServer (Server *, ServerType, void (*destroyServerdata) (void *data));

extern u8 cerver_startServer (Server *);

extern void cerver_shutdownServer (Server *);
extern u8 cerver_teardown (Server *);
extern Server *cerver_restartServer (Server *);

#pragma endregion

/*** LOAD BALANCER ***/

#pragma region LOAD BALANCER

// this is the generic load balancer struct, used to configure a load balancer
typedef struct LoadBalancer {

    i32 lbsock;             // lb socket
    u8 useIpv6;  
    u8 protocol;            // 12/10/2018 - we only support either tcp or udp
    u16 port; 
    u16 connectionQueue;    // number of queue connections

    bool isRunning;         // 22/10/2018 - the lb is handling the traffic

    List *servers;          // list of servers managed by the load balancer

    Pool *clientPool;       // does the load balancer handles clients directly??

    // 20/10/2018 -- i dont like this...
    // Vector holdClients;     // hold on the clients until they authenticate

    void (*destroyLoadBalancer) (void *data);   // ptr to a custom functipn to destroy the lb?

    // TODO: 14/10/2018 - maybe we can have listen and handle connections as generic functions, also a generic function
    // to recieve packets and specific functions to cast the packet to the type that we need?

} LoadBalancer;

#pragma endregion

/*** PACKETS ***/

#pragma region PACKETS

// 01/11/2018 - info from a recieved packet to be handle
typedef struct PacketInfo {

    Server *server;
    Client *client;
    char *packetData;
    size_t packetSize;

} PacketInfo;

typedef u32 ProtocolId;

typedef struct Version {

	u16 major;
	u16 minor;
	
} Version;

extern ProtocolId PROTOCOL_ID;
extern Version PROTOCOL_VERSION;

// TODO: I think we can use this for manu more applications?
// maybe something like our request type?

// 01/11/2018 -- this indicates what type of packet we are sending/recieving
// for exmaple if it is an error packet, a game packet, an auth packet or any other
// type of request
typedef enum PacketType {

    ERROR_PACKET = 1,
	REQUEST,
    AUTHENTICATION,
    GAME_PACKET,

    SERVER_TEARDOWN,    // FIXME: create a better packet type for server functions

    TEST_PACKET = 100,

} PacketType;

typedef struct PacketHeader {

	ProtocolId protocolID;
	Version protocolVersion;
	PacketType packetType;

} PacketHeader;

// TODO: Change the name
// 01/11/2018
// this indicates the data and more info about the packet type
// for example if we have a game packet type, with this we can send a create lobby packet or join lobby
// if we have an authenticate packet, here goes the data of a success authentication or a failed one

// the type of request we want to make to the server
typedef enum RequestType {

    REQ_GET_FILE = 1,
    POST_SEND_FILE,
    
    REQ_AUTH_CLIENT,

    LOBBY_CREATE,
    LOBBY_JOIN,
    LOBBY_UPDATE,
    LOBBY_DESTROY,

} RequestType;

// here we can add things like file names or game types
typedef struct RequestData {

    RequestType type;

} RequestData;

// 23/10/2018 -- lests test how this goes...
typedef enum ErrorType {

    ERR_SERVER_ERROR = 0,   // internal server error, like no memory

    ERR_CREATE_LOBBY = 1,
    ERR_JOIN_LOBBY,

} ErrorType;

typedef struct ErrorData {

    ErrorType type;
    char msg[256];

} ErrorData;

extern void sendPacket (Server *server, void *begin, size_t packetSize, struct sockaddr_storage address);
extern u8 sendErrorPacket (Server *server, Client *client, ErrorType type, char *msg);

extern void *createLobbyPacket (PacketType packetType, Lobby *lobby, size_t packetSize); 

#pragma endregion

/*** GAME SERVER ***/

extern u16 nextPlayerId;

extern void recievePackets (void);  // FIXME: is this a game server specific?

extern void checkTimeouts (void);
extern void sendGamePackets (Server *server, int to) ;

extern void destroyGameServer (void *data);


/*** SERIALIZATION ***/

// 24/10/2018 -- lets try how this works --> our goal with serialization is to send 
// a packet without our data structures but whitout any ptrs

typedef struct SLobby {

    GameSettings settings;      // 24/10/2018 -- we dont have any ptr in this struct
    bool inGame;

    // FIXME: how do we want to send this info?
    // Player owner;               // how do we want to send which is the owner
    // Vector players;             // ecah client also needs to keep track of other players in the lobby

} SLobby;


/*** TESTING ***/

#include "utils/thpool.h"

extern threadpool thpool;

extern void *authPacket;
extern size_t authPacketSize;
extern void *generateClientAuthPacket ();

#endif