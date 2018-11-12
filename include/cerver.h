#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdint.h>

#include "network.h"
#include "game.h"       // game server

#include "utils/list.h"
#include "utils/config.h"
#include "utils/objectPool.h"
// #include "utils/vector.h"

#include <poll.h>
#include "utils/avl.h"      // 02/11/2018 -- using an avl tree to handle clients

// 04/11/2018 -- 23:04 -- including a th pool inside each server
#include "utils/thpool.h"

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

// takes no argument and returns a value (int)
typedef u8 (*Func)(void);
// takes an argument and does not return a value
typedef void (*Action)(void *);
// takes an argument and returns a value (int)
typedef u8 (*delegate)(void *);

#define DEFAULT_USE_IPV6                0
#define DEFAULT_PROTOCOL                IPPROTO_TCP
#define DEFAULT_PORT                    7001
#define DEFAULT_CONNECTION_QUEUE        7
#define DEFAULT_POLL_TIMEOUT            180000      // 3 min in mili secs

#define DEFAULT_REQUIRE_AUTH            0   // by default, a server does not requires authentication
#define DEFAULT_AUTH_TRIES              3   // by default, a client can try 3 times to authenticate 
#define DEFAULT_AUTH_CODE               7

#define DEFAULT_TH_POOL_INIT            4

#define MAX_PORT_NUM            65535
#define MAX_UDP_PACKET_SIZE     65515

#define poll_n_fds      100           // n of fds for the pollfd array

struct _Server;

/*** CLIENT ***/

#pragma region CLIENT

// anyone that connects to the server
struct _Client {

    i32 clientID;
    i32 clientSock;
    struct sockaddr_storage address;

    u8 authTries;           // remaining attemps to authenticate
    bool dropClient;        // client failed to authenticate

};

typedef struct _Client Client;

extern void client_unregisterFromServer (struct _Server *server, Client *client);

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

struct _GameServerData {

    Config *gameSettingsConfig;     // stores game modes info

    Pool *lobbyPool;        // 21/10/2018 -- 22:04 -- each game server has its own pool
    List *currentLobbys;    // a list of the current lobbys

    Pool *playersPool;      // 22/10/2018 -- each server has its own player's pool
    // List *players;          // players connected to the server, but outside a lobby -> 24/10/2018
    AVLTree *players;

};

typedef struct _GameServerData GameServerData;

// info for the server to perfom a correct client authentication
typedef struct Auth {

    void *reqAuthPacket;
    size_t authPacketSize;
    u8 maxAuthTries;                // client's chances of auth before being dropped
    delegate authenticate;          // authentication function

} Auth;

// this is the generic server struct, used to create different server types
struct _Server {

    i32 serverSock;         // server socket
    u8 useIpv6;  
    u8 protocol;            // 12/10/2018 - we only support either tcp or udp
    u16 port; 
    u16 connectionQueue;    // each server can handle connection differently

    bool isRunning;         // 19/10/2018 - the server is recieving and/or sending packetss
    bool blocking;          // 29/10/2018 - sokcet fd is blocking?

    ServerType type;
    void *serverData;
    Action destroyServerData;

    // 01/11/2018 - lets try this
    // do web servers need this?
    AVLTree *clients;                   // 02/11/2018 -- connected clients with avl tree
    Pool *clientsPool;       

    // 28/10/2018 -- poll test
    struct pollfd fds[poll_n_fds];      // TODO: add the n_fds option in the cfg file
    u16 nfds;                           // n of active fds in the pollfd array
    bool compress_clients;              // compress the fds array?
    u32 pollTimeout;           

    bool authRequired;      // 02/11/2018 - authentication required by the server
    Auth auth;              // server auth info

    // 02/11/2018 -- 21:51 -- on hold clients         
    AVLTree *onHoldClients;                 // hold on the clients until they authenticate
    struct pollfd hold_fds[poll_n_fds];
    u16 hold_nfds;
    bool compress_hold_clients;              // compress the hold fds array?

    Pool *packetPool;       // 02/11/2018 - packet info

    // 04/11/2018 -- 23:04 -- including a th pool inside each server
    threadpool thpool;

};

typedef struct _Server Server;

extern Server *cerver_createServer (Server *, ServerType, Action);

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
struct _PacketInfo {

    Server *server;
    Client *client;
    char packetData[MAX_UDP_PACKET_SIZE];
    size_t packetSize;

};

typedef struct _PacketInfo PacketInfo;

typedef u32 ProtocolId;

typedef struct Version {

	u16 major;
	u16 minor;
	
} Version;

extern ProtocolId PROTOCOL_ID;
extern Version PROTOCOL_VERSION;

// 01/11/2018 -- this indicates what type of packet we are sending/recieving
typedef enum PacketType {

    ERROR_PACKET = 1,
	REQUEST,
    AUTHENTICATION,
    GAME_PACKET,

    SERVER_TEARDOWN,    // FIXME: create a better packet type for server functions

    TEST_PACKET = 100,
    DONT_CHECK_TYPE,

} PacketType;

typedef struct PacketHeader {

	ProtocolId protocolID;
	Version protocolVersion;
	PacketType packetType;
    u32 packetSize;             // expected packet size

} PacketHeader;

// 01/11/2018 -- this indicates the data and more info about the packet type
typedef enum RequestType {

    REQ_GET_FILE = 1,
    POST_SEND_FILE,
    
    REQ_AUTH_CLIENT,

    LOBBY_CREATE,
    LOBBY_JOIN,
    LOBBY_LEAVE,
    LOBBY_UPDATE,
    LOBBY_DESTROY,

    GAME_INPUT_UPDATE,
    GAME_SEND_MSG,

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

    ERR_FAILED_AUTH,

} ErrorType;

typedef struct ErrorData {

    ErrorType type;
    char msg[256];

} ErrorData;

extern void *generatePacket (PacketType packetType, size_t packetSize);
extern u8 checkPacket (size_t packetSize, char *packetData, PacketType expectedType);

extern PacketInfo *newPacketInfo (Server *server, Client *client, char *packetData, size_t packetSize);

extern i8 tcp_sendPacket (i32 socket_fd, const void *begin, size_t packetSize, int flags);
extern i8 udp_sendPacket (Server *server, const void *begin, size_t packetSize, 
    const struct sockaddr_storage address);
extern u8 sendErrorPacket (Server *server, Client *client, ErrorType type, char *msg);

#pragma endregion

/*** GAME SERVER ***/

struct _GameSettings;
struct _Lobby;

extern u16 nextPlayerId;

extern void checkTimeouts (void);
extern void sendGamePackets (Server *server, int to) ;

extern void destroyGameServer (void *data);

extern void *createLobbyPacket (PacketType packetType, struct _Lobby *lobby, size_t packetSize); 


/*** SERIALIZATION ***/

// 24/10/2018 -- lets try how this works --> our goal with serialization is to send 
// a packet without our data structures but whitout any ptrs

typedef struct SLobby {

    // struct _GameSettings settings;      // 24/10/2018 -- we dont have any ptr in this struct
    bool inGame;

    // FIXME: how do we want to send this info?
    // Player owner;               // how do we want to send which is the owner
    // Vector players;             // ecah client also needs to keep track of other players in the lobby

} SLobby;

// 03/11/2018 -> default auth data to use by default auth function
typedef struct DefAuthData {

    u32 code;

} DefAuthData;

#endif