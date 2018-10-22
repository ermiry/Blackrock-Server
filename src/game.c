/*** Here goes all the logic that makes a Blackrock game server works properly ***/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// #include "cerver.h"
#include "blackrock.h" 
#include "game.h"

#include "utils/config.h"
#include "utils/log.h"

/*** GAME MASTER ***/

/* All the logic that can handle the creation and management of the games goes here! */

#pragma region GAME MASTER

// loads the settings for the selected game type from a cfg file
GameSettings *getGameSettings (u8 gameType) {

    Config *gameConfig = parseConfigFile ("./config/gameSettings.cfg");
    if (!gameConfig) {
        logMsg (stderr, ERROR, GAME, "Problems loading game settings config!");
        return NULL;
    } 

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

// TODO: add support for multiple lobbys at the same time

// TODO: send feedback back to the player on error
// TODO: add object pooling to the lobby
// handles the creation of a new game lobby, requested by a current registered client -> player
Lobby *newLobby (Player *owner, GameType gameType) {

    #ifdef DEBUG
    logMsg (stdout, DEBUG_MSG, GAME, "Creatting a new lobby...");
    #endif

    // check that the owner isn't already in a lobby or game
    if (owner->inLobby) {
        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "A player inside a lobby wanted to create a new lobby.");
        #endif
        // TODO: send feedback to the player
        return NULL;
    }


    // FIXME:!!!!!!!
    // TODO: set the server socket to no blocking and make sure we have a udp connection
    // make sure that we have the correct config for the server in other func
    // FIXME: where do we want to call this?? sock_setNonBlocking (server);


    // TODO: add object pooling to the lobby
    // create the lobby and player owner data structures
    Lobby *newLobby = (Lobby *) malloc (sizeof (Lobby));
    newLobby->owner = owner;
    newLobby->settings = getGameSettings (gameType);
    if (!newLobby->settings) {
        free (newLobby);
        return NULL;
    } 

    // init the clients/players structures inside the lobby
    vector_init (&newLobby->players, sizeof (Player));
    vector_push (&newLobby->players, owner);

    newLobby->inGame = false;
    newLobby->owner->inLobby = true;

    return newLobby;

}

// TODO: maybe add an object pool because we might be creating and destroying many lobby structures??
// TODO: also add object pooling to the clients and players?

// only the owner of the lobby can destroy the lobby
// a lobby should only be destroyed when the owner quits or as a garbage collection and the lobby is empty
// also called if we teardown the server
u8 destroyLobby (Lobby *lobby) {

    if (!lobby) {
        #ifdef DEBUG
        logMsg (stderr, ERROR, GAME, "Can't destroy an empty lobby!");
        #endif
        return 1;
    }

    // TODO: dont forget to player->inLobby = false;

    // the logic is different if we are in game or not
    if (lobby->inGame) {
        // check if we have still players in the lobby

    }

    else {
        // check if we have still players in the lobby

    }

    // TODO: add object pooling here?
    // we are safe to clear the lobby structure
    lobby->owner = NULL;
    if (lobby->settings) free (lobby->settings);
    free (lobby);

    return 0;   // success

}

// TODO: how do we want to manage our current lobbys? --> with a list?

// FIXME: send feedback to the player whatever the output
// called by a registered player that wants to join a lobby on progress
u8 joinLobby (Lobby *lobby, Player *player) {

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
            // TODO: send feedback to the player...
            return 1;
        }
    }

    // check that the player can join the actual game...
    if (lobby->inGame) {
        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "A player tried to join a lobby that is in game.");
        #endif
        // TODO: send feedback to the player...
        return 1;
    }

    if (lobby->players.elements >= lobby->settings->maxPlayers) {
        #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "A player tried to join an already full lobby.");
        #endif
        // TODO: send feedback to the player...
        return 1;
    }

    // FIXME:
    // the player is clear to join the lobby...
    // move the player from the server's players to the in lobby players
    // sync the in lobby player(s) and the new player
    // TODO: send feedback to the players

    player->inLobby = true;

    return 0;   // success

}

// FIXME: send feedback to the player whatever the output
u8 leaveLobby (Lobby *lobby, Player *player) {

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

    // check if the current player is in the correct lobby
    Player *tempPlayer = NULL;
    for (size_t i_player = 0; i_player < lobby->players.elements; i_player++) {
        tempPlayer = vector_get (&lobby->players, i_player);
        if (player->id == tempPlayer->id) break;    // the player is in the lobby
    }

    if (!tempPlayer) {
        #ifdef DEBUG
        logMsg (stderr, ERROR, GAME, "The player doesn't belongs the the lobby!");
        #endif
        return 1;
    }

    // remove the player from the lobby structure -> this stops the lobby from sending & recieving packets
    // to the player
    // broadcast the other players that he left
    // handle if they are in the lobby scene or in current game
        // if the player is the owner -> the lobby gets destroyed...

    player->inLobby = false;

    return 0;   // the player left successfully

}

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