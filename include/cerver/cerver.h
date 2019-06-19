#ifndef _CERVER_CERVER_H_
#define _CERVER_CERVER_H_

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <poll.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/client.h"

#include "cerver/game/game.h"

#include "cerver/collections/dllist.h"
#include "cerver/collections/avl.h"

#include "cerver/utils/objectPool.h"
#include "cerver/utils/config.h"
#include "cerver/utils/thpool.h"

#define DEFAULT_USE_IPV6                0
#define DEFAULT_PORT                    7001
#define DEFAULT_CONNECTION_QUEUE        7
#define DEFAULT_POLL_TIMEOUT            180000      // 3 min in mili secs

#define DEFAULT_REQUIRE_AUTH            0   // by default, a server does not requires authentication
#define DEFAULT_AUTH_TRIES              3   // by default, a client can try 3 times to authenticate 
#define DEFAULT_AUTH_CODE               0x4CA140FF

#define DEFAULT_USE_SESSIONS            0   // by default, a server does not support client sessions

#define DEFAULT_TH_POOL_INIT            4

#define MAX_PORT_NUM            65535
#define MAX_UDP_PACKET_SIZE     65515

#define poll_n_fds      100           // n of fds for the pollfd array

/*** SEVER ***/

#pragma region SERVER

#ifdef RUN_FROM_MAKE
    #define SERVER_CFG          "./config/server.cfg"

#elif RUN_FROM_BIN
    #define SERVER_CFG          "../config/server.cfg" 

#else
    #define SERVER_CFG          ""
#endif  

typedef enum ServerType {

    FILE_SERVER = 1,
    WEB_SERVER, 
    GAME_SERVER

} ServerType;

// FIXME: move to auth.h and create constructors etc
// info for the server to perfom a correct client authentication
typedef struct Auth {

    void *req_auth_packet;
    size_t auth_packet_size;

    u8 max_auth_tries;                // client's chances of auth before being dropped
    delegate authenticate;            // authentication function

} Auth;

// this is the generic server struct, used to create different server types
struct _Cerver {

    i32 sock;               // server socket
    u8 use_ipv6;  
    Protocol protocol;            // we only support either tcp or udp
    u16 port; 
    u16 connection_queue;   // each server can handle connection differently

    bool isRunning;         // he server is recieving and/or sending packetss
    bool blocking;          // sokcet fd is blocking?

    ServerType type;
    void *server_data;
    Action delete_server_data;

    AVLTree *clients;                   // connected clients
    Pool *client_pool;       

    // TODO: option to make this dynamic
    struct pollfd fds[poll_n_fds];
    u16 nfds;                           // n of active fds in the pollfd array
    bool compress_clients;              // compress the fds array?
    u32 poll_timeout;           

    bool auth_required;      // does the server requires authentication?
    Auth auth;              // server auth info

    // on hold clients         
    AVLTree *onHoldClients;                 // hold on the clients until they authenticate
    // TODO: option to make this dynamic
    struct pollfd hold_fds[poll_n_fds];
    u16 hold_nfds;
    bool compress_hold_clients;              // compress the hold fds array?
    bool holdingClients;

    Pool *packetPool;

    threadpool thpool;

    void *serverInfo;           // useful info that we can send to clients 
    Action sendServerInfo;      // method to send server into to the client              

    // allow the clients to use sessions (have multiple connections)
    bool use_sessions;  
    // admin defined function to generate session ids bassed on usernames, etc             
    Action generateSessionID; 

    // the admin can define a function to handle the recieve buffer if they are using a custom protocol
    // otherwise, it will be set to the default one
    Action handle_recieved_buffer;

    // server info/stats
    // TODO: use this in the thpool names
    String *name;
    u32 connectedClients;
    u32 n_hold_clients;

};

typedef struct _Cerver Cerver;

/*** Cerver Configuration ***/

extern void cerver_set_auth_method (Cerver *cerver, delegate authMethod);

extern void cerver_set_handler_received_buffer (Cerver *cerver, Action handler);

extern void session_set_id_generator (Cerver *server, Action idGenerator);
extern char *session_default_generate_id (i32 fd, const struct sockaddr_storage address);

/*** Cerver Methods ***/

// cerver constructor, with option to init with some values
extern Server *cerver_new (Cerver *cerver);
extern void cerver_delete (Cerver *cerver);

// creates a new cerver of the specified type and with option for a custom name
// also has the option to take another cerver as a paramater
// if no cerver is passed, configuration will be read from config/server.cfg
extern Cerver *cerver_create (ServerType type, const char *name, Cerver *cerver);

// teardowns the cerver and creates a fresh new one with the same parameters
extern Cerver *cerver_restart (Cerver *cerver);

// starts the cerver
extern u8 cerver_start (Cerver *cerver);

// disable socket I/O in both ways and stop any ongoing job
extern u8 cerver_shutdown (Cerver *cerver);

// teardown a server -> stop the server and clean all of its data
extern u8 cerver_teardown (Cerver *cerver);

#pragma endregion

/*** PACKETS ***/

#pragma region PACKETS

// info from a recieved packet to be handle
struct _PacketInfo {

    Server *server;
    Client *client;
    i32 clientSock; 
    char *packetData;
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

// this indicates what type of packet we are sending/recieving
typedef enum PacketType {

    SERVER_PACKET = 0,
    CLIENT_PACKET,
    ERROR_PACKET,
	REQUEST,
    AUTHENTICATION,
    GAME_PACKET,

    APP_ERROR_PACKET,
    APP_PACKET,

    TEST_PACKET = 100,
    DONT_CHECK_TYPE,

} PacketType;

typedef struct PacketHeader {

    ProtocolId protocolID;
	Version protocolVersion;
	PacketType packetType;
    size_t packetSize;             // expected packet size

} PacketHeader;

// this indicates the data and more info about the packet type
typedef enum RequestType {

    SERVER_INFO = 0,
    SERVER_TEARDOWN,

    CLIENT_DISCONNET,

    REQ_GET_FILE,
    POST_SEND_FILE,
    
    REQ_AUTH_CLIENT,
    CLIENT_AUTH_DATA,
    SUCCESS_AUTH,

    LOBBY_CREATE,
    LOBBY_JOIN,
    LOBBY_LEAVE,
    LOBBY_UPDATE,
    LOBBY_NEW_OWNER,
    LOBBY_DESTROY,

    GAME_INIT,      // prepares the game structures
    GAME_START,     // strat running the game
    GAME_INPUT_UPDATE,
    GAME_SEND_MSG,

} RequestType;
 
// here we can add things like file names or game types
typedef struct RequestData {

    RequestType type;

} RequestData;

typedef enum ErrorType {

    ERR_SERVER_ERROR = 0,   // internal server error, like no memory

    ERR_CREATE_LOBBY = 1,
    ERR_JOIN_LOBBY,
    ERR_LEAVE_LOBBY,
    ERR_FIND_LOBBY,

    ERR_GAME_INIT,

    ERR_FAILED_AUTH,

} ErrorType;

typedef struct ErrorData {

    ErrorType type;
    char msg[256];

} ErrorData;

extern void initPacketHeader (void *header, PacketType type, u32 packetSize);
extern void *generatePacket (PacketType packetType, size_t packetSize);
extern u8 checkPacket (size_t packetSize, char *packetData, PacketType expectedType);

extern PacketInfo *newPacketInfo (Server *server, Client *client, i32 clientSock,
    char *packetData, size_t packetSize);

extern i8 tcp_sendPacket (i32 socket_fd, const void *begin, size_t packetSize, int flags);
extern i8 udp_sendPacket (Server *server, const void *begin, size_t packetSize, 
    const struct sockaddr_storage address);
extern u8 server_sendPacket (Server *server, i32 socket_fd, struct sockaddr_storage address, 
    const void *packet, size_t packetSize);

extern void *generateErrorPacket (ErrorType type, const char *msg);
extern u8 sendErrorPacket (Server *server, i32 sock_fd, struct sockaddr_storage address,
    ErrorType type, const char *msg);

#pragma endregion

/*** SERIALIZATION ***/

#pragma region SERIALIZATION

// 24/10/2018 -- lets try how this works --> our goal with serialization is to send 
// a packet without our data structures but whitout any ptrs

// 17/11/2018 - send useful server info to the client trying to connect
typedef struct SServer {

    u8 useIpv6;  
    u8 protocol;            // we only support either tcp or udp
    u16 port; 

    ServerType type;
    bool authRequired;      // authentication required by the server

} SServer;

// default auth data to use by default auth function
typedef struct DefAuthData {

    u32 code;

} DefAuthData;

// session id - token
typedef struct Token {

    char token[65];

} Token;

#pragma endregion

#endif