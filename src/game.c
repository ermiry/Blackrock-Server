/*** The logic that makes a game server work properly 
 * This file is part of the framework. It provides an interface to create lobby and players 
 * ***/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "cerver.h"
#include "blackrock.h" 
#include "game.h"

#include "utils/myUtils.h"
#include "utils/list.h"
#include "utils/objectPool.h"
#include "utils/config.h"
#include "utils/log.h"

#include <poll.h>
#include <errno.h>
#include "utils/avl.h"

/*** PLAYER ***/

#pragma region PLAYER

// constructor for a new player, option for an object pool
Player *newPlayer (Pool *pool, Player *player) {

    Player *new = NULL;

    if (pool) {
        if (POOL_SIZE (pool) > 0) {
            new = pool_pop (pool);
            if (!new) new = (Player *) malloc (sizeof (Player));
        }
    }

    else new = (Player *) malloc (sizeof (Player));

    // if we have initialization parameters...
    if (player) {
        new->client = player->client;
        new->alive = player->alive;
        new->inLobby = player->inLobby;
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

// inits the players server's structures
void game_initPlayers (GameServerData *gameData, u8 n_players) {

    if (!gameData) {
        logMsg (stderr, ERROR, SERVER, "Can't init players in a NULL game data!");
        return;
    }

    if (gameData->players) logMsg (stdout, WARNING, SERVER, "The server has already a list of players.");
    else gameData->players = initList (deletePlayer);

    if (gameData->playersPool)  logMsg (stdout, WARNING, SERVER, "The server has already a pool of players.");
    else gameData->playersPool = pool_init (deletePlayer);

    for (u8 i = 0; i < n_players; i++) pool_push (gameData->playersPool, malloc (sizeof (Player)));

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Players have been init in the game server.");
    #endif

}

// FIXME: dont forget pollin!!!
// TODO: log a time stamp?
// add a player to the lobby structures
u8 player_registerToLobby (Lobby *lobby, Player *player) {

    if (lobby && player) {
        // register a player to the lobby
        avl_insertNode (lobby->players, player);
        lobby->players_fds[lobby->players_nfds].fd = player->client->clientSock;
        lobby->players_nfds++;

        player->inLobby = true;

        return 0;
    }

    return 1;    

}

// FIXME:
// removes a player from the lobby's players vector and sends it to the game server's players list
u8 removePlayerFromLobby (GameServerData *gameData, Vector players, size_t i_player, Player *player) {

    // create a new player and add it to the server's players list
    Player *p = newPlayer (gameData->playersPool, player);
    insertAfter (gameData->players, LIST_END (gameData->players), p);
    p->inLobby = false;

    // delete the player from the lobby
    vector_delete (&players, i_player);

    return 0;

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
            logMsg (stderr, ERROR, SERVER, 
                createString ("Couldn't find a player associated with the passed fd: %i.", fd));
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
            if (player) sendPacket (server, packet, packetSize, player->client->address);
        }

        broadcastToAllPlayers (node->left, server, packet, packetSize);
    }

}

#pragma endregion

/*** LOBBY ***/

/* All the logic that can handle the creation and management of the games goes here! */

#pragma region LOBBY

// FIXME: how do we want to handle game packets from the players???

// FIXME: do we want to compress the poll array?
// handles packets from the players inside a lobby
void handlePlayersInLobby (Server *server, Lobby *lobby) {

    if (!lobby) {
        logMsg (stderr, ERROR, SERVER, "Can't handle packets of a NULL lobby!");
        return 1;
    }

    ssize_t rc;                                  // retval from recv -> size of buffer
    char packetBuffer[MAX_UDP_PACKET_SIZE];      // buffer for data recieved from fd
    PacketInfo *info = NULL;

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

            // recive all incoming data from this socket
            do {
                rc = recv (lobby->players_fds[i].fd, packetBuffer, sizeof (packetBuffer), 0);
                
                // recv error
                if (rc < 0) {
                    if (errno != EWOULDBLOCK) {
                        logMsg (stderr, ERROR, SERVER, "On hold recv failed!");
                        perror ("Error:");
                    }

                    break;  // there is no more data to handle
                }

                if (rc == 0) break;

                // info = newPacketInfo (server, 
                //     getClientBySock (server->onHoldClients, server->hold_fds[i].fd), packetBuffer, rc);

                // FIXME: new game packet info? we need a player structure!!!
                // info = newPacketInfo (server, getpla)

                // FIXME: handle lobby packets
                // we have a dedicated function to authenticate the clients
                // thpool_add_work (thpool, (void *) authenticateClient, info);
            } while (true);
        }

        // FIXME:
        // if (server->compress_hold_clients) compressHoldClients (server);
    }

}

// FIXME: clear the world
// deletes a lobby for ever -- called when we teardown the server
// we do not need to give any feedback to the players if there is any inside
void deleteLobby (void *data) {

    if (!data) return;

    Lobby *lobby = (Lobby *) data;

    lobby->inGame = false;      // just to be sure
    lobby->owner = NULL;

    // check that the lobby is empty
    if (lobby->players.elements > 0) {
        // send the players to the server player list
        Player *tempPlayer = NULL;
        while (lobby->players.elements > 0) {
            tempPlayer = vector_get (&lobby->players, 0);
            // removePlayerFromLobby (gameData, lobby->players, 0, tempPlayer);
            // 26/10/2018 -- just delete the player, it may be a left behind
            deletePlayer (tempPlayer);
        }
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

// FIXME: we need to take out the owner from the clients poll
// handles the creation of a new game lobby, requested by a current registered client -> player
Lobby *newLobby (Server *server, Player *owner, GameType gameType) {

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
        return NULL;
    }

    Lobby *newLobby = NULL;

    GameServerData *data = (GameServerData *) server->serverData;
    if (data->lobbyPool) {
        if (POOL_SIZE (data->lobbyPool) > 0) {
            newLobby = (Lobby *) pop (data->lobbyPool);
            if (!newLobby) newLobby = (Lobby *) malloc (sizeof (Lobby));
        }
    }

    else {
        logMsg (stderr, WARNING, SERVER, "Game server has no refrence to a lobby pool!");
        newLobby = (Lobby *) malloc (sizeof (Lobby));
    }

    newLobby->owner = owner;
    if (newLobby->settings) free (newLobby->settings);
    newLobby->settings = getGameSettings (data->gameSettingsConfig, gameType);
    if (!newLobby->settings) {
        logMsg (stderr, ERROR, GAME, "Failed to get the settings for the new lobby!");
        newLobby->owner = NULL;
        // send the lobby back to the object pool
        pool_push (data->lobbyPool, newLobby);

        // send feedback to the player
        sendErrorPacket (server, owner->client, ERR_SERVER_ERROR, "Game server failed to create new lobby!");

        return NULL;
    } 

    // FIXME: we need to take out the owner from the clients poll
    // init the clients/players structures inside the lobby
    newLobby->players = avl_init (playerComparator, deletePlayer);
    newLobby->players_nfds = 0;
    newLobby->compress_players = false;
    newLobby->pollTimeout = server->pollTimeout;    // inherit the poll timeout from server

    if (!player_registerToLobby (newLobby, owner)) {
        newLobby->inGame = false;

        // TODO: add a timestamp of the creation of the lobby
        // add the lobby the server active ones
        insertAfter (data->currentLobbys, LIST_END (data->currentLobbys), newLobby);

        newLobby->isRunning = true;

        // FIXME: 04/22/2018 -- 15:33 do we create a new thread to sever this socket with its own poll loop?

        return newLobby;
    }

    // TODO: send feedback to the owner -> internal server error
    else {
        logMsg (stderr, ERROR, GAME, "Failed to register the owner of the lobby to the lobby!");
        logMsg (stderr, ERROR, GAME, "Failed to create new lobby!");

        // FIXME: correctly dispose the lobby

        return NULL;
    }

}

// only the owner of the lobby can destroy the lobby
// a lobby should only be destroyed when the owner quits or if we teardown the server
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

    if (lobby->players.elements > 0) {
        // send the players the correct package so they can handle their logic
        size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + lobby->players.elements * sizeof (SPlayer);
        void *lobbyPacket = createLobbyPacket (LOBBY_DESTROY, lobby, packetSize);
        if (!lobbyPacket) {
            logMsg (stderr, ERROR, PACKET, "Failed to create lobby destroy packet!");
            return 1;   // TODO: how to better handle this error?
        }

        Player *tempPlayer = NULL;
        // send the lobby destriy packet to the players
        for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
            tempPlayer = vector_get (&lobby->players, i_player);
            if (tempPlayer) sendPacket (server, lobbyPacket, packetSize, tempPlayer->client->address);
            else logMsg (stderr, ERROR, GAME, "Got a NULL player inside a lobby player's vector!");
        }

        // remove the players from this structure and send them to the server's players list
        while (lobby->players.elements > 0) {
            tempPlayer = vector_get (&lobby->players, 0);
            removePlayerFromLobby (gameData, lobby->players, 0, tempPlayer);
        }
    }

    lobby->owner = NULL;
    if (lobby->settings) free (lobby->settings);

    // we are safe to clear the lobby structure
    if (gameData) {
        // first remove the lobby from the active ones, then send it to the inactive ones
        ListElement *le = getListElement (gameData->currentLobbys, lobby);
        if (le) {
            Lobby *temp = (Lobby *) removeElement (gameData->currentLobbys, le);
            if (temp) push (gameData->lobbyPool, temp);
        }

        else {
            logMsg (stdout, WARNING, GAME, "A lobby wasn't found in the current lobby list.");
            free (lobby);   // destroy the lobby forever
        } 
    }
    
    return 0;   // success

}

// 24/20/2018
// TODO: is sending packets to the players any different if each one has a thread?
    // do we want to send the packets in a separte thread?? think if each client has its own thread

// TODO: add a time stamp when the player joins the lobby

// 24/10/2018 the client is responsible of displaying the correct message depending on the packet we send him

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

    // TODO: send feedback to the player
    if (player->inLobby) {
        #ifdef CERVER_DEBUG
        logMsg (stdout, DEBUG_MSG, PLAYER, "A player is in a lobby and tries to join another one.");
        #endif
        return 1;
    }

    // FIXME:
    // check if for whatever reason a player al ready inside the lobby wants to join...
    Player *tempPlayer = NULL;
    /* for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
        tempPlayer = vector_get (&lobby->players, i_player);
        if (player->id == tempPlayer->id) {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "A player tries to join the same lobby he is in.");
            #endif
            sendErrorPacket (server, player->client, ERR_JOIN_LOBBY, "You can't join the same lobby you are in!");
            return 1;
        }
    } */

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
    tempPlayer = dllist_getPlayerById (gameData->players, player->id);
    if (!tempPlayer) {
        logMsg (stderr, ERROR, GAME, "A player wan't found in the current player -- join lobby");
        return 1;
    }

    // FIXME: be sure to send the correct player ptr
    if (!player_registerToLobby (lobby, tempPlayer)) {
        // sync the in lobby player(s) and the new player
        // generate the new lobby package
        // FIXME: lobby serialized packets
        size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + lobby->players.elements * sizeof (SPlayer);
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

// TODO: same problem as in the join lobby structure --> do we need to change the logic of how we 
// broadcast all the players inside the lobby about the same action? --> how do we keep them in sync?

// FIXME: what happens if the player that left was the owner of the lobby?
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

    // remove the player from the lobby structure -> this stops the lobby from sending & recieving packets
    // to the player
    Player *tempPlayer = NULL;
    bool found = false;
    for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
        tempPlayer = vector_get (&lobby->players, i_player);
        if (player->id == tempPlayer->id) {
            GameServerData *gameData = (GameServerData *) server->serverData;

            removePlayerFromLobby (gameData, lobby->players, i_player, tempPlayer);

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

    // FIXME: what happens if the player that left was the owner of the lobby?

    // broadcast the other players that the player left
    // we need to send an update lobby packet and the clients handle the logic
    size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + lobby->players.elements * sizeof (SPlayer);
    void *lobbyPacket = createLobbyPacket (LOBBY_UPDATE, lobby, packetSize);
    if (!lobbyPacket) {
        logMsg (stderr, ERROR, PACKET, "Failed to create lobby update packet!");
        return 1;
    }

    for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
        tempPlayer = vector_get (&lobby->players, i_player);
        if (tempPlayer) sendPacket (server, lobbyPacket, packetSize, tempPlayer->client->address);
        else logMsg (stderr, ERROR, GAME, "Got a NULL player inside a lobby player's vector!");
    }

    if (lobbyPacket) free (lobbyPacket);

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

// TODO: does this logic goes here on in blackrock???

// TODO: make sure that all the players inside the lobby are in sync before starting the game!!!
// TODO: maybe later we can pass the socket of the server?
// TODO: do we need to pass the lobby struct and settings??
// this is called from by the owner of the lobby
void startGame (Server *server, Lobby *lobby) {

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Starting the game...");
    #endif

    // FIXME: support a retry function -> we don't need to generate this things again!
    // first init the in game structures

    // server side
    // init map
    // init enemies
    // place stairs and other map elements
    initWorld (lobby->world);

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

#pragma region GAME PACKETS

void gameServer_handlePacket (PacketInfo *packet) {

    RequestData *reqData = (RequestData *) (packet->packetData + sizeof (PacketHeader));
    switch (reqData->type) {
        case LOBBY_CREATE: break;
        case LOBBY_JOIN: break;
        case LOBBY_DESTROY: break;
    }

}

#pragma endregion