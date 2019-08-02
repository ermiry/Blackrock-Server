#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/collections/dllist.h"

#include "cerver/cerver.h"
#include "cerver/client.h"
#include "cerver/packets.h"
#include "cerver/errors.h"

#include "cerver/game/game.h"
#include "cerver/game/gametype.h"
#include "cerver/game/player.h"
#include "cerver/game/lobby.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

static GameCerverStats *game_cerver_stats_new (void) {

    GameCerverStats *stats = (GameCerverStats *) malloc (sizeof (GameCerverStats));
    if (stats) memset (stats, 0, sizeof (GameCerverStats));
    return stats;

}

static void game_cerver_stats_delete (GameCerverStats *stats) {

    if (stats) free (stats);

}

void game_cerver_stats_print (Cerver *cerver) {

    if (cerver) {
        if (cerver->type == GAME_CERVER) {
            GameCerver *game_cerver = (GameCerver *) cerver->cerver_data;

            printf ("Current active lobbys:         %d\n", game_cerver->stats->current_active_lobbys);
            printf ("Total lobbys created:          %d\n", game_cerver->stats->lobbys_created);
        }

        else {
            cerver_log_msg (stderr, LOG_WARNING, LOG_CERVER, 
                c_string_create ("Can't print game stats of cerver %s -- it is not a game cerver.",
                cerver->info->name->str));
        }
    }

}

GameCerver *game_new (void) {

    GameCerver *game = (GameCerver *) malloc (sizeof (GameCerver));
    if (game) {
        game->current_lobbys = dlist_init (lobby_delete, lobby_comparator);

        game->lobby_id_generator = lobby_default_id_generator;
        game->player_comparator = NULL;

        game->game_types = dlist_init (game_type_delete, NULL);

        game->load_game_data = NULL;
        game->delete_game_data = NULL;
        game->game_data = NULL;

        game->final_game_action = NULL;
        game->final_action_args = NULL;

        game->stats = game_cerver_stats_new ();
    }

    return game;

}

void game_delete (void *game_ptr) {

    if (game_ptr) {
        GameCerver *game = (GameCerver *) game_ptr;

        dlist_destroy (game->current_lobbys);

        dlist_destroy (game->game_types);

        if (game->game_data) {
            if (game->delete_game_data)
                game->delete_game_data (game->game_data);
            else free (game->game_data);
        }

        game_cerver_stats_delete (game->stats);

        free (game);
    }

}

/*** Game Cerver configuration ***/

// option to set the game cerver lobby id generator
void game_set_lobby_id_generator (GameCerver *game_cerver, void *(*lobby_id_generator) (const void *)) {

    if (game_cerver) game_cerver->lobby_id_generator = lobby_id_generator;

}

// option to set the game cerver comparator
void game_set_player_comparator (GameCerver *game_cerver, Comparator player_comparator) {

    if (game_cerver) game_cerver->player_comparator = player_comparator;

}

// sets a way to get and destroy game cerver game data
void game_set_load_game_data (GameCerver *game_cerver, 
    Action load_game_data, Action delete_game_data) {

    if (game_cerver) {
        game_cerver->load_game_data = load_game_data;
        game_cerver->delete_game_data = delete_game_data;
    }

}

// option to set an action to be performed right before the game cerver teardown
// eg. send a message to all players
void game_set_final_action (GameCerver *game_cerver, 
    Action final_action, void *final_action_args) {

    if (game_cerver) {
        game_cerver->final_game_action = final_action;
        game_cerver->final_action_args = final_action_args;
    }

}

static void game_lobby_create (Packet *packet) {

    if (packet) {
        if (packet->data_size >= sizeof (RequestData) + sizeof (SStringS)) {
            char *end = (char *) packet->data;
            SStringS *stype = (SStringS *) (end += sizeof (RequestData));

            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, 
                c_string_create ("Client %ld requested to create a new lobby in cerver %s of tye: %s",
                packet->client->id, packet->cerver->info->name->str, stype->string));
            #endif

            Lobby *lobby = lobby_create (packet->cerver, packet->client);
            if (lobby) {
                GameCerver *game_cerver = (GameCerver *) packet->cerver->cerver_data;
                GameType *game_type = game_type_get_by_name (game_cerver->game_types, stype->string);
                if (game_type) {
                    // FIXME: add the configuration to the lobby based on game type
                    lobby->game_type = game_type;

                    // register the lobby to the cerver
                    dlist_insert_after (game_cerver->current_lobbys, dlist_end (game_cerver->current_lobbys), lobby);

                    game_cerver->stats->lobbys_created += 1;
                    game_cerver->stats->current_active_lobbys += 1;

                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_SUCCESS, LOG_GAME,
                        c_string_create ("Created new lobby! Sending back to client %ld...",
                        packet->client->id));
                    #endif

                    lobby_send (lobby, true, packet->cerver, packet->client, packet->connection);
                }

                else {
                    // the requested game type was not found in cerver, send and error to client
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
                        c_string_create ("Failed to find %s game type in cerver %s!",
                        stype->string, packet->cerver->info->name->str));
                    #endif
                    Packet *error_packet = error_packet_generate (ERR_CREATE_LOBBY, "Bad game type!");
                    if (error_packet) {
                        packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                        packet_send (error_packet, 0, NULL);
                        packet_delete (error_packet);
                    }
                }
            }

            else {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
                    c_string_create ("Failed to create a new lobby in cerver %s!",
                    packet->cerver->info->name->str));
                #endif
                // send error packet to client
                Packet *error_packet = error_packet_generate (ERR_CREATE_LOBBY, "Cerver error while creating lobby!");
                if (error_packet) {
                    packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                    packet_send (error_packet, 0, NULL);
                    packet_delete (error_packet);
                }
            }
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
                "Failed to retreive game type to create lobby!");
            #endif
            // send error packet back to client
            Packet *error_packet = error_packet_generate (ERR_CREATE_LOBBY, "No game type provided!");
            if (error_packet) {
                packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                packet_send (error_packet, 0, NULL);
                packet_delete (error_packet);
            }
        }
    }

}

static void game_lobby_join_specific (Packet *packet, LobbyJoin *lj) {

    if (packet && lobby_join) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, 
            c_string_create ("Client %ld requested to join lobby with id ""%s"" in cerver %s.",
            packet->client->id, lj->lobby_id.string, packet->cerver->info->name->str));
        #endif

        Lobby *lobby = lobby_search_by_id (packet->cerver, lj->lobby_id.string);
        if (lobby) {
            if (!lobby_join (packet->cerver, packet->client, lobby)) {
                // success joinning the lobby, send back the lobby
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_SUCCESS, LOG_GAME, 
                    c_string_create ("Client %ld joined lobby with id: %s",
                    packet->client->id, lobby->id->str));
                #endif

                lobby_send (lobby, false, packet->cerver, packet->client, packet->connection);
            }

            else {
                // client failed to join the lobby - cerver error
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME,
                    c_string_create ("Client %ld failed to join lobby %s!",
                    packet->client->id, lobby->id->str));
                #endif
                Packet *error_packet = error_packet_generate (ERR_JOIN_LOBBY, "Failed to join lobby!");
                if (error_packet) {
                    packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                    packet_send (error_packet, 0, NULL);
                    packet_delete (error_packet);
                }
            }
        }

        else {
            // failed to tget the lobby with matching id
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
                c_string_create ("Failed to get lobby with id: ""%s"" in cerver %s!", 
                lj->lobby_id.string, packet->cerver->info->name->str));
            #endif
            Packet *error_packet = error_packet_generate (ERR_JOIN_LOBBY, "Not lobby found with id!");
            if (error_packet) {
                packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                packet_send (error_packet, 0, NULL);
                packet_delete (error_packet);
            }
        }
    }

}

// search for a suitable lobby for the player
// TODO: what to do if non is found? create a new one? or just return an error?
static void game_lobby_join_search (Packet *packet, LobbyJoin *lj) {

    if (packet && lj) {
        #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME,
            c_string_create ("Client %ld request to join a lobby of type: %s in cerver %s",
            packet->client->id, lj->game_type.string, packet->cerver->info->name->str));
        #endif
    }

}

static void game_lobby_join (Packet *packet) {

    if (packet) {
        if (packet->data_size >= sizeof (RequestData) + sizeof (LobbyJoin)) {
            char *end = (char *) packet->data;
            end += sizeof (RequestData);
            LobbyJoin *lj = (LobbyJoin *) end;

            // check if we have to search a lobby for the player
            // or he wants to join an specific lobby 
            if (lj->lobby_id.len > 0) game_lobby_join_specific (packet, lj);
            else game_lobby_join_search (packet, lj);
        }

        else {
            #ifdef CERVER_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
                "Failed to retreive info to join lobby!");
            #endif
            // send error packet back to client
            Packet *error_packet = error_packet_generate (ERR_JOIN_LOBBY, "Failed to get lobby!");
            if (error_packet) {
                packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                packet_send (error_packet, 0, NULL);
                packet_delete (error_packet);
            }
        }
    }

}

static void game_lobby_leave (Packet *packet) {

    if (packet) {

    }

}

// prepares lobby's game data structures
static void game_lobby_init (Packet *packet) {

    // TODO:

}

// FIXME: how do we check that the owner made the request?
// initializes the lobby update (starts the game)
static void game_lobby_start (Packet *packet) {

    if (packet) {
        if (packet->data_size >= sizeof (RequestData) + sizeof (SStringS)) {
            char *end = (char *) packet->data;
            end += sizeof (RequestData);
            SStringS *lobby_id = (SStringS *) end;

            #ifdef CERVER_DEBUG
            cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, 
                c_string_create ("Client %ld requested to start lobby with id ""%s"" in cerver %s.",
                packet->client->id, lobby_id->string, packet->cerver->info->name->str));
            #endif

            Lobby *lobby = lobby_search_by_id (packet->cerver, lobby_id->string);
            if (lobby) {
                lobby->game_type->start (lobby);
                if (!lobby_start (packet->cerver, lobby)) {
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stdout, LOG_SUCCESS, LOG_GAME, 
                        c_string_create ("Lobby %s has started in cerver %s!",
                        lobby->id->str, packet->cerver->info->name->str));
                    #endif

                    // send success packet back to client
                    Packet *success_packet = packet_generate_request (GAME_PACKET, GAME_START, NULL, 0);
                    if (success_packet) {
                        packet_set_network_values (success_packet, packet->cerver, packet->client, packet->connection);
                        packet_send (success_packet, 0, NULL);
                        packet_delete (success_packet);
                    }
                }

                else {
                    // failed to start the lobby - cerver error
                    #ifdef CERVER_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_GAME,
                        c_string_create ("Lobby %s has failed to start in cerver %s!",
                        lobby->id->str, packet->cerver->info->name->str));
                    #endif
                    Packet *error_packet = error_packet_generate (ERR_GAME_START, 
                        "Failed to start game in lobby!");
                    if (error_packet) {
                        packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                        packet_send (error_packet, 0, NULL);
                        packet_delete (error_packet);
                    }
                }
            }

            else {
                // failed to get the lobby with matching id
                #ifdef CERVER_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, 
                    c_string_create ("Failed to get lobby with id: ""%s"" in cerver %s!", 
                    lobby_id->string, packet->cerver->info->name->str));
                #endif
                Packet *error_packet = error_packet_generate (ERR_GAME_START, "Not lobby found with id!");
                if (error_packet) {
                    packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                    packet_send (error_packet, 0, NULL);
                    packet_delete (error_packet);
                }
            }
        }

        else {
            cerver_log_msg (stderr, LOG_ERROR, LOG_GAME, "Failed to retreive info to start lobby!");
            Packet *error_packet = error_packet_generate (ERR_GAME_START, "Failed to get lobby!");
            if (error_packet) {
                packet_set_network_values (error_packet, packet->cerver, packet->client, packet->connection);
                packet_send (error_packet, 0, NULL);
                packet_delete (error_packet);
            }
        }
    }

}

// handles a game type packet
void game_packet_handler (Packet *packet) {

    if (packet) {
        if (packet->data && (packet->data_size >= sizeof (RequestData))) {
            RequestData *req = (RequestData *) (packet->data);
            switch (req->type) {
                case GAME_LOBBY_CREATE: game_lobby_create (packet); break;
                case GAME_LOBBY_JOIN: game_lobby_join (packet); break;
                case GAME_LOBBY_LEAVE: game_lobby_leave (packet); break;
                case GAME_LOBBY_UPDATE: break;
                case GAME_LOBBY_DESTROY: break;

                // prepares lobby's game data structures
                case GAME_INIT: game_lobby_init (packet); break;

                // initializes the lobby update (starts the game)
                case GAME_START: game_lobby_start (packet); break;

                default:
                    cerver_log_msg (stderr, LOG_WARNING, LOG_CLIENT,
                        "Got a game packet of unknown type!");
                    break;
            }
        }
    }

}