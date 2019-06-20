#ifndef _CERVER_CERVER_H_
#define _CERVER_CERVER_H_

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <poll.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/auth.h"

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

#define DEFAULT_REQUIRE_AUTH            false   // by default, a server does not requires authentication
#define DEFAULT_AUTH_TRIES              3   // by default, a client can try 3 times to authenticate 
#define DEFAULT_AUTH_CODE               0x4CA140FF

#define DEFAULT_USE_SESSIONS            false   // by default, a server does not support client sessions

#define DEFAULT_TH_POOL_INIT            4

#define MAX_PORT_NUM            65535
#define MAX_UDP_PACKET_SIZE     65515

#define poll_n_fds      100           // n of fds for the pollfd array

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
    Auth *auth;              // server auth info

    // on hold clients         
    AVLTree *on_hold_clients;                 // hold on the clients until they authenticate
    // FIXME: make this dynamic using max_on_hold_clients as the parameter
    struct pollfd hold_fds[poll_n_fds];
    u32 max_on_hold_clients;
    u16 current_on_hold_nfds;
    bool compress_hold_clients;              // compress the hold fds array?
    bool holding_clients;

    Pool *packetPool;

    threadpool thpool;

    // allow the clients to use sessions (have multiple connections)
    bool use_sessions;  
    // admin defined function to generate session ids bassed on usernames, etc             
    Action session_id_generator; 

    // the admin can define a function to handle the recieve buffer if they are using a custom protocol
    // otherwise, it will be set to the default one
    // Action handle_recieved_buffer;

    // server info/stats
    // TODO: use this in the thpool names
    String *name;
    String *welcome_msg;                    // this msg is sent to the client when it first connects
    Packet *cerver_info_packet;             // useful info that we can send to clients 
    u32 n_connected_clients;
    u32 n_hold_clients;

};

typedef struct _Cerver Cerver;

/*** Cerver Methods ***/

// cerver constructor, with option to init with some values
extern Server *cerver_new (Cerver *cerver);
extern void cerver_delete (void *ptr);

// sets the cerver msg to be sent when a client connects
extern void cerver_set_welcome_msg (Cerver *cerver, const char *msg);

// configures the cerver to require client authentication upon new client connections
extern u8 cerver_set_auth (Cerver *cerver, u8 max_auth_tries, delegate authenticate);

// configures the cerver to use client sessions
extern u8 cerver_set_sessions (Cerver *cerver, Action session_id_generator);

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

/*** Serialization ***/

// serialized cerver structure
typedef struct SCerver {

    bool use_ipv6;  
    Protocol protocol;
    u16 port; 

    char name[32];
    ServerType type;
    bool auth_required;

    bool uses_sessions;

} SCerver;

// session id - token
typedef struct Token {

    char token[64];

} Token;

#endif