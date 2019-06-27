#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <time.h>

#include <poll.h>
#include <errno.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/game/game.h"
#include "cerver/game/player.h"
#include "cerver/game/lobby.h"

#include "cerver/collections/dllist.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"
#include "cerver/utils/sha-256.h"

void lobby_default_id_generator (char *lobby_id) {

    time_t rawtime = time (NULL);
    struct tm *timeinfo = localtime (&rawtime);
    // we generate the string timestamp in order to generate the lobby id
    char temp[50] = { 0 };
    size_t len = strftime (temp, 50, "%F-%r", timeinfo);
    printf ("%s\n", temp);

    uint8_t hash[32];
    char hash_string[65];

    sha_256_calc (hash, temp, len);
    sha_256_hash_to_string (hash_string, hash);

    lobby_id = c_string_create ("%s", hash_string);

}

// we remove any fd that was set to -1 for what ever reason
static void lobby_players_compress (Lobby *lobby) {

    if (lobby) {
        lobby->compress_players = false;
        
        for (u16 i = 0; i < lobby->players_nfds; i++) {
            if (lobby->players_fds[i].fd == -1) {
                for (u16 j = i; j < lobby->players_nfds; j++)
                    lobby->players_fds[i].fd = lobby->players_fds[j + 1].fd;

                i--;
                lobby->players_nfds--;
            }
        }
    }

}

// takes as data a server lobby structure
// recieves packages from players inside the lobby
// packages are assumed to use cerver structure to transfer data
// if you need to manage packages in a custom way or do not user cerver types at all, 
// you can set your own handler
static void lobby_default_handler (void *ptr) {

    // check we have the correct data
    if (!ptr) {
        cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
            "Can start lobby handler, neither server nor lobby can be null!");
        return;
    } 

    ServerLobby *sl = (ServerLobby *) ptr;
    Cerver *server = sl->server;
    Lobby *lobby = sl->lobby;

    if (!server || !lobby) {
        cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
            "Can start lobby handler, neither server nor lobby can be null!");
        return;
    } 

    // we are good to go...
    cerver_log_msg (stdout, LOG_SUCCESS, LOG_CERVER, "Lobby has started!");

    int poll_retval;
    while (lobby->running) {
        poll_retval = poll (lobby->players_fds, lobby->players_nfds, lobby->poll_timeout);

        // poll failed
        if (poll_retval < 0) {
            cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, 
                c_string_create ("Lobby %s poll failed!", lobby->id->str));
            #ifdef CERVER_DEBUG
            perror ("Error");
            #endif
            server->isRunning = false;
            break;
        }

        // if poll has timed out, just continue to the next loop... 
        if (poll_retval == 0) {
            // #ifdef CERVER_DEBUG
            // cerver_log_msg (stdout, LOG_DEBUG, LOG_CERVER, "Poll timeout.");
            // #endif
            continue;
        }

        // FIXME:
        // one or more fd(s) are readable, need to determine which ones they are
        /* for (u8 i = 0; i < poll_n_fds; i++) {
            if (server->fds[i].revents == 0) continue;
            if (server->fds[i].revents != POLLIN) continue;

            // accept incoming connections that are queued
            if (server->fds[i].fd == server->serverSock) {
                if (server_accept (server)) {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_SUCCESS, LOG_CLIENT, "LOG_SUCCESS accepting a new client!");
                    #endif
                }
                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_CLIENT, "Failed to accept a new client!");
                    #endif
                } 
            }

            // TODO: maybe later add this directly to the thread pool
            // not the server socket, so a connection fd must be readable
            else server_recieve (server, server->fds[i].fd, false);
        } */

        // if (server->compress_clients) compressClients (server);
    }

    #ifdef CERVER_DEBUG
    cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, 
        c_string_create ("Lobby %s handler has stopped!", lobby->id->str));
    #endif

}


/*** Lobby ***/

// lobby constructor
Lobby *lobby_new (void) {

    Lobby *lobby = (Lobby *) malloc (sizeof (Lobby));
    if (lobby) {
        memset (lobby, 0, sizeof (Lobby));

        lobby->id = NULL;

        lobby->sock_fd_player_map = NULL;
        lobby->players_fds = NULL;
        lobby->compress_players = false;

        lobby->running = lobby->in_game = false;

        lobby->owner = NULL;

        lobby->packet_handler = NULL;

        lobby->game_settings = NULL;
        lobby->game_settings_delete = NULL;

        lobby->game_data = NULL;
        lobby->game_data_delete = NULL;

        lobby->update = NULL;
    }

    return lobby;

}

void lobby_delete (void *lobby_ptr) {

    if (lobby_ptr) {
        Lobby *lobby = (Lobby *) lobby_ptr;

        str_delete (lobby->id);

        htab_destroy (lobby->sock_fd_player_map);

        if (lobby->players_fds) free (lobby->players_fds);

        lobby->owner = NULL;

        if (lobby->game_settings) {
            if (lobby->game_settings_delete) 
                lobby->game_settings_delete (lobby->game_settings);
            else free (lobby->game_settings);
        }

        if (lobby->game_data) {
            if (lobby->game_data_delete) 
                lobby->game_data_delete (lobby->game_data);
            else free (lobby->game_data);
        }

        free (lobby);
    }

}

// reallocs lobby poll fds
// returns 0 on success, 1 on error
static u8 lobby_realloc_poll_fds (Lobby *lobby) {

    u8 retval = 1;

    if (lobby) {
        lobby->max_players_fds = lobby->max_players_fds * 2;
        lobby->players_fds = realloc (lobby->players_fds,
            lobby->max_players_fds * sizeof (struct pollfd));
        if (lobby->players_fds) retval = 0;
    }

    return retval;

}

// gets a free idx in the lobby poll structure
static i32 lobby_poll_get_free_idx (const Lobby *lobby) {

    if (lobby) {
        for (u32 i = 0; i < lobby->max_players_fds; i++) 
            if (lobby->players_fds[i].fd == -1) return i;
    }

    return -1;

}

// gets the idx of the connection fd in the poll fds
static i32 lobby_poll_get_idx_by_sock_fd (const Lobby *lobby, const i32 sock_fd) {

    if (lobby) {
        for (u32 i = 0; i < lobby->max_players_fds; i++)
            if (lobby->players_fds[i].fd == sock_fd) return i;
    }

    return -1;

}

// registers a player's client connection to the lobby poll
// and maps the sock fd to the player
u8 lobby_poll_register_connection (Lobby *lobby, Player *player, Connection *connection) {

    u8 retval = 1;

    if (lobby && player && connection) {
        i32 idx = lobby_poll_get_free_idx (lobby);
        if (idx > 0) {
            lobby->players_fds[idx].fd = connection->sock_fd;
            lobby->players_fds[idx].events = POLLIN;
            lobby->current_players_fds++;

            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_DEBUG, LOG_NO_TYPE, 
                c_string_create ("Added a new sock fd to lobby %s poll - idx %i",
                lobby->id->str, idx));
            #endif

            // map the socket fd with the player
            const void *key = &connection->sock_fd;
            htab_insert (lobby->sock_fd_player_map, key, sizeof (i32), player, sizeof (Player));

            retval = 0;
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE,
                c_string_create ("Lobby %s poll is full -- we need to realloc...",
                lobby->id->str));
            #endif

            if (lobby_realloc_poll_fds (lobby)) {
                cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE,
                    c_string_create ("Failed to realloc lobby %s poll structure!",
                    lobby->id->str));
            }
        }
    }

    return retval;

}

// unregisters a player's client connection from the lobby poll structure
// and removes the map from the sock fd to the player
// returns 0 on success, 1 on error
u8 lobby_poll_unregister_connection (Lobby *lobby, Player *player, Connection *connection) {

    u8 retval = 1;

    if (lobby && player && connection) {
        // get the idx of the connection sock in the lobby poll fds
        i32 idx = lobby_poll_get_idx_by_sock_fd (lobby, connection->sock_fd);
        if (idx >= 0) {
            lobby->players_fds[idx].fd = -1;
            lobby->players_fds[idx].events = -1;
            lobby->current_players_fds--;

            const void *key = &connection->sock_fd;
            retval = htab_remove (lobby->sock_fd_player_map, key, sizeof (i32));
            #ifdef CERVER_DEBUG
            if (retval) {
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME,
                    c_string_create ("Failed to remove from lobby's %s sock fd player map.",
                    lobby->id->str));
            }
            #endif
        }
    }

    return retval;

}

// initializes a new lobby
// pass the server game data
// pass 0 to max players to use the default 4
// pass NULL in handle to use default
Lobby *lobby_init (GameCerver *game_cerver, unsigned max_players, Action handler) {

    Lobby *lobby = lobby_new ();
    if (lobby) {
        lobby->creation_time_stamp = time (NULL);

        char *id = NULL;
        game_cerver->lobby_id_generator ((char *) id);
        lobby->id = str_new (id);

        // we dont want to really destroy the player data, just removed it from the tree
        lobby->players = avl_init (game_cerver->player_comparator, player_delete_dummy);
        lobby->max_players = max_players <= 0 ? DEFAULT_MAX_LOBBY_PLAYERS : max_players;
        lobby->players_fds = (struct pollfd *) calloc (lobby->max_players, sizeof (struct pollfd));
        lobby->poll_timeout = LOBBY_DEFAULT_POLL_TIMEOUT;

        lobby->handler = handler ? handler : lobby_default_handler;
    }

    return lobby;

}

int lobby_comparator (const void *one, const void *two) {

    if (one && two) {
        Lobby *lobby_one = (Lobby *) one;
        Lobby *lobby_two = (Lobby *) two;

        return str_compare (lobby_one->id, lobby_two->id);
    }

}

/*** Lobby Configuration ***/

// sets the lobby settings and a function to delete it
void lobby_set_game_settings (Lobby *lobby, void *game_settings, Action delete_game_settings) {

    if (lobby) {
        lobby->game_settings = game_settings;
        lobby->delete_lobby_game_settings = delete_game_settings;
    }

}

// sets the lobby game data and a function to delete it
void lobby_set_game_data (Lobby *lobby, void *game_cerver, Action delete_lobby_game_data) {

    if (lobby) {
        lobby->game_cerver = game_cerver;
        lobby->delete_lobby_game_data = delete_lobby_game_data;
    }

}

// set the lobby player handler
void lobby_set_handler (Lobby *lobby, Action handler) { if (lobby) lobby->handler = handler; }

// set lobby poll function timeout in mili secs
// how often we are checking for new packages
void lobby_set_poll_time_out (Lobby *lobby, unsigned int timeout) { if (lobby) lobby->poll_timeout = timeout; }

// searchs a lobby in the game data and returns a reference to it
Lobby *lobby_get (GameCerver *game_cerver, Lobby *query) {

    Lobby *found = NULL;
    for (ListElement *le = dlist_start (game_cerver->current_lobbys); le; le = le->next) {
        found = (Lobby *) le->data;
        if (!lobby_comparator (found, query)) break;
    }

    return found;

}

/*** Handle lobby players ***/

// FIXME:
// add a player to a lobby
static u8 lobby_add_player (Lobby *lobby, Player *player) {

    if (lobby && player) {
        // if (!player_is_in_lobby (player, lobby)) {

        // }
    }

}

// FIXME:
// removes a player from the lobby
static u8 lobby_remove_player (Lobby *lobby, Player *player) {

    u8 retval = 1;

    if (lobby && player) {

    }

    return retval;

}

// FIXME:
// add a player to the lobby structures
u8 player_add_to_lobby (Cerver *server, Lobby *lobby, Player *player) {

    /* if (lobby && player) {
        if (server->type == GAME_SERVER) {
            GameCerver *gameData = (GameCerver *) server->serverData;
            if (gameData) {
                if (!player_is_in_lobby (player, lobby)) {
                    Player *p = avl_remove_node (gameData->players, player);
                    if (p) {
                        i32 client_sock_fd = player->client->active_connections[0];

                        avl_insert_node (lobby->players, p);

                        // for (u32 i = 0; i < poll_n_fds; i++) {
                        //     if (server->fds[i].fd == client_sock_fd) {
                        //         server->fds[i].fd = -1;
                        //         server->fds[i].events = -1;
                        //     }
                        // } 

                        lobby->players_fds[lobby->players_nfds].fd = client_sock_fd;
                        lobby->players_fds[lobby->players_nfds].events = POLLIN;
                        lobby->players_nfds++;

                        player->inLobby = true;

                        return 0;
                    }

                    else cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "Failed to get player from avl!");
                }
            }
        }
    } */

    return 1;    

}

// FIXME: client socket!!
// removes a player from the lobby's players structures and sends it to the game server's players
u8 player_remove_from_lobby (Cerver *server, Lobby *lobby, Player *player) {

    // if (server && lobby && player) {
    //     if (server->type == GAME_SERVER) {
    //         GameCerver *gameData = (GameCerver *) gameData;
    //         if (gameData) {
    //             // make sure that the player is inside the lobby...
    //             if (player_is_in_lobby (player, lobby)) {
    //                 // create a new player and add it to the server's players
    //                 // Player *p = newPlayer (gameData->playersPool, NULL, player);

    //                 // remove from the poll fds
    //                 for (u8 i = 0; i < lobby->players_nfds; i++) {
    //                     /* if (lobby->players_fds[i].fd == player->client->clientSock) {
    //                         lobby->players_fds[i].fd = -1;
    //                         lobby->players_nfds--;
    //                         lobby->compress_players = true;
    //                         break;
    //                     } */
    //                 }

    //                 // delete the player from the lobby
    //                 avl_remove_node (lobby->players, player);

    //                 // p->inLobby = false;

    //                 // player_registerToServer (server, p);

    //                 return 0;
    //             }
    //         }
    //     }
    // }

    return 1;

}

/*** Public lobby functions ***/

// starts the lobby in a separte thread using its handler
u8 lobby_start (Cerver *server, Lobby *lobby) {

    u8 retval = 1;

    if (server && lobby) {
        if (lobby->handler) {
            lobby->running = true;      // make the lobby active
            ServerLobby *sl = (ServerLobby *) malloc (sizeof (ServerLobby));
            sl->server = server;
            sl->lobby = lobby;
            if (thpool_add_work (server->thpool, (void *) lobby->handler, sl) < 0)
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "Failed to start lobby - failed to add to thpool!");
            else retval = 0;        // LOG_SUCCESS
        } 

        else cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "Failed to start lobby - no reference to lobby handler.");
    }

    return retval;

}

// creates a new lobby and inits his values with an owner
// pass a custom handler or NULL to use teh default one
Lobby *lobby_create (Cerver *server, Player *owner, unsigned int max_players, Action handler) {

    Lobby *lobby = NULL;

    if (server && owner) {
        GameCerver *game_cerver = (GameCerver *) server->cerver_data;
        if (game_cerver) {
            // we create a timestamp of the creation of the lobby
            lobby = lobby_init (game_cerver, max_players, handler);
            if (lobby) {
                if (!lobby_add_player (lobby, owner)) {
                    lobby->owner = owner;
                    lobby->current_players = 1;

                    // add the lobby the server active ones
                    dlist_insert_after (game_cerver->current_lobbys, 
                        dlist_end (game_cerver->current_lobbys), lobby);
                }

                else {
                    cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "Failed to add owner to lobby!");
                    lobby_delete (lobby);
                    lobby = NULL;
                }
            }

            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "Failed to init new lobby!");
                #endif
            } 
        }  

        else {
            cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
                c_string_create ("Cerver %s doesn't have a reference to game data!", 
                server->name->str));
        } 
    }

    return lobby;

}

// FIXME: return a custom error code to handle by the server and client
// called by a registered player that wants to join a lobby on progress
// the lobby model gets updated with new values
u8 lobby_join (GameCerver *game_cerver, Lobby *lobby, Player *player) {

    u8 retval = 1;

    if (lobby && player) {
        // check if for whatever reason a player al ready inside the lobby wants to join...
        if (!player_is_in_lobby (lobby, game_cerver->player_comparator, player)) {
            // check if the lobby is al ready running the game
            if (!lobby->in_game) {
                // check lobby capacity
                if (lobby->current_players < lobby->max_players) {
                    // the player is clear to join the lobby
                    if (!lobby_add_player (lobby, player)) {
                        lobby->current_players += 1;
                        retval = 0;
                    }
                }

                else cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, "A player tried to join a full lobby.");
            }

            else cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, "A player tried to join a lobby that is in game.");
        }

        else cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "A player tries to join the same lobby he is in.");
    }

    return retval;

}

// called when a player requests to leave the lobby
u8 lobby_leave (GameCerver *game_cerver, Lobby *lobby, Player *player) {

    u8 retval = 1;

    if (lobby && player) {
        // first check if the player is inside the lobby
        if (player_is_in_lobby (lobby, game_cerver->player_comparator, player)) {
            // FIXME:
            // now check if the player is the owner

            // remove the player from the lobby
            if (!lobby_remove_player (lobby, player)) lobby->current_players -= 1;

            // check if there are still players in the lobby
            if (lobby->current_players > 0) {
                // TODO: what do we do to the players??
            }

            else {  
                // the player was the last one in, so we can safely delete the lobby
                lobby_delete (lobby);
                retval = 0;
            } 
        }
    }

    return retval;

}

u8 lobby_destroy (Cerver *server, Lobby *lobby) {}

// FIXME: 19/april/2019 -- do we still need this?
// a lobby should only be destroyed when there are no players left or if we teardown the server
u8 destroyLobby (Cerver *server, Lobby *lobby) {

    /* if (server && lobby) {
        if (server->type == GAME_SERVER) {
            GameCerver *gameData = (GameCerver *) server->serverData;
            if (gameData) {
                // the game function must have a check for this every loop!
                if (lobby->inGame) lobby->inGame = false;
                if (lobby->gameData) {
                    if (lobby->deleteLobbyGameData) lobby->deleteLobbyGameData (lobby->gameData);
                    else free (lobby->gameData);
                }

                if (lobby->players_nfds > 0) {
                    // send the players the correct package so they can handle their own logic
                    // expected player behaivor -> leave the lobby 
                    size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);
                    void *destroyPacket = generatePacket (GAME_PACKET, packetSize);
                    if (destroyPacket) {
                        char *end = destroyPacket + sizeof (PacketHeader);
                        RequestData *reqdata = (RequestData *) end;
                        reqdata->type = LOBBY_DESTROY;

                        broadcastToAllPlayers (lobby->players->root, server, destroyPacket, packetSize);
                        free (destroyPacket);
                    }

                    else 
                        cerver_log_msg (stderr, LOG_ERROR, LOG_PACKET, "Failed to create lobby destroy packet!");

                    // this should stop the lobby poll thread
                    lobby->isRunning = false;

                    // remove the players from this structure and send them to the server's players
                    Player *tempPlayer = NULL;
                    while (lobby->players_nfds > 0) {
                        tempPlayer = (Player *) lobby->players->root->id;
                        if (tempPlayer) player_remove_from_lobby (server, lobby, tempPlayer);
                    }
                }

                lobby->owner = NULL;
                if (lobby->settings) free (lobby->settings);

                // we are safe to clear the lobby structure
                // first remove the lobby from the active ones, then send it to the inactive ones
                ListElement *le = dlist_get_element (gameData->current_lobbys, lobby);
                if (le) {
                    void *temp = dlist_remove_element (gameData->current_lobbys, le);
                    if (temp) pool_push (gameData->lobbyPool, temp);
                }

                else {
                    cerver_log_msg (stdout, LOG_WARNING, LOG_GAME, "A lobby wasn't found in the current lobby list.");
                    deleteLobby (lobby);   // destroy the lobby forever
                } 
                
                return 0;   // LOG_SUCCESS
            }

            else cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "No game data found in the server!");
        }
    }

    return 1; */

}

// FIXME: client socket!!
// FIXME: send packet!
// TODO: add a timestamp when the player leaves

// FIXME: finish refcatoring lobby_leaver with this!!
u8 leaveLobby (Cerver *server, Lobby *lobby, Player *player) {

    // make sure that the player is inside the lobby
    // if (player_is_in_lobby (player, lobby)) {
    //     // check to see if the player is the owner of the lobby
    //     bool wasOwner = false;
    //     // TODO: we should be checking for the player's id instead
    //     // if (lobby->owner->client->clientSock == player->client->clientSock) 
    //     //     wasOwner = true;

    //     if (player_remove_from_lobby (server, lobby, player)) return 1;

    //     // there are still players in the lobby
    //     if (lobby->players_nfds > 0) {
    //         if (lobby->inGame) {
    //             // broadcast the other players that the player left
    //             // we need to send an update lobby packet and the players handle the logic
    //             sendLobbyPacket (server, lobby);

    //             // if he was the owner -> set a new owner of the lobby -> first one in the array
    //             if (wasOwner) {
    //                 Player *temp = NULL;
    //                 u8 i = 0;
    //                 do {
    //                     temp = getPlayerBySock (lobby->players->root, lobby->players_fds[i].fd);
    //                     if (temp) {
    //                         lobby->owner = temp;
    //                         size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);
    //                         void *packet = generatePacket (GAME_PACKET, packetSize);
    //                         if (packet) {
    //                             // server->protocol == IPPROTO_TCP ?
    //                             // tcp_sendPacket (temp->client->clientSock, packet, packetSize, 0) :
    //                             // udp_sendPacket (server, packet, packetSize, temp->client->address);
    //                             free (packet);
    //                         }
    //                     }

    //                     // we got a NULL player in the structures -> we don't expect this to happen!
    //                     else {
    //                         cerver_log_msg (stdout, LOG_WARNING, LOG_GAME, 
    //                             "Got a NULL player when searching for new owner!");
    //                         lobby->players_fds[i].fd = -1;
    //                         lobby->compress_players = true; 
    //                         i++;
    //                     }
    //                 } while (!temp);
    //             }
    //         }

    //         // players are in the lobby screen -> owner destroyed the lobby
    //         else destroyLobby (server, lobby);
    //     }

    //     // player that left was the last one 
    //     // 21/11/2018 -- destroy lobby is in charge of correctly ending the game
    //     else destroyLobby (server, lobby);
        
    //     return 0;   // the player left the lobby successfully
    // }

    // else {
    //     #ifdef CERVER_DEBUG
    //         cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "The player doesn't belong to the lobby!");
    //     #endif
    // }

    // return 1;

}