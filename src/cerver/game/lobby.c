#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <time.h>

#include <poll.h>
#include <errno.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/cerver.h"
#include "cerver/client.h"
#include "cerver/handler.h"

#include "cerver/game/game.h"
#include "cerver/game/player.h"
#include "cerver/game/lobby.h"

#include "cerver/collections/dllist.h"
#include "cerver/collections/htab.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"
#include "cerver/utils/sha-256.h"

void *lobby_default_id_generator (const void *data_ptr) {

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

    return str_new (hash_string);

}

static CerverLobby *cerver_lobby_new (Cerver *cerver, Lobby *lobby) {

    CerverLobby *cerver_lobby = (CerverLobby *) malloc (sizeof (CerverLobby));
    if (cerver_lobby) {
        cerver_lobby->cerver = cerver;
        cerver_lobby->lobby = lobby;
    }

    return cerver_lobby;

}

static void cerver_lobby_delete (CerverLobby *cerver_lobby) {

    if (cerver_lobby) {
        cerver_lobby->cerver = NULL;
        cerver_lobby->lobby = NULL;
        free (cerver_lobby);
    }

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

        lobby->running = lobby->in_game = false;

        lobby->owner = NULL;

        lobby->packet_handler = NULL;

        lobby->game_type = NULL;

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

        dlist_destroy (lobby->players);
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

// initializes a new lobby
u8 lobby_init (GameCerver *game_cerver, Lobby *lobby) {

    u8 retval = 1;

    if (lobby) {
        lobby->creation_time_stamp = time (NULL);
        void *id = game_cerver->lobby_id_generator (lobby);
        if (id) {
            lobby->id = (String *) id;
            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, 
                c_string_create ("Lobby id: %s", lobby->id->str));
            #endif

            lobby->players = dlist_init (player_delete, player_comparator_by_id);

            retval = 0;
        }
    }

    return retval;

}

//compares two lobbys based on their ids
int lobby_comparator (const void *one, const void *two) {

    if (one && two) return str_compare (((Lobby *) one)->id, ((Lobby *) two)->id);
    else if (!one && two) return -1;
    else if (one && ! two) return 1;
    return 0;

}

// inits the lobby poll structures
// returns 0 on success, 1 on error
u8 lobby_poll_init (Lobby *lobby, unsigned int max_players_fds) {

    u8 retval = 1;

    if (lobby) {
        lobby->players_fds = (struct pollfd *) calloc (max_players_fds, sizeof (struct pollfd));
        if (lobby->players_fds) {
            lobby->max_players_fds = max_players_fds;
            lobby->current_players_fds = 0;

            for (u32 i = 0; i < lobby->max_players_fds; i++) {
                lobby->players_fds[i].fd = -1;
                lobby->players_fds[i].events = -1;
            }
        }
    }

    return retval;

}

// set lobby poll function timeout in mili secs
// how often we are checking for new packages
void lobby_set_poll_time_out (Lobby *lobby, unsigned int timeout) { 
    
    if (lobby) lobby->poll_timeout = timeout; 
    
}

// set the lobby hanlder
void lobby_set_handler (Lobby *lobby, Action handler) { if (lobby) lobby->handler = handler; }

// set the lobby packet handler
void lobby_set_packet_handler (Lobby *lobby, Action packet_handler) {

    if (lobby) lobby->packet_handler = packet_handler;

}

// sets the lobby settings and a function to delete it
void lobby_set_game_settings (Lobby *lobby, void *game_settings, Action game_settings_delete) {

    if (lobby) {
        lobby->game_settings = game_settings;
        lobby->game_settings_delete = game_settings_delete;
    }

}

// sets the lobby game data and a function to delete it
void lobby_set_game_data (Lobby *lobby, void *game_data, Action game_data_delete) {

    if (lobby) {
        lobby->game_data = game_data;
        lobby->game_data_delete = game_data_delete;
    }

}

// sets the lobby update action, the lobby will we passed as the args
void lobby_set_update (Lobby *lobby, Action update) { if (lobby) lobby->update = update; }

// reallocs lobby poll fds
// returns 0 on success, 1 on error
static u8 lobby_realloc_poll_fds (Lobby *lobby) {

    u8 retval = 1;

    if (lobby) {
        lobby->max_players_fds = lobby->max_players_fds * 2;
        lobby->players_fds = (struct pollfd *) realloc (lobby->players_fds,
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

// searches a lobby in the game cerver and returns a reference to it
Lobby *lobby_get (GameCerver *game_cerver, Lobby *query) {

    Lobby *found = NULL;
    for (ListElement *le = dlist_start (game_cerver->current_lobbys); le; le = le->next) {
        found = (Lobby *) le->data;
        if (!lobby_comparator (found, query)) return found;
    }

    return NULL;

}

// seraches for a lobby with matching id
Lobby *lobby_search_by_id (Cerver *cerver, const char *id) {

    if (cerver && id) {
        GameCerver *game_cerver = (GameCerver *) cerver->cerver_data;

        Lobby *lobby = NULL;
        for (ListElement *le =  dlist_start (game_cerver->current_lobbys); le; le = le->next) {
            lobby = (Lobby *) le->data;
            if (!strcmp (lobby->id->str, id)) return lobby;
        }
    }

    return NULL;

}

// lobby default handler
void lobby_poll (void *ptr) {

    if (ptr) {
        CerverLobby *cerver_lobby = (CerverLobby *) ptr;
        Cerver *cerver = cerver_lobby->cerver;
        Lobby *lobby = cerver_lobby->lobby;

        cerver_log_msg (stdout, LOG_SUCCESS, LOG_GAME, 
            c_string_create ("Lobby %s handler has started!", lobby->id->str));

        int poll_retval = 0;
        while (lobby->running) {
            poll_retval = poll (lobby->players_fds, lobby->max_players_fds, lobby->poll_timeout);

            // poll failed
            if (poll_retval < 0) {
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
                    c_string_create ("Lobby %s poll has failed!", lobby->id->str));
                perror ("Error");
                break;
            }

            // if poll has timed out, just continue to the next loop... 
            if (poll_retval == 0) {
                // #ifdef CERVER_DEBUG
                // cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, 
                //    c_string_create ("Lobby %s poll timeout.", lobby->id->str));
                // #endif
                continue;
            }

            // one or more fd(s) are readable, need to determine which ones they are
            for (u16 i = 0; i < lobby->current_players_fds; i++) {
                if (lobby->players_fds[i].revents == 0) continue;
                if (lobby->players_fds[i].revents != POLLIN) continue;

                // if (thpool_add_work (cerver->thpool, lobby->handler, 
                //     cerver_receive_new (cerver, lobby->players_fds[i].fd, false))) {
                //     cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                //         c_string_create ("Failed to add cerver_receive () to cerver's %s thpool!", 
                //         cerver->info->name->str));
                // }
            }
        }

        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME,
            c_string_create ("Lobby %s poll has stopped!", lobby->id->str));
        #endif
    }

    else {
        cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
            "Can't handle players on a NULL client");
    } 

}

/*** Public lobby functions ***/

// creates and inits a new lobby
// creates a new user associated with the client and makes him the owner
Lobby *lobby_create (Cerver *cerver, Client *client) {

    Lobby *lobby = NULL;

    if (cerver && client) {
        GameCerver *game_cerver = (GameCerver *) cerver->cerver_data;
        if (game_cerver) {
            lobby = lobby_new ();
            if (!lobby_init (game_cerver, lobby)) {
                // make the client that make the request, the lobby owner
                Player *owner = player_new ();
                if (owner) {
                    owner->client = client;

                    // register the owner to the lobby
                    if (player_register_to_lobby (lobby, owner)) {
                        cerver_log_msg (stderr, LOG_ERROR, LOG_PLAYER, 
                            c_string_create ("Failed to register the owner to the lobby %s.",
                            lobby->id->str));

                        lobby_delete (lobby);
                        lobby = NULL;
                    }
                }

                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_PLAYER, 
                        c_string_create ("Failed to create the lobby owner for lobby %s.",
                        lobby->id->str));
                    #endif

                    lobby_delete (lobby);
                    lobby = NULL;
                }
            }

            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "Failed to init the new lobby!");
                #endif

                lobby_delete (lobby);
                lobby = NULL;
            }
        }
    }

    return lobby;

}

// FIXME: check if the lobby is already full
// a client wants to join a game, so we create a new player and register him to the lobby
// returns 0 on success, 1 on error
u8 lobby_join (Cerver *cerver, Client *client, Lobby *lobby) {

    u8 retval = 1;

    if (cerver && client && lobby) {
        GameCerver *game_cerver = (GameCerver *) cerver->cerver_data;
        if (game_cerver) {
            // check for lobby max players cap
            // if (lobby->current_players_fds < lobby->max_players) {
                // make the client that make the request, a lobby member
                Player *member = player_new ();
                if (member) {
                    member->client = client;

                    // register the member to the lobby
                    if (!player_register_to_lobby (lobby, member)) retval = 0;      // success
                    else {
                        cerver_log_msg (stderr, LOG_ERROR, LOG_PLAYER, 
                            c_string_create ("Failed to register a new member to lobby %s",
                            lobby->id->str));
                    }

                    return 0;
                }

                else {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_PLAYER,
                        c_string_create ("Failed to create a new lobby owner fro lobby %s",
                        lobby->id->str));
                    #endif
                }
            // }

            // else {
            //     #ifdef CERVER_DEBUG
            //     cerver_log_msg (stderr, LOG_WARNING, LOG_PLAYER, 
            //         c_string_create ("Lobby %s is already full!",
            //         lobby->id->str));
            //     #endif
            // }
            
        }
    }

    return retval;

}

// called when a player requests to leave the lobby
// returns 0 on success, 1 on error
u8 lobby_leave (Cerver *cerver, Lobby *lobby, Player *player) {

    u8 retval = 1;

    if (cerver && lobby && player) {
        // check that the player is in the lobby
        Player *found = player_get_from_lobby (lobby, player);
        if (found) {
            // remove the player from the lobby
            player_unregister_from_lobby (lobby, player);

            // check if there are players left inside the lobby
            if (lobby->n_current_players <= 0) {
                // destroy the lobby
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME,
                    c_string_create ("Destroying lobby %s -- it is empty.",
                    lobby->id->str));
                #endif
                lobby_delete (lobby);
            }

            // check if the player was the owner
            else if (!str_compare (lobby->owner->id, player->id)) {
                // get a new owner
                Player *new_owner = (Player *) (dlist_start (lobby->players))->data;
                if (new_owner) {
                    lobby->owner = new_owner;
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_DEBUG, LOG_PLAYER,
                        c_string_create ("Player %s is the new owner of lobby %s.",
                        new_owner->id->str, lobby->id->str));
                    #endif
                }
            }

            player_delete (player);

            retval = 0;
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_PLAYER,
                c_string_create ("Player %s does not seem to be in lobby %s.",
                player->id->str, lobby->id->str));
            #endif
        }
    }

    return retval;

}

// starts the lobby's handler and/or update method in the cervers thpool
u8 lobby_start (Cerver *cerver, Lobby *lobby) {

    u8 retval = 1;

    if (cerver && lobby) {
        bool started = false;
        CerverLobby *cerver_lobby = cerver_lobby_new (cerver, lobby);
        lobby->running = true;

        // check if the lobby has a handler method
        if (lobby->handler) {
            if (!thpool_add_work (cerver->thpool, lobby->handler, cerver_lobby)) {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME,
                    c_string_create ("Started lobby %s handler",
                    lobby->id->str));
                #endif
                started = true;
            }

            else {
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME,
                    c_string_create ("Failed to add lobby %s handler to cerver's thpool!",
                    lobby->id->str, cerver->info->name->str));
            }
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_GAME, 
                c_string_create ("lobby_start () -- lobby %s does not have a handler.",
                lobby->id->str));
            #endif
        }

        // check if the lobby has an update method
        if (lobby->update) {
            if (!thpool_add_work (cerver->thpool, lobby->update, cerver_lobby)) {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME,
                    c_string_create ("Started lobby %s update",
                    lobby->id->str));
                #endif
                started = true;
            }

            else {
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME,
                    c_string_create ("Failed to add lobby %s update to cerver's thpool!",
                    lobby->id->str, cerver->info->name->str));
            }
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_GAME,
                c_string_create ("lobby_start () -- lobby %s does not have an update.",
                lobby->id->str));
            #endif
        }

        if (started) retval = 0;
        else {
            cerver_lobby_delete (cerver_lobby);
            lobby->running = false;
        } 
    }

    return retval;

}

/*** seralization ***/

#pragma region serialization

static SLobby *slobby_new (void) {

    SLobby *slobby = (SLobby *) malloc (sizeof (SLobby));
    if (slobby) memset (slobby, 0, sizeof (SLobby));
    return slobby;

}

// FIXME: game settings
SLobby *lobby_serialize (Lobby *lobby) {

    SLobby *slobby = NULL;

    if (lobby) {
        slobby = slobby_new ();
        if (slobby) {
            strncpy (slobby->id.string, lobby->id->str, 64);
            slobby->id.len = (lobby->id->len >= 64) ? 64 : lobby->id->len;
            slobby->creation_timestamp = lobby->creation_time_stamp;
            slobby->in_game = lobby->in_game;
            slobby->running = lobby->running;
            slobby->n_players = lobby->n_current_players;
        }
    }

    return slobby;

}

// FIXME: send players inside the lobby
// sends a lobby and all of its data to the client
// created: if the lobby was just created
void lobby_send (Lobby *lobby, bool created,
    Cerver *cerver, Client *client, Connection *connection) {

    if (lobby) {
        // serialize and send back the lobby
        SLobby *slobby = lobby_serialize (lobby);
        if (slobby) {
            Packet *lobby_packet = packet_generate_request (GAME_PACKET, 
                created ? GAME_LOBBY_CREATE : GAME_LOBBY_JOIN, 
                slobby, sizeof (SLobby));
            if (lobby_packet) {
                packet_set_network_values (lobby_packet, cerver, client, connection);
                packet_send (lobby_packet, 0, NULL);
                packet_delete (lobby_packet);
            }

            free (slobby);
        }
    }

}

#pragma endregion