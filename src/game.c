/*** The logic that makes a game server work properly 
 * This file is part of the framework. It provides an interface to create lobby and players 
 * ***/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "blackrock/blackrock.h" 
#include "game.h"

#include "utils/myUtils.h"
#include "utils/list.h"
#include "utils/objectPool.h"
#include "utils/config.h"
#include "utils/log.h"

#include <poll.h>
#include <errno.h>
#include "utils/avl.h"

GamePacketInfo *newGamePacketInfo (Server *server, Lobby *lobby, Player *player, 
    char *packetData, size_t packetSize);

/*** PLAYER ***/

#pragma region PLAYER

// FIXME: player ids, we cannot have infinite ids!!
// constructor for a new player, option for an object pool
Player *newPlayer (Pool *pool, Client *client, Player *player) {

    Player *new = NULL;

    if (pool) {
        if (POOL_SIZE (pool) > 0) {
            new = pool_pop (pool);
            if (!new) new = (Player *) malloc (sizeof (Player));
        }
    }

    else new = (Player *) malloc (sizeof (Player));

    // if we have initialization parameters...
    if (client) new->client = client;
    else new->client = NULL;

    if (player) {
        new->client = player->client;
        new->alive = player->alive;
        new->inLobby = player->inLobby;
    }

    // default player values
    else {
        new->client = NULL;
        new->alive = false;
        new->inLobby = false;
    } 

    new->id = nextPlayerId;
    nextPlayerId++;

    return new;

}

// TODO: what happens with the client data??
// FIXME: clean up the player structure
// deletes a player struct for ever
void deletePlayer (void *data) {

    if (data) {
        Player *player = (Player *) data;

        free (player);
    }

}

// FIXME: players
// inits the players server's structures
void game_initPlayers (GameServerData *gameData, u8 n_players) {

    if (!gameData) {
        logMsg (stderr, ERROR, SERVER, "Can't init players in a NULL game data!");
        return;
    }

    // if (gameData->players) logMsg (stdout, WARNING, SERVER, "The server has already a list of players.");
    // else gameData->players = initList (deletePlayer);

    if (gameData->playersPool)  logMsg (stdout, WARNING, SERVER, "The server has already a pool of players.");
    else gameData->playersPool = pool_init (deletePlayer);

    for (u8 i = 0; i < n_players; i++) pool_push (gameData->playersPool, malloc (sizeof (Player)));

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Players have been init in the game server.");
    #endif

}

// adds a player to the server's players struct (avl)
void player_registerToServer (Server *server, Player *player) {

    if (server && player) {
        if (server->type == GAME_SERVER) {
            GameServerData *gameData = (GameServerData *) server->serverData;
            if (gameData->players) avl_insertNode (gameData->players, player);
        }

        else {
            #ifdef DEBUG
                logMsg (stdout, WARNING, SERVER, 
                    "Trying to add a player to a server of incompatible type!");
            #endif
        }
    }

}

// TODO: log a time stamp?
// add a player to the lobby structures
u8 player_addToLobby (Lobby *lobby, Player *player) {

    if (lobby && player) {
        // register a player to the lobby
        avl_insertNode (lobby->players, player);
        lobby->players_fds[lobby->players_nfds].fd = player->client->clientSock;
        lobby->players_fds[lobby->players_nfds].events = POLLIN;
        lobby->players_nfds++;

        player->inLobby = true;

        return 0;
    }

    return 1;    

}

// FIXME:
// removes a player from the lobby's players structures and sends it to the game server's players
u8 player_removeFromLobby (GameServerData *gameData, Lobby *lobby, Player *player) {

    if (lobby && player) {
        // create a new player and add it to the server's players
        Player *p = newPlayer (gameData->playersPool, player);
        // FIXME: what type of struct do we have for players??
        //  insertAfter (gameData->players, LIST_END (gameData->players), p);
        p->inLobby = false;

        // remove from the poll fds
        for (u8 i = 0; i < lobby->players_nfds; i++) {
            if (lobby->players_fds[i].fd == player->client->clientSock) {
                lobby->players_fds[i].fd = -1;
                lobby->players_nfds--;
                lobby->compress_players = true;
                break;
            }
        }

        // delete the player from the lobby
        avl_removeNode (lobby->players, player);

        return 0;
    }

    return 1;

}

// comparator for players's avl tree
int playerComparator (const void *a, const void *b) {

    if (a && b) {
        Player *player_a = (Player *) a;
        Player *player_b = (Player *) b;

        if (player_a->client->clientSock > player_b->client->clientSock) return 1;
        else if (player_a->client->clientSock == player_b->client->clientSock) return 0;
        else return -1;
    }

}

// search a player in the lobbys players avl by his sock's fd
Player *getPlayerBySock (AVLTree *players, i32 fd) {

    if (players) {
        Player *temp = (Player *) malloc (sizeof (Player));
        temp->client = (Client *) malloc (sizeof (Client));
        temp->client->clientSock = fd;

        void *data = avl_getNodeData (players, temp);

        free (temp->client);
        free (temp);

        if (data) return (Player *) data;
        else {
            #ifdef DEBUG
                logMsg (stderr, ERROR, SERVER, 
                    createString ("Couldn't find a player associated with the passed fd: %i.", fd));
            #endif
            return NULL;
        } 
    }

    return NULL;

}

// broadcast a packet/msg to all clients inside an avl structure
void broadcastToAllPlayers (AVLNode *node, Server *server, void *packet, size_t packetSize) {

    if (node && server && packet && (packetSize > 0)) {
        broadcastToAllPlayers (node->right, server, packet, packetSize);

        // send packet to curent player
        if (node->id) {
            Player *player = (Player *) node->id;
            if (player) 
                server->protocol == IPPROTO_TCP ?
                tcp_sendPacket (player->client->clientSock, packet, packetSize, 0) :
                udp_sendPacket (server, packet, packetSize, player->client->address);
        }

        broadcastToAllPlayers (node->left, server, packet, packetSize);
    }

}

// TODO:
// this is used to clean disconnected players inside a lobby
// if we haven't recieved any kind of input from a player, disconnect it 
void checkPlayerTimeouts (void) {
}

// TODO: 10/11/2018 -- do we need this?
// when a client creates a lobby or joins one, it becomes a player in that lobby
void tcpAddPlayer (Server *server) {
}

// TODO: 10/11/2018 -- do we need this?
// TODO: as of 22/10/2018 -- we only have support for a tcp connection
// when we recieve a packet from a player that is not in the lobby, we add it to the game session
void udpAddPlayer () {
}

// TODO: 10/11/2018 -- do we need this?
// TODO: this is used in space shooters to add a new player using a udp protocol
// FIXME: handle a limit of players!!
void addPlayer (struct sockaddr_storage address) {

    // TODO: handle ipv6 ips
    char addrStr[IP_TO_STR_LEN];
    sock_ip_to_string ((struct sockaddr *) &address, addrStr, sizeof (addrStr));
    logMsg (stdout, SERVER, PLAYER, createString ("New player connected from ip: %s @ port: %d.\n", 
        addrStr, sock_ip_port ((struct sockaddr *) &address)));

    // TODO: init other necessarry game values
    // add the new player to the game
    // Player newPlayer;
    // newPlayer.id = nextPlayerId;
    // newPlayer.address = address;

    // vector_push (&players, &newPlayer);

    // FIXME: this is temporary
    // spawnPlayer (&newPlayer);

}

#pragma endregion

/*** LOBBY ***/

#pragma region LOBBY

// 10/11/2018 - aux reference to a server and lobby for thread functions
typedef struct ServerLobby {

    Server *server;
    Lobby *lobby;

} ServerLobby;

// we remove any fd that was set to -1 for what ever reason
void compressPlayers (Lobby *lobby) {

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

void handleGamePacket (void *);

// recieves packages from players inside the lobby
void handlePlayersInLobby (void *data) {

    if (!data) {
        logMsg (stderr, ERROR, SERVER, "Can't handle packets of a NULL lobby!");
        return;
    }

    Server *server = ((ServerLobby *) data)->server;
    Lobby *lobby = ((ServerLobby *) data)->lobby;

    ssize_t rc;                                  // retval from recv -> size of buffer
    char packetBuffer[MAX_UDP_PACKET_SIZE];      // buffer for data recieved from fd
    GamePacketInfo *info = NULL;

    #ifdef CERVER_DEBUG
    logMsg (stdout, SUCCESS, SERVER, "New lobby has started!");
    #endif

    int poll_retval;    // ret val from poll function
    int currfds;        // copy of n active server poll fds
    while (lobby->isRunning) {
        poll_retval = poll (lobby->players_fds, lobby->players_nfds, lobby->pollTimeout);

        // poll failed
        if (poll_retval < 0) {
            logMsg (stderr, ERROR, SERVER, "Lobby poll failed!");
            perror ("Error");
            lobby->isRunning = false;
            break;
        }

        // if poll has timed out, just continue to the next loop... 
        if (poll_retval == 0) {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, SERVER, "Lobby poll timeout.");
            #endif
            continue;
        }

        // one or more fd(s) are readable, need to determine which ones they are
        currfds = lobby->players_nfds;
        for (u8 i = 0; i < currfds; i++) {
            if (lobby->players_fds[i].fd <= -1) continue;

            if (lobby->players_fds[i].revents == 0) continue;

            // FIXME: how to hanlde an unexpected result??
            if (lobby->players_fds[i].revents != POLLIN) {
                // TODO: log more detailed info about the fd, or client, etc
                // printf("  Error! revents = %d\n", fds[i].revents);
                logMsg (stderr, ERROR, GAME, "Unexpected poll result!");
            }

            do {
                rc = recv (lobby->players_fds[i].fd, packetBuffer, sizeof (packetBuffer), 0);
                
                if (rc < 0) {
                    if (errno != EWOULDBLOCK) {
                        logMsg (stderr, ERROR, SERVER, "On hold recv failed!");
                        perror ("Error:");
                    }

                    break;  // there is no more data to handle
                }

                if (rc == 0) break;

                info = newGamePacketInfo (server, lobby, 
                    getPlayerBySock (lobby->players, lobby->players_fds[i].fd), packetBuffer, rc);

                // FIXME: where is the thpool?
                thpool_add_work (server->thpool, (void *) handleGamePacket, info);
            } while (true);
        }

        if (lobby->compress_players) compressPlayers (lobby);
    }

}

Lobby *newLobby (Server *server) {

    if (server) {
        if (server->type == GAME_SERVER) {
            Lobby *lobby = NULL;
            GameServerData *data = (GameServerData *) server->serverData;
            if (data->lobbyPool) {
                if (POOL_SIZE (data->lobbyPool) > 0) {
                    lobby = (Lobby *) pool_pop (data->lobbyPool);
                    if (!lobby) lobby = (Lobby *) malloc (sizeof (Lobby));
                }
            }

            else {
                logMsg (stderr, WARNING, SERVER, "Game server has no refrence to a lobby pool!");
                lobby = (Lobby *) malloc (sizeof (Lobby));
            }

            return lobby;
        }

        else return NULL;
    }

    else return NULL;

}

// FIXME: !!!!!
// FIXME: clear the world
// deletes a lobby for ever -- called when we teardown the server
// we do not need to give any feedback to the players if there is any inside
void deleteLobby (void *data) {

    if (!data) return;

    Lobby *lobby = (Lobby *) data;

    lobby->inGame = false;      // just to be sure
    lobby->owner = NULL;

    // check that the lobby is empty
    if (lobby->players_nfds > 0) {
        // FIXME: send the players to the server player list

        // OLD
        // Player *tempPlayer = NULL;
        // while (lobby->players.elements > 0) {
        //     tempPlayer = vector_get (&lobby->players, 0);
        //     // removePlayerFromLobby (gameData, lobby->players, 0, tempPlayer);
        //     // 26/10/2018 -- just delete the player, it may be a left behind
        //     deletePlayer (tempPlayer);
        // }
    }

    // clear lobby data
    if (lobby->settings) free (lobby->settings);
    // FIXME: is this a cerver function or a blackrock one??
    // if (lobby->world) deleteWorld (world);   

    free (lobby); 

}

// create a list to manage the server lobbys
// called when we init the game server
void game_initLobbys (GameServerData *gameData, u8 n_lobbys) {

    if (!gameData) {
        logMsg (stderr, ERROR, SERVER, "Can't init lobbys in a NULL game data!");
        return;
    }

    if (gameData->currentLobbys) logMsg (stdout, WARNING, SERVER, "The server has already a list of lobbys.");
    else gameData->currentLobbys = initList (deleteLobby);

    if (gameData->lobbyPool)  logMsg (stdout, WARNING, SERVER, "The server has already a pool of lobbys.");
    else gameData->lobbyPool = pool_init (deleteLobby);

    for (u8 i = 0; i < n_lobbys; i++) pool_push (gameData->lobbyPool, malloc (sizeof (Lobby)));

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Lobbys have been init in the game server.");
    #endif

}

// loads the settings for the selected game type from the game server data
GameSettings *getGameSettings (Config *gameConfig, u8 gameType) {

    ConfigEntity *cfgEntity = getEntityWithId (gameConfig, gameType);
	if (!cfgEntity) {
        logMsg (stderr, ERROR, GAME, "Problems with game settings config!");
        return NULL;
    } 

    GameSettings *settings = (GameSettings *) malloc (sizeof (GameSettings));

    char *playerTimeout = getEntityValue (cfgEntity, "playerTimeout");
    if (playerTimeout) settings->playerTimeout = atoi (playerTimeout);
    else {
        logMsg (stdout, WARNING, GAME, "No player timeout found in cfg. Using default.");        
        settings->playerTimeout = DEFAULT_PLAYER_TIMEOUT;
    }

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, createString ("Player timeout: %i", settings->playerTimeout));
    #endif

    char *fps = getEntityValue (cfgEntity, "fps");
    if (fps) settings->fps = atoi (fps);
    else {
        logMsg (stdout, WARNING, GAME, "No fps found in cfg. Using default.");        
        settings->fps = DEFAULT_FPS;
    }

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, createString ("FPS: %i", settings->fps));
    #endif

    char *minPlayers = getEntityValue (cfgEntity, "minPlayers");
    if (minPlayers) settings->minPlayers = atoi (minPlayers);
    else {
        logMsg (stdout, WARNING, GAME, "No min players found in cfg. Using default.");        
        settings->minPlayers = DEFAULT_MIN_PLAYERS;
    }

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, createString ("Min players: %i", settings->minPlayers));
    #endif

    char *maxPlayers = getEntityValue (cfgEntity, "minPlayers");
    if (maxPlayers) settings->maxPlayers = atoi (minPlayers);
    else {
        logMsg (stdout, WARNING, GAME, "No max players found in cfg. Using default.");        
        settings->maxPlayers = DEFAULT_MIN_PLAYERS;
    }

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, createString ("Max players: %i", settings->maxPlayers));
    #endif

    if (playerTimeout) free (playerTimeout);
    if (fps) free (fps);
    if (minPlayers) free (minPlayers);
    if (maxPlayers) free (maxPlayers);

    return settings;

}

// TODO: add a timestamp of the creation of the lobby
// creates a new lobby and inits his values with an owner and a game type
Lobby *createLobby (Server *server, Player *owner, GameType gameType) {

    if (!server) {
        logMsg (stderr, ERROR, SERVER, "Don't know in which server to create the lobby!");
        return NULL;
    }

    else {
        if (server->type != GAME_SERVER) {
            logMsg (stderr, ERROR, SERVER, "Can't create a lobby in a server of incorrect type!");
            return NULL;
        }
    }

    if (!owner) {
        logMsg (stderr, ERROR, SERVER, "A NULL player can't create a lobby!");
        return NULL;
    }

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Creatting a new lobby...");
    #endif

    GameServerData *data = (GameServerData *) server->serverData;
    Lobby *lobby = newLobby (server);

    lobby->owner = owner;
    if (lobby->settings) free (lobby->settings);
    lobby->settings = getGameSettings (data->gameSettingsConfig, gameType);
    if (!lobby->settings) {
        logMsg (stderr, ERROR, GAME, "Failed to get the settings for the new lobby!");
        lobby->owner = NULL;
        // send the lobby back to the object pool
        pool_push (data->lobbyPool, lobby);

        return NULL;
    } 

    // init the clients/players structures inside the lobby
    lobby->players = avl_init (playerComparator, deletePlayer);
    lobby->players_nfds = 0;
    lobby->compress_players = false;
    lobby->pollTimeout = server->pollTimeout;    // inherit the poll timeout from server

    if (!player_addToLobby (lobby, owner)) {
        lobby->inGame = false;

        // add the lobby the server active ones
        insertAfter (data->currentLobbys, LIST_END (data->currentLobbys), lobby);

        lobby->isRunning = true;

        // create a unique thread to handle this lobby
        ServerLobby sl = { .server = server, .lobby = lobby };
        thpool_add_work (server->thpool, (void *) handlePlayersInLobby, &sl);

        return lobby;
    }

    else {
        logMsg (stderr, ERROR, GAME, "Failed to register the owner of the lobby to the lobby!");
        logMsg (stderr, ERROR, GAME, "Failed to create new lobby!");

        pool_push (data->lobbyPool, lobby);      // dispose the lobby

        return NULL;
    }

}

// only the owner of the lobby can destroy the lobby
// a lobby should only be destroyed when there are no players left or if we teardown the server
u8 destroyLobby (Server *server, Lobby *lobby) {

    if (!server) {
        logMsg (stderr, ERROR, SERVER, "Don't know from which server to destroy the lobby!");
        return 1;
    }

    else {
        if (server->type != GAME_SERVER) {
            logMsg (stderr, ERROR, SERVER, "Can't destroy a lobby from a server of wrong type!");
            return 1;
        }
    }

    if (!lobby) {
        #ifdef DEBUG
        logMsg (stderr, ERROR, GAME, "Can't destroy an empty lobby!");
        #endif
        return 1;
    }

    GameServerData *gameData = (GameServerData *) server->serverData;
    if (!gameData) {
        logMsg (stderr, ERROR, SERVER, "No game data found in the server!");
        return 1;
    } 

    if (lobby->players_nfds > 0) {
        // send the players the correct package so they can handle their logic
        // expected player behaivor -> leave the lobby 
        size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);
        void *lobbyPacket = generatePacket (GAME_PACKET, packetSize);
        char *end = lobbyPacket + sizeof (PacketHeader); 

        RequestData *rdata = (RequestData *) end;
        if (!lobbyPacket) logMsg (stderr, ERROR, PACKET, "Failed to create lobby destroy packet!");
        else {
            broadcastToAllPlayers (lobby->players->root, server, lobbyPacket, packetSize);
            free (lobbyPacket);
        }

        // remove the players from this structure and send them to the server's players
        Player *tempPlayer = NULL;
        while (lobby->players_nfds > 0) {
            tempPlayer = (Player *) lobby->players->root->id;
            if (tempPlayer) player_removeFromLobby (gameData, lobby, tempPlayer);
        }
    }

    lobby->owner = NULL;
    if (lobby->settings) free (lobby->settings);

    // we are safe to clear the lobby structure
    // first remove the lobby from the active ones, then send it to the inactive ones
    ListElement *le = getListElement (gameData->currentLobbys, lobby);
    if (le) {
        void *temp = removeElement (gameData->currentLobbys, le);
        if (temp) pool_push (gameData->lobbyPool, temp);
    }

    else {
        logMsg (stdout, WARNING, GAME, "A lobby wasn't found in the current lobby list.");
        free (lobby);   // destroy the lobby forever
    } 
    
    return 0;   // success

}

// FIXME:
// TODO: pass the correct game type and maybe create a more advance algorithm
// finds a suitable lobby for the player
Lobby *findLobby (Server *server) {

    // FIXME: how do we want to handle these errors?
    // perform some check here...
    if (!server) return NULL;
    if (server->type != GAME_SERVER) {
        logMsg (stderr, ERROR, SERVER, "Can't search for lobbys in non game server.");
        return NULL;
    }
    
    GameServerData *gameData = (GameServerData *) server->serverData;
    if (!gameData) {
        logMsg (stderr, ERROR, SERVER, "NULL reference to game data in game server!");
        return NULL;
    }

    // 11/11/2018 -- we have a simple algorithm that only searches for the first available lobby
    Lobby *lobby = NULL;
    for (ListElement *e = LIST_START (gameData->currentLobbys); e != NULL; e = e->next) {
        lobby = (Lobby *) e->data;
        if (lobby) {
            if (!lobby->inGame) {
                if (lobby->players_nfds < lobby->settings->maxPlayers) {
                    // we have found a suitable lobby
                    break;
                }
            }
        }
    }

    // TODO: what do we do if we do not found a lobby? --> create a new one?
    if (!lobby) {

    }

    return lobby;

}

// FIXME:
// called by a registered player that wants to join a lobby on progress
u8 joinLobby (Server *server, Lobby *lobby, Player *player) {

    if (!lobby) {
        #ifdef DEBUG
        logMsg (stderr, ERROR, GAME, "Can't join an empty lobby!");
        #endif
        return 1;
    }

    if (!player) {
        #ifdef DEBUG
        logMsg (stderr, ERROR, GAME, "A NULL player can't join a lobby!");
        #endif
        return 1;
    }

    // check if for whatever reason a player al ready inside the lobby wants to join...
    Player *tempPlayer = NULL;
    for (u8 i = 0; i < lobby->players_nfds; i++) {
        if (lobby->players_fds[i].fd == player->client->clientSock) {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "A player tries to join the same lobby he is in.");
            #endif
            sendErrorPacket (server, player->client, ERR_JOIN_LOBBY, "You can't join the same lobby you are in!");
            return 1;
        }
    }

    // check that the player can join the actual game...
    if (lobby->inGame) {
        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "A player tried to join a lobby that is in game.");
        #endif
        sendErrorPacket (server, player->client, ERR_JOIN_LOBBY, "A game is in progress in the lobby!");
        return 1;
    }

    // lobby is already full
    if (lobby->players_nfds >= lobby->settings->maxPlayers) {
        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "A player tried to join a full lobby.");
        #endif
        sendErrorPacket (server, player->client, ERR_JOIN_LOBBY, "The lobby is already full!");
        return 1;
    }

    // FIXME: remove the players from the previous poll fd structure!!
    // the player is clear to join the lobby...
    // move the player from the server's players to the in lobby players
    GameServerData *gameData = (GameServerData *) server->serverData;
    // tempPlayer = dllist_getPlayerById (gameData->players, player->id);
    if (!tempPlayer) {
        logMsg (stderr, ERROR, GAME, "A player wan't found in the current player -- join lobby");
        return 1;
    }

    // FIXME: be sure to send the correct player ptr
    if (!player_addToLobby (lobby, tempPlayer)) {
        // sync the in lobby player(s) and the new player
        // FIXME: lobby serialized packets
        size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + lobby->players_nfds * sizeof (SPlayer);
        void *packet = createLobbyPacket (LOBBY_UPDATE, lobby, packetSize);
        if (packet) {
            // send the packet to each player inside the lobby...
            broadcastToAllPlayers (lobby->players->root, server, packet, packetSize);
            #ifdef CERVER_DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "Broadcasted to players inside the lobby.");
            #endif
            free (packet);    // free the lobby packet
        }

        else logMsg (stderr, ERROR, PACKET, "Failed to create lobby update packet!");

        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "A new player has joined the lobby");
        #endif

        return 0;   // success
    }
    
    else {
        logMsg (stderr, ERROR, PLAYER, "Failed to register a player to a lobby!");
        return 1;
    }

}

// TODO: send feedback to the new owner
// FIXME: 04/11/2018 -- 20:10 - in game lobby logic!!
// TODO: add a timestamp when the player leaves
// called when a player requests to leave the lobby
u8 leaveLobby (Server *server, Lobby *lobby, Player *player) {

    if (!lobby) {
        #ifdef DEBUG
        logMsg (stderr, ERROR, GAME, "Can't leave a NULL lobby!");
        #endif
        return 1;
    }

    if (!player) {
        #ifdef DEBUG
        logMsg (stderr, ERROR, GAME, "A NULL player can't leave a lobby!");
        #endif
        return 1;
    }

    // make sure that the player is inside the lobby
    bool found = false;
    bool wasOwner = false;
    for (u8 i = 0; i < lobby->players_nfds; i++) {
        if (lobby->players_fds[i].fd == player->client->clientSock) {
            if (lobby->owner->client->clientSock == player->client->clientSock) 
                wasOwner = true;

            GameServerData *gameData = (GameServerData *) server->serverData;
            player_removeFromLobby (gameData, lobby, player);
            found = true;
            break;
        }
    }

    if (!found) {
        #ifdef DEBUG
        logMsg (stderr, ERROR, GAME, "The player doesn't belong to the lobby!");
        #endif
        return 1;
    }

    // there are still players in the lobby
    if (lobby->players_nfds > 0) {
        if (lobby->inGame) {
            // broadcast the other players that the player left
            // we need to send an update lobby packet and the players handle the logic
            size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + lobby->players_nfds * sizeof (SPlayer);
            void *lobbyPacket = createLobbyPacket (LOBBY_UPDATE, lobby, packetSize);
            if (lobbyPacket) {
                broadcastToAllPlayers (lobby->players->root, server, lobbyPacket, packetSize);
                free (lobbyPacket);
            }

            else logMsg (stderr, ERROR, PACKET, "Failed to create lobby update packet!");

            // if he was the owner -> set a new owner of the lobby -> first one in the array
            if (wasOwner) {
                Player *temp = NULL;
                u8 i = 0;
                do {
                    temp = getPlayerBySock (lobby->players, lobby->players_fds[i].fd);
                    if (temp) {
                        lobby->owner = temp;
                        // TODO: send feedback to player
                    }

                    // we got a NULL player in the structures -> we don't expect this to happen!
                    else {
                        logMsg (stdout, WARNING, GAME, "Got a NULL player when searching for new owner!");
                        lobby->players_fds[i].fd = -1;
                        lobby->compress_players = true; 
                        i++;
                    }
                } while (!temp);
            }
        }

        // players are in the lobby screen -> owner destroyed the server
        else destroyLobby (server, lobby);
    }

    // player that left was the last one 
    else {
        // if he was in a game 
        if (lobby->inGame) {
            // FIXME: end the game 
            // dispose the lobby
            destroyLobby (server, lobby);
        }

        // if he was in the lobby screen
        else destroyLobby (server, lobby);
    }
    
    return 0;   // the player left the lobby successfully

}

#pragma endregion

// 25/10/2018 -- what do we want to do server and client side?
/***
 * at the start of the game, we need to be sure that the client has the correct dbs and files
 * 
 * server: - init the map
 *         - init enemies
 *         - calculate enemy pathfinding and send to client the result
 *         - enemy loot
 *         - score and leaderboards
 *         - keyframes to sync the clients every so often
 *         - server can send messages to the clients' message log
 * 
 * client: - calculate fov: and send server the result
 *         - comabt and send the result
 * **/

/*** GAME THREAD ***/

#pragma region GAME LOGIC

// TODO: make sure that all the players inside the lobby are in sync before starting the game!!!
// this is called from by the owner of the lobby
u8 startGame (Server *server, Lobby *lobby) {

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Starting the game...");
    #endif

    // FIXME: support a retry function -> we don't need to generate this things again!
    // first init the in game structures

    // server side
    // init map
    // init enemies
    // place stairs and other map elements
    // initWorld (lobby->world);

    // client side
    // init their player
    // generate map and enemies based on our data

    // place each player in a near spot to each other
    // get the fov structure

    // TODO: make sure that all the players are sync and have inited their own game

    // FIXME: send messages to the client log
    // TODO: add different texts here!!
    // logMessage ("You have entered the dungeon!", 0xFFFFFFFF);

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Players have entered the dungeon!");
    #endif

    // init the game
    // start calculating pathfinding
    // sync fov of all players
    // sync player data like health
    // keep track of players score
    // sync players movement

}

// FIXME:
// stops the current game in progress
// called when the players don't finish the current game
void stopGame (void) {

    // logic to stop the current game in progress

}

// FIXME:
// called when the win condition has been reached inside a lobby
void endGame (void) {

    // logic to stop the current game in the server
    // send feedback to the players that the game has finished
    // send players the scores and leaderboard
    // the lobby owner can retry the game or exit to the lobby screen

}

#pragma endregion

// FIXME: where do we want to put this?
/*** REQUESTS FROM INSIDE THE LOBBY ***/

// a player wants to send a message in the lobby chat
void msgtoLobbyPlayers () {

}

/*** GAME UPDATES ***/

// handle a player input, like movement -> TODO: we need to check for collisions between enemies
void handlePlayerInput () {

}

#pragma region GAME PACKETS

/*** Prototypes of public game server functions ***/

/*** reqs outside a lobby ***/
void gs_createLobby (Server *, Client *, GameType);
void gs_joinLobby (Server *, Client *);

/*** reqs from inside a lobby ***/
void gs_leaveLobby (Server *, Player *, Lobby *);
void gs_initGame (Server *, Player *, Lobby *);
void gs_sendMsg (Server *, Player *, Lobby *, char *msg);

// TODO: todo public game sever functions
// player movement and updates

// this is called from the main poll in a new thread
void gs_handlePacket (PacketInfo *packetInfo) {

    RequestData *reqData = (RequestData *) (packetInfo->packetData + sizeof (PacketHeader));
    switch (reqData->type) {
        // TODO: get the correct game type from the packet
        case LOBBY_CREATE: gs_createLobby (packetInfo->server, packetInfo->client, 0); break;
        case LOBBY_JOIN: gs_joinLobby (packetInfo->server, packetInfo->client); break;
        // case LOBBY_LEAVE: gs_leaveLobby (packetInfo->server, packetInfo->client); break;
        case LOBBY_DESTROY: break;
    }

}

// TODO: where do we want to have a pool of these?
GamePacketInfo *newGamePacketInfo (Server *server, Lobby *lobby, Player *player, 
    char *packetData, size_t packetSize) {

    if (server && lobby && player && packetData && (packetSize > 0)) {
        GamePacketInfo *info = (GamePacketInfo *) malloc (sizeof (GamePacketInfo));
        if (info) {
            info->server = server;
            info->lobby = lobby;
            info->player = player;
            strcpy (info->packetData, packetData);
            info->packetSize = packetSize;

            return info;
        }

        return NULL;
    }

    return NULL;

}

// TODO:
void destroyGamePacketInfo (void *data) {}

// FIXME:
// 04/11/2018 -- handles all valid requests a player can make from inside a lobby
void handleGamePacket (void *data) {

    if (data) {
        GamePacketInfo *packet = (GamePacketInfo *) data;
        if (!checkPacket (packet->packetSize, packet->packetData, GAME_PACKET)) {
            RequestData *rdata = (RequestData *) packet->packetData + sizeof (PacketHeader);

            switch (rdata->type) {
                case LOBBY_LEAVE: break;
                case LOBBY_DESTROY: break;

                case GAME_INPUT_UPDATE:  break;
                case GAME_SEND_MSG: break;

                // TODO: dispose the packet -> send it to the pool
                default: break;
            }
        }

        // TODO: invalid packet -> dispose -> send it to the pool
        // else 
    }

}

// send game update packets to all players inside a lobby
// we need to serialize the server data in order to send it...
// FIXME:
// TODO: maybe we can clean and refactor this?
// creates and sends game packets
void sendGamePackets (Server *server, int to) {

    // Player *destPlayer = vector_get (&players, to);

    // first we need to prepare the packet...

    // TODO: clean this a little, but don't forget this can be dynamic!!
	// size_t packetSize = packetHeaderSize + updatedGamePacketSize +
	// 	players.elements * sizeof (Player);

	// // buffer for packets, extend if necessary...
	// static void *packetBuffer = NULL;
	// static size_t packetBufferSize = 0;
	// if (packetSize > packetBufferSize) {
	// 	packetBufferSize = packetSize;
	// 	free (packetBuffer);
	// 	packetBuffer = malloc (packetBufferSize);
	// }

	// void *begin = packetBuffer;
	// char *end = begin; 

	// // packet header
	// PacketHeader *header = (PacketHeader *) end;
    // end += sizeof (PacketHeader);
    // initPacketHeader (header, GAME_UPDATE_TYPE);

    // // TODO: do we need to send the game settings each time?
	// // game settings and other non-array data
    // UpdatedGamePacket *gameUpdate = (UpdatedGamePacket *) end;
    // end += updatedGamePacketSize;
    // // gameUpdate->sequenceNum = currentTick;  // FIXME:
	// // tick_packet->ack_input_sequence_num = dest_player->input_sequence_num;  // FIXME:
    // gameUpdate->playerId = destPlayer->id;
    // gameUpdate->gameSettings.playerTimeout = 30;    // FIXME:
    // gameUpdate->gameSettings.fps = 20;  // FIXME:

    // TODO: maybe add here some map elements, dropped items and enemies??
    // such as the players or explosions?
	// arrays headers --- 01/10/2018 -- we only have the players array
    // void *playersArray = (char *) gameUpdate + offsetof (UpdatedGamePacket, players);
    // FIXME:
	// void *playersArray = (char *) gameUpdate + offsetof (gameUpdate, players);

	// send player info ---> TODO: what player do we want to send?
    /* s_array_init (playersArray, end, players.elements);

    Player *player = NULL;
    for (size_t p = 0; p < players.elements; p++) {
        player = vector_get (&players, p);

        // FIXME: do we really need to serialize the packets??
        // FIXME: do we really need to have serializable data structs?

		// SPlayer *s_player = (SPlayer *) packet_end;
		// packet_end += sizeof(*s_player);
		// s_player->id = player->id;
		// s_player->alive = !!player->alive;
		// s_player->position = player->position;
		// s_player->heading = player->heading;
		// s_player->score = player->score;
		// s_player->color.red = player->color.red;
		// s_player->color.green = player->color.green;
		// s_player->color.blue = player->color.blue;
    }

    // FIXME: better eroror handling!!
    // assert (end == (char *) begin + packetSize); */

    // after the pakcet has been prepare, send it to the dest player...
    // sendPacket (server, begin, packetSize, destPlayer->address);

}

#pragma endregion

/*** PUBLIC FUNCTIONS ***/

// These functions are the ones that handle the request from the client/player
// They provide an interface to interact with the server
// Apart from this functions, all other logic must be PRIVATE to the client/player

#pragma region PUBLIC FUNCTIONS

/*** FROM OUTSIDE A LOBBY ***/

// TODO: this logic is okay --> but fix lobby packet
// request from a from client to create a new lobby 
void gs_createLobby (Server *server, Client *client, GameType gameType) {

    if (server && client) {
        GameServerData *gameData = (GameServerData *) server->serverData;

        // check if the client is associated with a player
        Player *owner = getPlayerBySock (gameData->players, client->clientSock);

        // create new player data for the client
        if (!owner) owner = newPlayer (gameData->playersPool, client, NULL);
        player_registerToServer (server, owner);
        client_unregisterFromServer (server, client);

        // check that the owner isn't already in a lobby or game
        if (owner->inLobby) {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "A player inside a lobby wanted to create a new lobby.");
            #endif
            if (sendErrorPacket (server, owner->client, ERR_CREATE_LOBBY, "Player is already in a lobby!")) {
                #ifdef DEBUG
                logMsg (stderr, ERROR, PACKET, "Failed to create & send error packet to client!");
                #endif
            }
            return;
        }

        Lobby *lobby = createLobby (server, owner, gameType);
        if (lobby) {
            #ifdef DEBUG
                logMsg (stdout, GAME, NO_TYPE, "New lobby created.");
            #endif  

            // send the lobby info to the owner -- we only have one player inside the lobby
            size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + sizeof (SPlayer);
            void *lobbyPacket = createLobbyPacket (LOBBY_CREATE, lobby, packetSize);
            if (lobbyPacket) {
                server->protocol == IPPROTO_TCP ?
                tcp_sendPacket (owner->client->clientSock, lobbyPacket, packetSize, 0) : 
                udp_sendPacket (server, lobbyPacket, packetSize, owner->client->address);
                free (lobbyPacket);
            }

            else logMsg (stderr, ERROR, PACKET, "Failed to create lobby packet!");

            // TODO: do we wait for an ack packet from the client?
        }

        // there was an error creating the lobby
        else {
            logMsg (stderr, ERROR, GAME, "Failed to create a new game lobby.");

            // send feedback to the player
            sendErrorPacket (server, owner->client, ERR_SERVER_ERROR, 
                "Game server failed to create new lobby!");
        }
    }

}

// FIXME:
// TODO: in a larger game, a player migth wanna join a lobby with his friend
// or we would have an algorithm to find a suitable lobby based on player stats
// as of 10/11/2018 -- a player just joins the first lobby that we find that is not full
void gs_joinLobby (Server *server, Client *client) {

    if (server && client) {
        GameServerData *gameData = (GameServerData *) server->serverData;

        // check if the client is associated with a player
        Player *player = getPlayerBySock (gameData->players, client->clientSock);

        // create new player data for the client
        if (!player) player = newPlayer (gameData->playersPool, client, NULL);
        player_registerToServer (server, player);
        client_unregisterFromServer (server, client);

        // check that the owner isn't already in a lobby or game
        if (player->inLobby) {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "A player inside a lobby wanted to join a new lobby.");
            #endif
            if (sendErrorPacket (server, player->client, ERR_CREATE_LOBBY, "Player is already in a lobby!")) {
                #ifdef DEBUG
                logMsg (stderr, ERROR, PACKET, "Failed to create & send error packet to client!");
                #endif
            }
            return;
        }

        Lobby *lobby = findLobby (server);

        if (lobby) {
            // add the player to the lobby
            if (!joinLobby (server, lobby, player)) {
                // the player joined successfully
            }

            // else TODO: there was a problem
        }

        // else TODO: there was a problem
        
    }

}

/*** FROM INSIDE A LOBBY ***/

// TODO: send feedback to the player
void gs_leaveLobby (Server *server, Player *player, Lobby *lobby) {

    if (server && player && lobby) {
        GameServerData *gameData = (GameServerData *) server->serverData;

        if (player->inLobby) {
            if (!leaveLobby (server, lobby, player)) {
                // success leaving the lobby
                // TODO: send feedback to the player
            }

            else {
                #ifdef DEBUG
                logMsg (stdout, DEBUG_MSG, GAME, "There was a problem with a player leaving a lobby!");
                #endif
                if (sendErrorPacket (server, player->client, ERR_LEAVE_LOBBY, "Problem with player leaving the lobby!")) {
                    #ifdef DEBUG
                    logMsg (stderr, ERROR, PACKET, "Failed to create & send error packet to client!");
                    #endif
                }
            }
        }

        else {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "A player tries to leave a lobby but he is not inside one!");
            #endif
            if (sendErrorPacket (server, player->client, ERR_LEAVE_LOBBY, "Player is not inside a lobby!")) {
                #ifdef DEBUG
                logMsg (stderr, ERROR, PACKET, "Failed to create & send error packet to client!");
                #endif
            }
        }
    }

}

// TODO: we need to set a delegate in the game server to init each type of game
void gs_initGame (Server *server, Player *player, Lobby *lobby) {

    if (server && player && lobby) {
        GameServerData *gameData = (GameServerData *) server->serverData;

        if (player->inLobby) {
            if (lobby->owner->id == player->id) {
                if (lobby->players_nfds >= lobby->settings->minPlayers) {
                    if (startGame (server, lobby)) {
                        logMsg (stderr, ERROR, GAME, "Failed to start a new game!");
                        // send feedback to the players
                        size_t packetSize = sizeof (PacketHeader) + sizeof (ErrorData);
                        void *errpacket = generateErrorPacket (ERR_GAME_INIT, 
                            "Server error. Failed to init the game.");
                        if (errpacket) {
                            broadcastToAllPlayers (lobby->players->root, server, errpacket, packetSize);
                            free (errpacket);
                        }

                        else {
                            #ifdef DEBUG
                            logMsg (stderr, ERROR, PACKET, 
                                "Failed to create & send error packet to client!");
                            #endif
                        }
                    }

                    // upon success, we expect the start game func to send the packages
                }

                else {
                    #ifdef DEBUG
                    logMsg (stdout, WARNING, GAME, "Need more players to start the game.");
                    #endif
                    if (sendErrorPacket (server, player->client, ERR_GAME_INIT, 
                        "We need more players to start the game!")) {
                        #ifdef DEBUG
                        logMsg (stderr, ERROR, PACKET, "Failed to create & send error packet to client!");
                        #endif
                    }
                }
            }

            else {
                #ifdef DEBUG
                logMsg (stdout, WARNING, GAME, "Player is not the lobby owner.");
                #endif
                if (sendErrorPacket (server, player->client, ERR_GAME_INIT, 
                    "Player is not the lobby owner!")) {
                    #ifdef DEBUG
                    logMsg (stderr, ERROR, PACKET, "Failed to create & send error packet to client!");
                    #endif
                }
            }
        }

        else {
            #ifdef DEBUG
            logMsg (stdout, WARNING, GAME, "Player must be inside a lobby and be the owner to start a game.");
            #endif
            if (sendErrorPacket (server, player->client, ERR_GAME_INIT, 
                "The player is not inside a lobby!")) {
                #ifdef DEBUG
                logMsg (stderr, ERROR, PACKET, "Failed to create & send error packet to client!");
                #endif
            }
        }
    }

}

// FIXME:
// a player wants to send a msg to the players inside the lobby
void gs_sendMsg (Server *server, Player *player, Lobby *lobby, char *msg) {

    if (server && player && lobby && msg) {

    }

}

#pragma endregion