/*** Here goes all the logic that makes a Blackrock game server works properly ***/

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

/*** PLAYER ***/

#pragma region PLAYER

// TODO: where do we want to put this?
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

    // if we have initialize parameters...
    if (player) {
        new->client = player->client;
        new->alive = player->alive;
        new->inLobby = player->inLobby;
    }

    new->id = nextPlayerId;
    nextPlayerId++;

    return new;

}

#pragma endregion

/*** GAME MASTER ***/

/* All the logic that can handle the creation and management of the games goes here! */

#pragma region GAME MASTER

// FIXME: what is our lobby destroy function? -> must be similar to destroy lobby,
// but we must not send the lobby to the pool but free it

// TODO: create a pool of game lobbys with a couple of lobbys in it
// create a list to manage the server lobbys
void initLobbys (Server *gameServer) {

    if (!gameServer) {
        logMsg (stderr, ERROR, SERVER, "Can't init lobbys in a NULL server!");
        return;
    }

    if (gameServer->type != GAME_SERVER) {
        logMsg (stderr, ERROR, SERVER, "Can't init lobbys in a server of incorrect type!");
        return;
    }

    if (!gameServer->serverData) {
        logMsg (stderr, ERROR, SERVER, "No server data found in the server!");
        return;
    }

    GameServerData *data = (GameServerData *) gameServer->serverData;

    if (data->currentLobbys) logMsg (stdout, WARNING, SERVER, "The server has already a list of lobbys.");
    else data->currentLobbys = initList (free);

    if (data->lobbyPool)  logMsg (stdout, WARNING, SERVER, "The server has already a pool of lobbys.");
    else data->lobbyPool = initPool ();

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

// TODO: do we want to set the sock to nonblocking to handle the game? remember that in space-shooters
// they use udp and they set the socket to non-blocking mode...
// 23/10/2018 -- what about using poll? or select?

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
        if (sendErrorPacket (server, owner->client, ERR_Create_Lobby, "Player is already in a lobby!")) {
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
    newLobby->settings = getGameSettings (data->gameSettingsConfig, gameType);
    if (!newLobby->settings) {
        logMsg (stderr, ERROR, GAME, "Failed to get the settings for the new lobby!");
        newLobby->owner = NULL;
        // send the lobby back to the object pool
        push (data->lobbyPool, newLobby);

        // send feedback to the player
        sendErrorPacket (server, owner->client, ERR_SERVER_ERROR, "Game server failed to create new lobby!");

        return NULL;
    } 

    // init the clients/players structures inside the lobby
    vector_init (&newLobby->players, sizeof (Player));
    vector_push (&newLobby->players, owner);

    newLobby->inGame = false;
    newLobby->owner->inLobby = true;

    // add the lobby the server active ones
    insertAfter (data->currentLobbys, LIST_END (data->currentLobbys), newLobby);

    return newLobby;

}

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

// FIXME:
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

    if (lobby->players.elements > 0) {
        // send the players the correct package so they can handle their logic
        size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + lobby->players.elements * sizeof (SPlayer);
        void *lobbyPacket = createLobbyPacket (LOBBY_DESTROY, lobby, packetSize);
        if (!lobbyPacket) {
            logMsg (stderr, ERROR, PACKET, "Failed to create lobby destroy packet!");
            return 1;   // TODO: how to better handle this error?
        }

        Player *tempPlayer = NULL;
        for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
            tempPlayer = vector_get (&lobby->players, i_player);
            if (tempPlayer) sendPacket (server, lobbyPacket, packetSize, tempPlayer->client->address);
            else logMsg (stderr, ERROR, GAME, "Got a NULL player inside a lobby player's vector!");
        }

        // FIXME: finalize the game
        if (lobby->inGame) {}

        // remove the players from this structure and send them to the server's players list

        // TODO: dont forget to player->inLobby = false;
    }

    lobby->owner = NULL;
    if (lobby->settings) free (lobby->settings);

    // we are safe to clear the lobby structure
    GameServerData *data = (GameServerData *) server->serverData;
    if (data) {
        // first remove the lobby from the active ones, then send it to the inactive ones
        ListElement *le = getListElement (data->currentLobbys, lobby);
        if (le) {
            Lobby *temp = (Lobby *) removeElement (data->currentLobbys, le);
            if (temp) push (data->lobbyPool, temp);
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
    for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
        tempPlayer = vector_get (&lobby->players, i_player);
        if (player->id == tempPlayer->id) {
            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "A player tries to join the same lobby he is in.");
            #endif
            sendErrorPacket (server, player->client, ERR_Join_Lobby, "You can't join the same lobby you are in!");
            return 1;
        }
    }

    // check that the player can join the actual game...
    if (lobby->inGame) {
        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "A player tried to join a lobby that is in game.");
        #endif
        sendErrorPacket (server, player->client, ERR_Join_Lobby, "A game is in progress in the lobby!");
        return 1;
    }

    if (lobby->players.elements >= lobby->settings->maxPlayers) {
        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "A player tried to join an already full lobby.");
        #endif
        sendErrorPacket (server, player->client, ERR_Join_Lobby, "The lobby is already full!");
        return 1;
    }

    // the player is clear to join the lobby...
    // move the player from the server's players to the in lobby players
    GameServerData *gameData = (GameServerData *) server->serverData;
    tempPlayer = dllist_getPlayerById (gameData->players, player->id);
    if (!tempPlayer) {
        logMsg (stderr, ERROR, GAME, "A player wan't found in the current player -- join lobby");
        return 1;
    }

    vector_push (&lobby->players, tempPlayer);      // add the player to the lobby
    player->inLobby = true;     // mark the player as in a lobby

    // sync the in lobby player(s) and the new player
    // we need to send again the lobby packages and mark it as an update
    // TODO: maybe is a good idea to add a time stamp?
    
    // generate the new lobby package
    size_t packetSize = sizeof (PacketHeader) + sizeof (SLobby) + lobby->players.elements * sizeof (SPlayer);
    void *packetBegin = createLobbyPacket (LOBBY_UPDATE, lobby, packetSize);
    if (!packetBegin) {
        logMsg (stderr, ERROR, PACKET, "Failed to create lobby update packet!");
        return 1;
    }

    // send the packet to each player inside the lobby...
    for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
        tempPlayer = vector_get (&lobby->players, i_player);
        if (tempPlayer) sendPacket (server, packetBegin, packetSize, tempPlayer->client->address);
        else logMsg (stderr, ERROR, GAME, "Got a NULL player inside a lobby player's vector!");
    }

    if (packetBegin) free (packetBegin);    // free the lobby packet

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "A new player has joined the lobby");
    #endif

    return 0;   // success

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

// TODO: make sure that all the players inside the lobby are in sync before starting the game!!!
// TODO: maybe later we can pass the socket of the server?
// TODO: do we need to pass the lobby struct and settings??
// this is called from a request from the owner of the lobby
void startGame (void) {

    // TODO: init here other game structures like the map and enemies

    // TODO: we need to send the players a request to init their game, and we need to send
    // them our game settings

    // TODO: make sure that all the players are sync and have inited their own game

    // if all the players are fine, start the game
    // inGame = true;
    // gameLoop ();

}

#pragma endregion

/*** MULTIPLAYER ***/

/* Here goes all the logic that make the in game logic work */

#pragma region MULTIPLAYER GAME



#pragma endregion