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

/*** Sessions ***/

#pragma region Sessions

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

// the cerver admin can define a custom session id generator
void session_set_id_generator (Cerver *cerver, Action idGenerator) {

    if (cerver && idGenerator) {
        if (cerver->useSessions) 
            cerver->generateSessionID = idGenerator;

        else cerver_log_msg (stderr, ERROR, SERVER, "Cerver is not set to use sessions!");
    }

}

#pragma endregion

/*** SERVER ***/

// TODO: handle ipv6 configuration
// TODO: create some helpful timestamps
// TODO: add the option to log our output to a .log file

const char welcome[64] = "Welcome to cerver!";

// FIXME: packets and check that the data structures have been created correctly!
static u8 cerver_init_data_structures (Cerver *cerver) {

    u8 retval = 1;

    if (cerver) {
        if (cerver->use_sessions) cerver->clients = avl_init (client_comparator_sessionID, destroyClient);
        else cerver->clients = avl_init (client_comparator_clientID, destroyClient);

        cerver->client_pool = pool_init (destroyClient);

        cerver->packetPool = pool_init (destroyPacketInfo);
        // by default init the packet pool with some members
        PacketInfo *info = NULL;
        for (u8 i = 0; i < 3; i++) {
            info = (PacketInfo *) malloc (sizeof (PacketInfo));
            info->packetData = NULL;
            pool_push (cerver->packetPool, info);
        } 

        // initialize pollfd structures
        memset (cerver->fds, 0, sizeof (cerver->fds));
        cerver->nfds = 0;
        cerver->compress_clients = false;

        // set all fds as available spaces
        for (u16 i = 0; i < poll_n_fds; i++) cerver->fds[i].fd = -1;

        if (cerver->authRequired) {
            cerver->onHoldClients = avl_init (client_comparator_clientID, destroyClient);

            memset (cerver->hold_fds, 0, sizeof (cerver->hold_fds));
            cerver->hold_nfds = 0;
            cerver->compress_hold_clients = false;

            cerver->n_hold_clients = 0;

            for (u16 i = 0; i < poll_n_fds; i++) cerver->hold_fds[i].fd = -1;
        }

        // initialize cerver's own thread pool
        cerver->thpool = thpool_init (DEFAULT_TH_POOL_INIT);
        if (!cerver->thpool) {
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to init cerver's thread pool!");
            return 1;
        } 

        switch (type) {
            case FILE_SERVER: break;
            case WEB_SERVER: break;
            case GAME_SERVER: {
                // FIXME: correctly init the game cerver!!
                // GameServerData *gameData = (GameServerData *) malloc (sizeof (GameServerData));

                // // init the lobbys with n inactive in the pool
                // if (game_init_lobbys (gameData, GS_LOBBY_POOL_INIT)) {
                //     cerver_log_msg (stderr, ERROR, NO_TYPE, "Failed to init cerver lobbys!");
                //     return 1;
                // }

                // // init the players with n inactive in the pool
                // if (game_init_players (gameData, GS_PLAYER_POOL_INT)) {
                //     cerver_log_msg (stderr, ERROR, NO_TYPE, "Failed to init cerver players!");
                //     return 1;
                // }

                // cerver->serverData = gameData;
            } break;
            default: break;
        }

        retval = 0;     // success!!
    }

    return retval;

}

// depending on the type of cerver, we need to init some const values
static u8 cerver_init_values (Cerver *cerver) {

    u8 retval = 1;

    if (cerver) {
        cerver->handle_recieved_buffer = default_handle_recieved_buffer;

        if (cerver->authRequired) {
            cerver->auth.reqAuthPacket = createClientAuthReqPacket ();
            cerver->auth.authPacketSize = sizeof (PacketHeader) + sizeof (RequestData);

            cerver->auth.authenticate = defaultAuthMethod;
        }

        else {
            cerver->auth.reqAuthPacket = NULL;
            cerver->auth.authPacketSize = 0;
        }

        if (cerver->use_sessions) 
            cerver->generateSessionID = (void *) session_default_generate_id;

        cerver->serverInfo = generateServerInfoPacket (cerver);

        switch (cerver->type) {
            case FILE_SERVER: break;
            case WEB_SERVER: break;
            case GAME_SERVER: {
                GameServerData *data = (GameServerData *) cerver->serverData;

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

        cerver->connectedClients = 0;

        retval = 0;     // success!!
    }

    return retval;

}

static u8 cerver_get_cfg_values (Cerver *cerver, ConfigEntity *cfgEntity) {

    if (!cerver || !cfgEntity) return 1;

    char *ipv6 = config_get_entity_value (cfgEntity, "ipv6");
    if (ipv6) {
        cerver->use_ipv6 = atoi (ipv6);
        // if we have got an invalid value, the default is not to use ipv6
        if (cerver->use_ipv6 != 0 || cerver->use_ipv6 != 1) cerver->use_ipv6 = 0;

        free (ipv6);
    } 
    // if we do not have a value, use the default
    else cerver->use_ipv6 = DEFAULT_USE_IPV6;

    #ifdef CERVER_DEBUG
    cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Use IPv6: %i", cerver->useIpv6));
    #endif

    char *tcp = config_get_entity_value (cfgEntity, "tcp");
    if (tcp) {
        u8 usetcp = atoi (tcp);
        if (usetcp < 0 || usetcp > 1) {
            cerver_log_msg (stdout, WARNING, SERVER, "Unknown protocol. Using default: tcp protocol");
            usetcp = 1;
        }

        if (usetcp) cerver->protocol = PROTOCOL_TCP;
        else cerver->protocol = PROTOCOL_UDP;

        free (tcp);

    }

    // set to default (tcp) if we don't found a value
    else {
        cerver->protocol = PROTOCOL_TCP;
        cerver_log_msg (stdout, WARNING, SERVER, "No protocol found. Using default: TCP protocol");
    }

    char *port = config_get_entity_value (cfgEntity, "port");
    if (port) {
        cerver->port = atoi (port);
        // check that we have a valid range, if not, set to default port
        if (cerver->port <= 0 || cerver->port >= MAX_PORT_NUM) {
            cerver_log_msg (stdout, WARNING, SERVER, 
                c_string_create ("Invalid port number. Setting port to default value: %i", DEFAULT_PORT));
            cerver->port = DEFAULT_PORT;
        }

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Listening on port: %i", cerver->port));
        #endif
        free (port);
    }
    // set to default port
    else {
        cerver->port = DEFAULT_PORT;
        cerver_log_msg (stdout, WARNING, SERVER, 
            c_string_create ("No port found. Setting port to default value: %i", DEFAULT_PORT));
    } 

    char *queue = config_get_entity_value (cfgEntity, "queue");
    if (queue) {
        cerver->connection_queue = atoi (queue);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
            c_string_create ("Connection queue: %i", cerver->connection_queue));
        #endif
        free (queue);
    } 
    else {
        cerver->connection_queue = DEFAULT_CONNECTION_QUEUE;
        cerver_log_msg (stdout, WARNING, SERVER, 
            c_string_create ("Connection queue no specified. Setting it to default: %i", 
                DEFAULT_CONNECTION_QUEUE));
    }

    char *timeout = config_get_entity_value (cfgEntity, "timeout");
    if (timeout) {
        cerver->poll_timeout = atoi (timeout);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
            c_string_create ("Cerver poll timeout: %i", cerver->poll_timeout));
        #endif
        free (timeout);
    }
    else {
        cerver->poll_timeout = DEFAULT_POLL_TIMEOUT;
        cerver_log_msg (stdout, WARNING, SERVER, 
            c_string_create ("Poll timeout no specified. Setting it to default: %i", 
                DEFAULT_POLL_TIMEOUT));
    }

    char *auth = config_get_entity_value (cfgEntity, "authentication");
    if (auth) {
        cerver->auth_required = atoi (auth);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, cerver->auth_required == 1 ? 
            "Cerver requires client authentication" : "Cerver does not requires client authentication");
        #endif
        free (auth);
    }
    else {
        cerver->auth_required = DEFAULT_REQUIRE_AUTH;
        cerver_log_msg (stdout, WARNING, SERVER, 
            "No auth option found. No authentication required by default.");
    }

    if (cerver->auth_required) {
        char *tries = config_get_entity_value (cfgEntity, "authTries");
        if (tries) {
            cerver->auth.max_auth_tries = atoi (tries);
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
                c_string_create ("Max auth tries set to: %i.", cerver->auth.max_auth_tries));
            #endif
            free (tries);
        }
        else {
            cerver->auth.max_auth_tries = DEFAULT_AUTH_TRIES;
            cerver_log_msg (stdout, WARNING, SERVER, 
                c_string_create ("Max auth tries set to default: %i.", cerver->auth.max_auth_tries));
        }
    }

    char *sessions = config_get_entity_value (cfgEntity, "sessions");
    if (sessions) {
        cerver->use_sessions = atoi (sessions);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, cerver->use_sessions == 1 ? 
            "Cerver supports client sessions." : "Cerver does not support client sessions.");
        #endif
        free (sessions);
    }
    else {
        cerver->use_sessions = DEFAULT_USE_SESSIONS;
        cerver_log_msg (stdout, WARNING, SERVER, 
            "No sessions option found. No support for client sessions by default.");
    }

    return 0;

}

// inits the cerver networking capabilities
static u8 cerver_network_init (Cerver *cerver) {

    if (cerver) {
        // init the cerver with the selected protocol
        switch (cerver->protocol) {
            case IPPROTO_TCP: 
                cerver->sock = socket ((cerver->use_ipv6 == 1 ? AF_INET6 : AF_INET), SOCK_STREAM, 0);
                break;
            case IPPROTO_UDP:
                cerver->sock = socket ((cerver->use_ipv6 == 1 ? AF_INET6 : AF_INET), SOCK_DGRAM, 0);
                break;

            default: cerver_log_msg (stderr, ERROR, SERVER, "Unkonw protocol type!"); return 1;
        }
        
        if (cerver->sock < 0) {
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to create cerver socket!");
            return 1;
        }

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Created cerver socket");
        #endif

        // set the socket to non blocking mode
        if (!sock_set_blocking (cerver->sock, cerver->blocking)) {
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to set cerver socket to non blocking mode!");
            close (cerver->sock);
            return 1;
        }

        else {
            cerver->blocking = false;
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Cerver socket set to non blocking mode.");
            #endif
        }

        struct sockaddr_storage address;
        memset (&address, 0, sizeof (struct sockaddr_storage));

        if (cerver->use_ipv6) {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &address;
            addr->sin6_family = AF_INET6;
            addr->sin6_addr = in6addr_any;
            addr->sin6_port = htons (cerver->port);
        } 

        else {
            struct sockaddr_in *addr = (struct sockaddr_in *) &address;
            addr->sin_family = AF_INET;
            addr->sin_addr.s_addr = INADDR_ANY;
            addr->sin_port = htons (cerver->port);
        }

        if ((bind (cerver->sock, (const struct sockaddr *) &address, sizeof (struct sockaddr_storage))) < 0) {
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to bind cerver socket!");
            return 1;
        }  

        return 0;       // success!!
    }

    return 1; 

}

// inits a cerver of a given type
static u8 cerver_init (Cerver *cerver, Config *cfg, ServerType type) {

    int retval = 1;

    if (cerver) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Initializing cerver...");
        #endif

        if (cfg) {
            ConfigEntity *cfgEntity = config_get_entity_with_id (cfg, type);
            if (!cfgEntity) {
                cerver_log_msg (stderr, ERROR, SERVER, "Problems with cerver config!");
                return 1;
            } 

            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Using config entity to set cerver values...");
            #endif

            if (!cerver_get_cfg_values (cerver, cfgEntity)) 
                cerver_log_msg (stdout, SUCCESS, SERVER, "Done getting cfg cerver values");
        }

        // log cerver values
        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Use IPv6: %i", cerver->useIpv6));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Listening on port: %i", cerver->port));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Connection queue: %i", cerver->connectionQueue));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, c_string_create ("Cerver poll timeout: %i", cerver->pollTimeout));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, cerver->authRequired == 1 ? 
                "Cerver requires client authentication" : "Cerver does not requires client authentication");
            if (cerver->authRequired) 
                cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
                c_string_create ("Max auth tries set to: %i.", cerver->auth.maxAuthTries));
            cerver_log_msg (stdout, DEBUG_MSG, SERVER, cerver->useSessions == 1 ? 
                "Cerver supports client sessions." : "Cerver does not support client sessions.");
            #endif
        }

        cerver->type = type;

        if (!cerver_network_init (cerver)) {
            if (!cerver_init_data_structures (cerver)) {
                if (!cerver_init_values (cerver)) {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
                        "Done initializing cerver data structures and values!");
                    #endif

                    retval = 0;     // success!!
                }

                else cerver_log_msg (stderr, ERROR, SERVER, "Failed to init cerver values!");
            }

            else cerver_log_msg (stderr, ERROR, SERVER, "Failed to init cerver data structures!");
        }
    }

    return retval;

}

// cerver constructor, with option to init with some values
Cerver *cerver_new (Cerver *cerver) {

    Cerver *c = (Cerver *) malloc (sizeof (Cerver));
    if (c) {
        memset (c, 0, sizeof (Cerver));

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

void cerver_delete (Cerver *cerver) {

    if (cerver) {
        // TODO: what else do we want to delete here?
        str_delete (cerver->name);

        free (cerver);
    }

}

// creates a new cerver of the specified type and with option for a custom name
// also has the option to take another cerver as a paramater
// if no cerver is passed, configuration will be read from config/cerver.cfg
Cerver *cerver_create (ServerType type, const char *name, Cerver *cerver) {

    Cerver *c = NULL;

    // create a cerver with the requested parameters
    if (cerver) {
        c = cerver_new (cerver);
        if (!cerver_init (c, NULL, type)) {
            if (name) c->name = str_new (name);
            log_server (c);
        }

        else {
            cerver_log_msg (stderr, ERROR, SERVER, "Failed to init the cerver!");
            cerver_delete (c);   // delete the failed cerver...
        }
    }

    // create the cerver from the default config file
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
                cerver_log_msg (stderr, ERROR, SERVER, "Failed to init the cerver!");
                config_destroy (serverConfig);
                cerver_delete (c); 
            }
        } 

        else cerver_log_msg (stderr, ERROR, NO_TYPE, "Problems loading cerver config!\n");
    }

    return c;

}

// teardowns the cerver and creates a fresh new one with the same parameters
Cerver *cerver_restart (Cerver *cerver) {

    if (cerver) {
        cerver_log_msg (stdout, SERVER, NO_TYPE, "Restarting the cerver...");

        Cerver temp = { 
            .use_ipv6 = cerver->use_ipv6, .protocol = cerver->protocol, .port = cerver->port,
            .connection_queue = cerver->connection_queue, .type = cerver->type,
            .poll_timeout = cerver->poll_timeout, .auth_required = cerver->auth_required,
            .use_sessions = cerver->use_sessions };

        temp.name = str_new (cerver->name->str);

        if (!cerver_teardown (cerver)) cerver_log_msg (stdout, SUCCESS, SERVER, "Done with cerver teardown");
        else cerver_log_msg (stderr, ERROR, SERVER, "Failed to teardown the cerver!");

        // what ever the output, create a new cerver --> restart
        Cerver *retServer = cerver_new (&temp);
        if (!cerver_init (retServer, NULL, temp.type)) {
            cerver_log_msg (stdout, SUCCESS, SERVER, "Cerver has restarted!");
            return retServer;
        }

        else cerver_log_msg (stderr, ERROR, SERVER, "Unable to retstart the cerver!");
    }

    else cerver_log_msg (stdout, WARNING, SERVER, "Can't restart a NULL cerver!");
    
    return NULL;

}

// TODO: handle logic when we have a load balancer --> that will be the one in charge to listen for
// connections and accept them -> then it will send them to the correct cerver
// TODO: 13/10/2018 -- we can only handle a tcp cerver
// depending on the protocol, the logic of each cerver might change...
u8 cerver_start (Cerver *cerver) {

    if (cerver->isRunning) {
        cerver_log_msg (stdout, WARNING, SERVER, "The cerver is already running!");
        return 1;
    }

    // one time only inits
    // if we have a game cerver, we might wanna load game data -> set by cerver admin
    if (cerver->type == GAME_SERVER) {
        GameServerData *game_data = (GameServerData *) cerver->serverData;
        if (game_data && game_data->load_game_data) {
            game_data->load_game_data (NULL);
        }

        else cerver_log_msg (stdout, WARNING, GAME, "Game cerver doesn't have a reference to a game data!");
    }

    u8 retval = 1;
    switch (cerver->protocol) {
        case IPPROTO_TCP: {
            if (cerver->blocking == false) {
                if (!listen (cerver->serverSock, cerver->connectionQueue)) {
                    // set up the initial listening socket     
                    cerver->fds[cerver->nfds].fd = cerver->serverSock;
                    cerver->fds[cerver->nfds].events = POLLIN;
                    cerver->nfds++;

                    cerver->isRunning = true;

                    if (cerver->authRequired) cerver->holdingClients = false;

                    server_poll (cerver);

                    retval = 0;
                }

                else {
                    cerver_log_msg (stderr, ERROR, SERVER, "Failed to listen in cerver socket!");
                    close (cerver->serverSock);
                    retval = 1;
                }
            }

            else {
                cerver_log_msg (stderr, ERROR, SERVER, "Cerver socket is not set to non blocking!");
                retval = 1;
            }
            
        } break;

        case IPPROTO_UDP: /* TODO: */ break;

        default: 
            cerver_log_msg (stderr, ERROR, SERVER, "Cant't start cerver! Unknown protocol!");
            retval = 1;
            break;
    }

    return retval;

}

// disable socket I/O in both ways and stop any ongoing job
u8 cerver_shutdown (Cerver *cerver) {

    if (cerver->isRunning) {
        cerver->isRunning = false; 

        // close the cerver socket
        if (!close (cerver->serverSock)) {
            #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, DEBUG_MSG, SERVER, "The cerver socket has been closed.");
            #endif

            return 0;
        }

        else cerver_log_msg (stdout, ERROR, SERVER, "Failed to close cerver socket!");
    } 

    return 1;

}

// cleans up the client's structures in the current cerver
// if ther are clients connected, we send a cerver teardown packet
static void cerver_destroy_clients (Cerver *cerver) {

    // create cerver teardown packet
    size_t packetSize = sizeof (PacketHeader);
    void *packet = generatePacket (SERVER_TEARDOWN, packetSize);

    // send a packet to any active client
    broadcastToAllClients (cerver->clients->root, cerver, packet, packetSize);
    
    // clear the client's poll  -> to stop any connection
    // this may change to a for if we have a dynamic poll structure
    memset (cerver->fds, 0, sizeof (cerver->fds));

    // destroy the active clients tree
    // avl_clear_tree (&cerver->clients->root, cerver->clients->destroy);
    avl_delete (cerver->clients);

    if (cerver->authRequired) {
        // send a packet to on hold clients
        broadcastToAllClients (cerver->onHoldClients->root, cerver, packet, packetSize);
        // destroy the tree
        // avl_clear_tree (&cerver->onHoldClients->root, cerver->onHoldClients->destroy);
        avl_delete (cerver->onHoldClients);

        // clear the on hold client's poll
        // this may change to a for if we have a dynamic poll structure
        memset (cerver->hold_fds, 0, sizeof (cerver->fds));
    }

    // the pool has only "empty" clients
    pool_clear (cerver->clientsPool);

    free (packet);

}

// teardown a cerver -> stop the cerver and clean all of its data
u8 cerver_teardown (Cerver *cerver) {

    if (!cerver) {
        cerver_log_msg (stdout, ERROR, SERVER, "Can't destroy a NULL cerver!");
        return 1;
    }

    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, SERVER, NO_TYPE, "Init cerver teardown...");
    #endif

    // TODO: what happens if we have a custom auth method?
    // clean cerver auth data
    if (cerver->authRequired) 
        if (cerver->auth.reqAuthPacket) free (cerver->auth.reqAuthPacket);

    // clean independent cerver type structs
    // if the cerver admin added a destroy cerver data action...
    if (cerver->destroyServerData) cerver->destroyServerData (cerver);
    else {
        // FIXME:
        // we use the default destroy cerver data function
        // switch (cerver->type) {
        //     case GAME_SERVER: 
        //         if (!destroyGameServer (cerver))
        //             cerver_log_msg (stdout, SUCCESS, SERVER, "Done clearing game cerver data!"); 
        //         break;
        //     case FILE_SERVER: break;
        //     case WEB_SERVER: break;
        //     default: break; 
        // }
    }

    // clean common cerver structs
    cerver_destroy_clients (cerver);
    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Done cleaning up clients.");
    #endif

    // disable socket I/O in both ways and stop any ongoing job
    if (!cerver_shutdown (cerver))
        cerver_log_msg (stdout, SUCCESS, SERVER, "Cerver has been shutted down.");

    else cerver_log_msg (stderr, ERROR, SERVER, "Failed to shutdown cerver!");
    
    if (cerver->thpool) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, 
            c_string_create ("Cerver active thpool threads: %i", 
            thpool_num_threads_working (cerver->thpool)));
        #endif

        // FIXME: 04/12/2018 - 15:02 -- getting a segfault
        thpool_destroy (cerver->thpool);

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, DEBUG_MSG, SERVER, "Destroyed cerver thpool!");
        #endif
    } 

    // destroy any other cerver data
    if (cerver->packetPool) pool_clear (cerver->packetPool);
    if (cerver->serverInfo) free (cerver->serverInfo);

    if (cerver->name) free (cerver->name);

    free (cerver);

    cerver_log_msg (stdout, SUCCESS, NO_TYPE, "Cerver teardown was successfull!");

    return 0;

}