#ifndef _CERVER_CERVER_H_
#define _CERVER_CERVER_H_

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <poll.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/network.h"
#include "cerver/packets.h"
#include "cerver/auth.h"

#include "cerver/game/game.h"

#include "cerver/collections/avl.h"
#include "cerver/collections/htab.h"

#include "cerver/utils/config.h"
#include "cerver/utils/thpool.h"

#define DEFAULT_USE_IPV6                0
#define DEFAULT_PORT                    7001
#define DEFAULT_CONNECTION_QUEUE        7
#define DEFAULT_POLL_TIMEOUT            180000      // 3 min in mili secs

#define DEFAULT_TH_POOL_INIT            8

#define MAX_PORT_NUM            65535
#define MAX_UDP_PACKET_SIZE     65515

#define poll_n_fds              100           // n of fds for the pollfd array

typedef enum ServerType {

    FILE_SERVER = 1,
    WEB_SERVER, 
    GAME_SERVER

} ServerType;

// this is the generic server struct, used to create different server types
struct _Cerver {

    i32 sock;                           // server socket
    u8 use_ipv6;  
    Protocol protocol;                  // we only support either tcp or udp
    u16 port; 
    u16 connection_queue;               // each server can handle connection differently

    bool isRunning;                     // the server is recieving and/or sending packetss
    bool blocking;                      // sokcet fd is blocking?

    ServerType type;
    void *server_data;
    Action delete_server_data;

    threadpool thpool;

    AVLTree *clients;                   // connected clients 
    Htab *client_sock_fd_map;           // direct indexing by sokcet fd as key
    // action to be performed when a new client connects
    Action on_client_connected;   
    void *on_client_connected_data;
    Action delete_on_client_connected_data;

    /*** auth ***/
    struct pollfd *fds;
    u32 max_n_fds;                      // current max n fds in pollfd
    u16 current_n_fds;                  // n of active fds in the pollfd array
    bool compress_clients;              // compress the fds array?
    u32 poll_timeout;           

    bool auth_required;                 // does the server requires authentication?
    Auth *auth;                         // server auth info
     
    AVLTree *on_hold_connections;       // hold on the connections until they authenticate
    struct pollfd *hold_fds;
    u32 max_on_hold_connections;
    u16 current_on_hold_nfds;
    bool compress_on_hold;              // compress the hold fds array?
    bool holding_connections;

    // allow the clients to use sessions (have multiple connections)
    bool use_sessions;  
    // admin defined function to generate session ids bassed on usernames, etc             
    void *(*session_id_generator) (void *);

    // the admin can define a function to handle the recieve buffer if they are using a custom protocol
    // otherwise, it will be set to the default one
    // Action handle_recieved_buffer;

    /*** server info/stats ***/
    String *name;
    String *welcome_msg;                 // this msg is sent to the client when it first connects
    Packet *cerver_info_packet;          // useful info that we can send to clients 
    u32 n_connected_clients;
    u32 n_hold_clients;

    // TODO:
    // add time started
    // add uptime
    // maybe a function to be executed every x time -> as an update

};

typedef struct _Cerver Cerver;

/*** Cerver Methods ***/

// cerver constructor, with option to init with some values
extern Server *cerver_new (Cerver *cerver);
extern void cerver_delete (void *ptr);

// sets the cerver msg to be sent when a client connects
// retuns 0 on success, 1 on error
extern u8 cerver_set_welcome_msg (Cerver *cerver, const char *msg);

// sets an action to be performed by the cerver when a new client connects
extern u8 cerver_set_on_client_connected  (Cerver *cerver, 
    Action on_client_connected, void *data, Action delete_data);

// configures the cerver to require client authentication upon new client connections
// retuns 0 on success, 1 on error
extern u8 cerver_set_auth (Cerver *cerver, u8 max_auth_tries, delegate authenticate);

// configures the cerver to use client sessions
// retuns 0 on success, 1 on error
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

// creates a cerver info packet ready to be sent
extern Packet *cerver_packet_generate (Cerver *cerver);

#endif