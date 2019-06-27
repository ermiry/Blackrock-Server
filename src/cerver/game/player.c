#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/game/game.h"
#include "cerver/game/player.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

Player *player_new (void) {

    Player *player = (Player *) malloc (sizeof (Player));
    if (player) {
        player->id = NULL;
        player->client = NULL;
        player->data = NULL;
        player->data_delete = NULL;
    }

    return player;

}

void player_delete (void *player_ptr) {

    if (player_ptr) {
        Player *player = (Player *) player_ptr;

        str_delete (player->id);

        player->client = NULL;

        if (player->data) {
            if (player->data_delete)
                player->data_delete (player->data);
            else free (player->data);
        }

        free (player);
    }

}

// sets the player id
void player_set_id (Player *player, const char *id) {

    if (player) player->id = str_new (id);

}

// sets player data and a way to delete it
void player_set_data (Player *player, void *data, Action data_delete) {

    if (player) {
        player->data = data;
        player->data_delete = data_delete;
    }

}

int player_comparator_by_id (const void *a, const void *b) {

    if (a && b) 
        return str_compare (((Player *) a)->id, ((Player *) b)->id);

}

int player_comparator_client_id (const void *a, const void *b) {

    if (a && b) {
        return str_compare (((Player *) a)->client->client_id, 
            ((Player *) b)->client->client_id);
    }

}

// registers a player to the lobby --> add him to lobby's structures
u8 player_register_to_lobby (Lobby *lobby, Player *player) {

    u8 retval = 1;

    if (lobby && player) {
        if (player->client) {
            bool failed = false;

            // register all the player's client connections to the lobby poll
            Connection *connection = NULL;
            for (ListElement *le = dlist_start (player->client->connections); le; le = le->next) {
                connection = (Connection *) le->data;
                failed = lobby_poll_register_connection (lobby, player, connection);
            }

            if (!failed) {
                #ifdef CERVER_DEBUG
                cerver_log_msg (stdout, LOG_SUCCESS, LOG_PLAYER, 
                    c_string_create ("Registered a new player to lobby %s",
                    lobby->id->str));
                #endif

                lobby->n_current_players++;
                #ifdef CERVER_STATS
                cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME,
                    c_string_create ("Registered players to lobby %s: %i.",
                    lobby->id->str, lobby->n_current_players));
                #endif

                retval = 0;
            }
        }
    }

    return retval;

}

// unregisters a player from a lobby --> removes him from lobby's structures
u8 player_unregister_from_lobby (Lobby *lobby, Player *player) {

    u8 retval = 1;

    if (lobby && player) {
        if (player->client) {
            // unregister all the player's client connections from the lobby
            Connection *connection = NULL;
            for (ListElement *le = dlist_start (player->client->connections); le; le = le->next) {
                connection = (Connection *) le->data;
                lobby_poll_unregister_connection (lobby, player, connection);
            }
        }
    }

    return retval;

}

// get a player from an avl tree using a comparator and a query
Player *player_get (AVLNode *node, Comparator comparator, void *query) {

    if (node && query) {
        Player *player = NULL;

        player = player_get (node->right, comparator, query);

        if (!player) 
            if (!comparator (node->id, query)) return (Player *) node->id;

        if (!player) player = player_get (node->left, comparator, query)   ;

        return player;
    }

    return NULL;

}

// FIXME:
// recursively get the player associated with the socket
Player *player_get_by_socket (AVLNode *node, i32 socket_fd) {

    // if (node) {
    //     Player *player = NULL;

    //     player = player_get_by_socket (node->right, socket_fd);

    //     if (!player) {
    //         if (node->id) {
    //             player = (Player *) node->id;
                
    //             // search the socket fd in the clients active connections
    //             for (int i = 0; i < player->client->n_active_cons; i++)
    //                 if (socket_fd == player->client->active_connections[i])
    //                     return player;

    //         }
    //     }

    //     if (!player) player = player_get_by_socket (node->left, socket_fd);

    //     return player;
    // }

    return NULL;

}

// check if a player is inside a lobby using a comparator and a query
bool player_is_in_lobby (Lobby *lobby, Comparator comparator, void *query) {

    if (lobby && query) {
        Player *player = player_get (lobby->players->root, comparator, query);
        if (player) return true;
    }

    return false;

}

// FIXME: client socket!!
// broadcast a packet/msg to all clients/players inside an avl structure
void player_broadcast_to_all (AVLNode *node, Cerver *server, void *packet, size_t packetSize) {

    // if (node && server && packet && (packetSize > 0)) {
    //     player_broadcast_to_all (node->right, server, packet, packetSize);

    //     // send packet to curent player
    //     if (node->id) {
    //         Player *player = (Player *) node->id;
    //         if (player) 
    //             server_sendPacket (server, player->client->active_connections[0],
    //                 player->client->address, packet, packetSize);
    //     }

    //     player_broadcast_to_all (node->left, server, packet, packetSize);
    // }

}

// performs an action on every player in an avl tree 
void player_traverse (AVLNode *node, Action action, void *data) {

    if (node && action) {
        player_traverse (node->right, action, data);

        if (node->id) {
            PlayerAndData pd = { .playerData = node->id, .data = data };
            action (&pd);
        } 

        player_traverse (node->left, action, data);
    }

}

// inits the players server's structures
u8 game_init_players (GameServerData *gameData, Comparator player_comparator) {

    if (!gameData) {
        cerver_log_msg (stderr, LOG_ERROR, LOG_CERVER, "Can't init players in a NULL game data!");
        return 1;
    }

    if (gameData->players)
        cerver_log_msg (stdout, LOG_WARNING, LOG_CERVER, "The server already has an avl of players!");
    else {
        gameData->players = avl_init (player_comparator, player_delete);
        if (!gameData->players) {
            cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to init server's players avl!");
            return 1;
        }
    } 

    #ifdef CERVER_DEBUG
        cerver_log_msg (stdout, LOG_DEBUG, LOG_GAME, "Players have been init in the game server.");
    #endif

    return 0;

}


// TODO:
// this is used to clean disconnected players inside a lobby
// if we haven't recieved any kind of input from a player, disconnect it 
void checkPlayerTimeouts (void) {}

// TODO: 10/11/2018 -- do we need this?
// when a client creates a lobby or joins one, it becomes a player in that lobby
void tcpAddPlayer (Cerver *server) {}

// TODO: 10/11/2018 -- do we need this?
// TODO: as of 22/10/2018 -- we only have support for a tcp connection
// when we recieve a packet from a player that is not in the lobby, we add it to the game session
void udpAddPlayer () {}

// TODO: 10/11/2018 -- do we need this?
// TODO: this is used in space shooters to add a new player using a udp protocol
// FIXME: handle a limit of players!!
void addPlayer (struct sockaddr_storage address) {

    // TODO: handle ipv6 ips
    /* char addrStr[IP_TO_STR_LEN];
    sock_ip_to_string ((struct sockaddr *) &address, addrStr, sizeof (addrStr));
    cerver_log_msg (stdout, LOG_CERVER, LOG_PLAYER, c_string_create ("New player connected from ip: %s @ port: %d.\n", 
        addrStr, sock_ip_port ((struct sockaddr *) &address))); */

    // TODO: init other necessarry game values
    // add the new player to the game
    // Player newPlayer;
    // newPlayer.id = nextPlayerId;
    // newPlayer.address = address;

    // vector_push (&players, &newPlayer);

    // FIXME: this is temporary
    // spawnPlayer (&newPlayer);

}