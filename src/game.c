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
    // --> maybe create a different socket for each lobby?

// FIXME: pass the owner of the lobby and the game type!!
// we create the lobby, and we wait until the owner of the lobby tell us to start the game

// TODO: handle different game types

// handles the creation of a new game lobby, requested by a current register client
Lobby *newLobby (Player *owner) {

    logMsg (stdout, GAME, NO_TYPE, "Creatting a new lobby...");

    // TODO: what about a new connection in a new socket??

    // TODO: set the server socket to no blocking and make sure we have a udp connection
    // make sure that we have the correct config for the server in other func
    // FIXME: where do we want to call this?? sock_setNonBlocking (server);

    // TODO: better manage who created the game lobby --> better players/clients management
    // TODO: create the lobby and player owner data structures
    Lobby *newLobby = (Lobby *) malloc (sizeof (Lobby));
    newLobby->owner = owner;
    // as of 02/10/2018 -- we only have one possible game type...
    newLobby->settings = getGameSettings (1);
    if (newLobby->settings == NULL) {
        free (newLobby);
        return NULL;
    } 

    // init the clients/players structures inside the lobby
    vector_init (&newLobby->players, sizeof (Player));
    vector_push (&newLobby->players, owner);

    return newLobby;

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