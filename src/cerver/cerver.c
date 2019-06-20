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
#include "cerver/auth.h"
#include "cerver/handler.h"
#include "cerver/cerver.h"
#include "cerver/game/game.h"

#include "cerver/collections/dllist.h"
#include "cerver/collections/avl.h" 

#include "cerver/utils/thpool.h"
#include "cerver/utils/log.h"
#include "cerver/utils/config.h"
#include "cerver/utils/utils.h"

static DoubleList *cervers = NULL;

// cerver constructor, with option to init with some values
Cerver *cerver_new (Cerver *cerver) {

    Cerver *c = (Cerver *) malloc (sizeof (Cerver));
    if (c) {
        memset (c, 0, sizeof (Cerver));

        // init with some values
        if (cerver) {
            c->use_ipv6 = cerver->use_ipv6;
            c->protocol = cerver->protocol;
            c->port = cerver->port;
            c->connection_queue = cerver->connection_queue;
            c->poll_timeout = cerver->poll_timeout;
            c->auth_required = cerver->auth_required;
            c->type = cerver->type;
        }

        c->name = NULL;
        c->welcome_msg = NULL;

        c->delete_server_data = NULL;

        // by default the socket is assumed to be a blocking socket
        c->blocking = true;

        c->auth = NULL;
        c->auth_required = false;
        c->use_sessions = false;
        
        c->isRunning = false;
    }

    return c;

}

// deletes a cerver
void cerver_delete (void *ptr) {

    if (ptr) {
        Cerver *cerver = (Cerver *) ptr;

        str_delete (cerver->name);
        str_delete (cerver->welcome_msg);

        free (cerver);
    }

}

// sets the cerver poll timeout
void cerver_set_poll_time_out (Cerver *cerver, u32 poll_timeout) {

    if (cerver) cerver->poll_timeout = poll_timeout;

}

// sets the cerver msg to be sent when a client connects
// retuns 0 on success, 1 on error
u8 cerver_set_welcome_msg (Cerver *cerver, const char *msg) {

    if (cerver) {
        str_delete (cerver->welcome_msg);
        cerver->welcome_msg = msg ? str_new (msg) : NULL;
        return 0;
    }

    return 1;

}

// configures the cerver to require client authentication upon new client connections
// retuns 0 on success, 1 on error
u8 cerver_set_auth (Cerver *cerver, u8 max_auth_tries, delegate authenticate) {

    u8 retval = 1;

    if (cerver) {
        cerver->auth = auth_new ();
        if (cerver->auth) {
            cerver->auth->max_auth_tries = max_auth_tries;
            cerver->auth->authenticate = authenticate;
            cerver->auth->auth_packet = auth_packet_generate ();

            // FIXME:
            if (cerver->auth_required) {
                cerver->on_hold_clients = avl_init (client_comparator_clientID, destroyClient);

                memset (cerver->hold_fds, 0, sizeof (cerver->hold_fds));
                cerver->current_on_hold_nfds = 0;
                cerver->compress_hold_clients = false;

                cerver->n_hold_clients = 0;

                for (u16 i = 0; i < poll_n_fds; i++) cerver->hold_fds[i].fd = -1;
            }
        }
    }

    return retval;

}

// configures the cerver to use client sessions
// retuns 0 on success, 1 on error
u8 cerver_set_sessions (Cerver *cerver, Action session_id_generator) {

    u8 retval = 1;

    if (cerver) {
        if (session_id_generator) {
            cerver->session_id_generator = session_id_generator;
            cerver->use_sessions = true;
            // FIXME: regenerate avl with new client comparator
        }
    }

    return retval;

}

// FIXME: game data
static u8 cerver_init_data_structures (Cerver *cerver) {

    u8 retval = 1;

    if (cerver) {
        cerver->clients = avl_init (client_comparator_clientID, destroyClient);

        // initialize main pollfd structures
        cerver->fds = (struct pollfd *) calloc (poll_n_fds, sizeof (struct pollfd));
        if (cerver->fds) {
            memset (cerver->fds, 0, sizeof (cerver->fds));
            // set all fds as available spaces
            for (u32 i = 0; i < poll_n_fds; i++) cerver->fds[i].fd = -1;

            cerver->max_n_fds = poll_n_fds;
            cerver->current_n_fds = 0;
            cerver->compress_clients = false;
            cerver->poll_timeout = DEFAULT_POLL_TIMEOUT;

            // initialize cerver's own thread pool
            cerver->thpool = thpool_init (DEFAULT_TH_POOL_INIT);
            if (cerver->thpool) {
                switch (cerver->type) {
                    case FILE_SERVER: break;
                    case WEB_SERVER: break;
                    case GAME_SERVER: {
                        // FIXME: correctly init the game cerver!!
                        // GameServerData *gameData = (GameServerData *) malloc (sizeof (GameServerData));

                        // // init the lobbys with n inactive in the pool
                        // if (game_init_lobbys (gameData, GS_LOBBY_POOL_INIT)) {
                        //     cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to init cerver lobbys!");
                        //     return 1;
                        // }

                        // // init the players with n inactive in the pool
                        // if (game_init_players (gameData, GS_PLAYER_POOL_INT)) {
                        //     cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to init cerver players!");
                        //     return 1;
                        // }

                        // cerver->serverData = gameData;
                    } break;
                    default: break;
                }
            }

            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                    c_string_create ("Failed to init cerver %s thpool!", cerver->name));
                #endif
            }
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to allocate cerver main fds!");
            #endif
        }

        retval = 0;     // success!!
    }

    return retval;

}

// depending on the type of cerver, we need to init some const values
static u8 cerver_init_values (Cerver *cerver) {

    u8 retval = 1;

    if (cerver) {
        // cerver->handle_recieved_buffer = default_handle_recieved_buffer;
        // FIXME:
        cerver->cerver_info_packet = generateServerInfoPacket (cerver);

        switch (cerver->type) {
            case FILE_SERVER: break;
            case WEB_SERVER: break;
            case GAME_SERVER: {
                GameServerData *data = (GameServerData *) cerver->server_data;

                // FIXME:
                // get game modes info from a config file
                // data->gameSettingsConfig = config_parse_file (GS_GAME_SETTINGS_CFG);
                // if (!data->gameSettingsConfig) 
                //     cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "Problems loading game settings config!");

                // data->n_gameInits = 0;
                // data->gameInitFuncs = NULL;
                // data->loadGameData = NULL;
            } break;
            default: break;
        }

        retval = 0;     // LOG_SUCCESS!!
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
    cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, c_string_create ("Use IPv6: %i", cerver->useIpv6));
    #endif

    char *tcp = config_get_entity_value (cfgEntity, "tcp");
    if (tcp) {
        u8 usetcp = atoi (tcp);
        if (usetcp < 0 || usetcp > 1) {
            cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, "Unknown protocol. Using default: tcp protocol");
            usetcp = 1;
        }

        if (usetcp) cerver->protocol = PROTOCOL_TCP;
        else cerver->protocol = PROTOCOL_UDP;

        free (tcp);
    }

    // set to default (tcp) if we don't found a value
    else {
        cerver->protocol = PROTOCOL_TCP;
        cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, "No protocol found. Using default: TCP protocol");
    }

    char *port = config_get_entity_value (cfgEntity, "port");
    if (port) {
        cerver->port = atoi (port);
        // check that we have a valid range, if not, set to default port
        if (cerver->port <= 0 || cerver->port >= MAX_PORT_NUM) {
            cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, 
                c_string_create ("Invalid port number. Setting port to default value: %i", DEFAULT_PORT));
            cerver->port = DEFAULT_PORT;
        }

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, c_string_create ("Listening on port: %i", cerver->port));
        #endif
        free (port);
    }
    // set to default port
    else {
        cerver->port = DEFAULT_PORT;
        cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, 
            c_string_create ("No port found. Setting port to default value: %i", DEFAULT_PORT));
    } 

    char *queue = config_get_entity_value (cfgEntity, "queue");
    if (queue) {
        cerver->connection_queue = atoi (queue);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
            c_string_create ("Connection queue: %i", cerver->connection_queue));
        #endif
        free (queue);
    } 
    else {
        cerver->connection_queue = DEFAULT_CONNECTION_QUEUE;
        cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, 
            c_string_create ("Connection queue no specified. Setting it to default: %i", 
                DEFAULT_CONNECTION_QUEUE));
    }

    char *timeout = config_get_entity_value (cfgEntity, "timeout");
    if (timeout) {
        cerver->poll_timeout = atoi (timeout);
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
            c_string_create ("Cerver poll timeout: %i", cerver->poll_timeout));
        #endif
        free (timeout);
    }
    else {
        cerver->poll_timeout = DEFAULT_POLL_TIMEOUT;
        cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, 
            c_string_create ("Poll timeout no specified. Setting it to default: %i", 
                DEFAULT_POLL_TIMEOUT));
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

            default: cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Unkonw protocol type!"); return 1;
        }
        
        if (cerver->sock < 0) {
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to create cerver socket!");
            return 1;
        }

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Created cerver socket");
        #endif

        // set the socket to non blocking mode
        if (!sock_set_blocking (cerver->sock, cerver->blocking)) {
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to set cerver socket to non blocking mode!");
            close (cerver->sock);
            return 1;
        }

        else {
            cerver->blocking = false;
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Cerver socket set to non blocking mode.");
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
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to bind cerver socket!");
            return 1;
        }  

        return 0;       // LOG_SUCCESS!!
    }

    return 1; 

}

// inits a cerver of a given type
static u8 cerver_init (Cerver *cerver, Config *cfg, ServerType type) {

    int retval = 1;

    if (cerver) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Initializing cerver...");
        #endif

        if (cfg) {
            ConfigEntity *cfgEntity = config_get_entity_with_id (cfg, type);
            if (!cfgEntity) {
                cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Problems with cerver config!");
                return 1;
            } 

            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Using config entity to set cerver values...");
            #endif

            if (!cerver_get_cfg_values (cerver, cfgEntity)) 
                cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, "Done getting cfg cerver values");
        }

        // log cerver values
        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, c_string_create ("Use IPv6: %i", cerver->useIpv6));
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, c_string_create ("Listening on port: %i", cerver->port));
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, c_string_create ("Connection queue: %i", cerver->connectionQueue));
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, c_string_create ("Cerver poll timeout: %i", cerver->pollTimeout));
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, cerver->authRequired == 1 ? 
                "Cerver requires client authentication" : "Cerver does not requires client authentication");
            if (cerver->authRequired) 
                cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
                c_string_create ("Max auth tries set to: %i.", cerver->auth.maxAuthTries));
            cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, cerver->useSessions == 1 ? 
                "Cerver supports client sessions." : "Cerver does not support client sessions.");
            #endif
        }

        cerver->type = type;

        if (!cerver_network_init (cerver)) {
            if (!cerver_init_data_structures (cerver)) {
                if (!cerver_init_values (cerver)) {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
                        "Done initializing cerver data structures and values!");
                    #endif

                    retval = 0;     // LOG_SUCCESS!!
                }

                else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to init cerver values!");
            }

            else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to init cerver data structures!");
        }
    }

    return retval;

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
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to init the cerver!");
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
                cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to init the cerver!");
                config_destroy (serverConfig);
                cerver_delete (c); 
            }
        } 

        else cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Problems loading cerver config!\n");
    }

    return c;

}

// teardowns the cerver and creates a fresh new one with the same parameters
Cerver *cerver_restart (Cerver *cerver) {

    if (cerver) {
        cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, "Restarting the cerver...");

        Cerver temp = { 
            .use_ipv6 = cerver->use_ipv6, .protocol = cerver->protocol, .port = cerver->port,
            .connection_queue = cerver->connection_queue, .type = cerver->type,
            .poll_timeout = cerver->poll_timeout, .auth_required = cerver->auth_required,
            .use_sessions = cerver->use_sessions };

        temp.name = str_new (cerver->name->str);

        if (!cerver_teardown (cerver)) 
            cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, "Done with cerver teardown");
        else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to teardown the cerver!");

        // what ever the output, create a new cerver --> restart
        Cerver *retServer = cerver_new (&temp);
        if (!cerver_init (retServer, NULL, temp.type)) {
            cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, "Cerver has restarted!");
            return retServer;
        }

        else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Unable to retstart the cerver!");
    }

    else cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, "Can't restart a NULL cerver!");
    
    return NULL;

}

// tell the cerver to start listening for connections and packets
u8 cerver_start (Cerver *cerver) {

    if (cerver->isRunning) {
        cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, "The cerver is already running!");
        return 1;
    }

    u8 retval = 1;

    // one time only inits
    // if we have a game cerver, we might wanna load game data -> set by cerver admin
    if (cerver->type == GAME_SERVER) {
        GameServerData *game_data = (GameServerData *) cerver->server_data;
        if (game_data && game_data->load_game_data) {
            game_data->load_game_data (NULL);
        }

        else {
            cerver_log_msg (stdout, LOG_WARNING, LOG_GAME, 
                "Game cerver doesn't have a reference to a game data!");
        } 
    }

    switch (cerver->protocol) {
        case PROTOCOL_TCP: {
            if (!cerver->blocking) {
                if (!listen (cerver->sock, cerver->connection_queue)) {
                    // set up the initial listening socket     
                    cerver->fds[cerver->nfds].fd = cerver->sock;
                    cerver->fds[cerver->nfds].events = POLLIN;
                    cerver->nfds++;

                    cerver->isRunning = true;

                    // cerver is not holding clients if there is not new connections
                    if (cerver->auth_required) cerver->holding_clients = false;

                    cerver_poll (cerver);

                    retval = 0;
                }

                else {
                    cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, 
                        "Failed to listen in cerver socket!");
                    close (cerver->sock);
                }
            }

            else {
                cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, 
                    "Cerver socket is not set to non blocking!");
            }
            
        } break;

        case PROTOCOL_UDP: /* TODO: */ break;

        default: 
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Cant't start cerver! Unknown protocol!");
            break;
    }

    return retval;

}

// disable socket I/O in both ways and stop any ongoing job
u8 cerver_shutdown (Cerver *cerver) {

    if (cerver->isRunning) {
        cerver->isRunning = false; 

        // close the cerver socket
        if (!close (cerver->sock)) {
            #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "The cerver socket has been closed.");
            #endif

            return 0;
        }

        else cerver_log_msg (stdout, LOG_ERROR, LOG_CERVER, "Failed to close cerver socket!");
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

    if (cerver->auth_required) {
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
    // pool_clear (cerver->clientsPool);

    free (packet);

}

// teardown a cerver -> stop the cerver and clean all of its data
u8 cerver_teardown (Cerver *cerver) {

    if (!cerver) {
        cerver_log_msg (stdout, LOG_ERROR, LOG_CERVER, "Can't destroy a NULL cerver!");
        return 1;
    }

    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_CERVER, LOG_NO_TYPE, "Init cerver teardown...");
    #endif

    // TODO: what happens if we have a custom auth method?
    // clean cerver auth data
    // FIXME:
    // if (cerver->auth_required) 
    //     if (cerver->auth.reqAuthPacket) free (cerver->auth.reqAuthPacket);

    // clean independent cerver type structs
    // if the cerver admin added a destroy cerver data action...
    if (cerver->delete_server_data) cerver->delete_server_data (cerver);
    else {
        // FIXME:
        // we use the default destroy cerver data function
        // switch (cerver->type) {
        //     case GAME_SERVER: 
        //         if (!destroyGameServer (cerver))
        //             cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, "Done clearing game cerver data!"); 
        //         break;
        //     case FILE_SERVER: break;
        //     case WEB_SERVER: break;
        //     default: break; 
        // }
    }

    // clean common cerver structs
    cerver_destroy_clients (cerver);
    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Done cleaning up clients.");
    #endif

    // disable socket I/O in both ways and stop any ongoing job
    if (!cerver_shutdown (cerver))
        cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, "Cerver has been shutted down.");

    else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Failed to shutdown cerver!");
    
    if (cerver->thpool) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, 
            c_string_create ("Cerver active thpool threads: %i", 
            thpool_num_threads_working (cerver->thpool)));
        #endif

        // FIXME: 04/12/2018 - 15:02 -- getting a segfault
        thpool_destroy (cerver->thpool);

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Destroyed cerver thpool!");
        #endif
    } 

    // destroy any other cerver data
    if (cerver->packetPool) pool_clear (cerver->packetPool);
    if (cerver->serverInfo) free (cerver->serverInfo);

    cerver_delete (cerver);

    cerver_log_msg (stdout, LOG_SUCCESS, LOG_NO_TYPE, "Cerver teardown was successfull!");

    return 0;

}