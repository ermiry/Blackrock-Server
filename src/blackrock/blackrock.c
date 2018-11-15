// Here all goes the independent logic of the Blackrock game!
// This is a sample file of how to use the framework with your own game functions

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sqlite3.h>

#include "game.h"           // game server

#include "utils/myUtils.h"
#include "utils/list.h"
#include "utils/objectPool.h"
#include "utils/config.h"
#include "utils/log.h"

#include "blackrock/blackrock.h"      // blackrock dependent types --> same as in the client
#include "blackrock/map.h"

#pragma region BLACKROCK INIT

const char *itemsDBPath = "./data/items.db";
sqlite3 *itemsDB;

/*** ENEMIES ***/

const char *enemiesDBPath = "./data/enemies.db";
sqlite3 *enemiesDB;

typedef struct {

    u8 minGold;
    u8 maxGold;
    u32 *drops;
    u8 dropCount;

} MonsterLoot;

// 04/09/2018 -- 12:18
typedef struct {

    u32 id;
    char *name;
    double probability;
    MonsterLoot loot;

} Monster;

#define MONSTER_COUNT       9

List *enemyData = NULL;

u8 loadMonsterLoot (u32 monId, MonsterLoot *loot) {

    // get the db data
    sqlite3_stmt *res;
    char *sql = "SELECT * FROM Drops WHERE Id = ?";

    if (sqlite3_prepare_v2 (enemiesDB, sql, -1, &res, 0) == SQLITE_OK) sqlite3_bind_int (res, 1, monId);
    else {
        logMsg (stderr, ERROR, GAME, 
            createString ("Failed to execute db statement: %s", sqlite3_errmsg (enemiesDB)));
        return 1;
    } 

    int step = sqlite3_step (res);

    loot->minGold = (u32) sqlite3_column_int (res, 1);
    loot->maxGold = (u32) sqlite3_column_int (res, 2);

    // get the possible drop items
    const char *c = sqlite3_column_text (res, 3);
    if (c != NULL) {
        char *itemsTxt = (char *) calloc (strlen (c) + 1, sizeof (char));
        strcpy (itemsTxt, c);

        // parse the string by commas
        char **tokens = splitString (itemsTxt, ',');
        if (tokens) {
            // count how many elements will be extracted
            u32 count = 0;
            while (*itemsTxt++) if (',' == *itemsTxt) count++;

            count += 1;     // take into account the last item after the last comma

            loot->dropCount = count;
            loot->drops = (u32 *) calloc (count, sizeof (u32));
            u32 idx = 0;
            for (u32 i = 0; *(tokens + i); i++) {
                loot->drops[idx] = atoi (*(tokens + i));
                idx++;
                free (*(tokens + i));
            }

            free (tokens);
        }

        // no drop items in the db or an error somewhere retrieving the data
        // so only hanlde the loot with the money
        else {
            loot->drops = NULL;
            loot->dropCount = 0;
        } 
    }
    
    else {
        loot->drops = NULL;
        loot->dropCount = 0;
    } 

    sqlite3_finalize (res);

    return 0;

}

// Getting the enemies data from the db into memory
static int loadEnemyData (void *data, int argc, char **argv, char **azColName) {
   
    Monster *mon = (Monster *) malloc (sizeof (Monster));

    mon->id = atoi (argv[0]);
    char temp[20];
    strcpy (temp, argv[1]);
    mon->name = (char *) calloc (strlen (temp) + 1, sizeof (char));
    strcpy (mon->name, temp);
    mon->probability = atof (argv[2]);

    if (loadMonsterLoot (mon->id, &mon->loot))
        logMsg (stderr, ERROR, GAME, 
            createString ("Error getting monster loot. Monster id: %i.\n", mon->id));

    insertAfter (enemyData, LIST_END (enemyData), mon);

   return 0;

}

// FIXME:
// TODO: try loading the db from the backup sever if we don't find it
u8 connectEnemiesDB (void) {

    if (sqlite3_open (enemiesDBPath, &enemiesDB) != SQLITE_OK) {
        logMsg (stderr, ERROR, GAME, "Failed to open the enemies db!");
        // TODO: try loading the file from a backup
        return 1;
    } 

    // createEnemiesDb ();

    // FIXME: does each lobby need to have its own enemy data?
    // TODO: create a destroy function for this
    // enemies in memory
    /* enemyData = initList (free);

    // load the enemies data into memory
    char *err = 0;
    char *sql = "SELECT * FROM Monsters";
          
    if (sqlite3_exec (enemiesDB, sql, loadEnemyData, NULL, &err) != SQLITE_OK ) {
        logMsg (stderr, ERROR, GAME, createString ("SQL error: %s\n", err));
        sqlite3_free (err);
    } */

    return 0;
    
}

// TODO: how do we want to handle multiple game servers??
    // they need to access the same info but we will not want the same copies 
    // of this memory right?

// TODO: how do we better handle this error?
        // -> maybe we can have a backup where we can get the file

// TODO: don't forget to add in the documentation that this needs to return 0 on succes, otherwise,
// we will handle it as an error!
// we connect to our dbs and get any other data that we need
u8 blackrock_loadGameData (void) {

    // connect to the items db
    if (sqlite3_open (itemsDBPath, &itemsDB) != SQLITE_OK) {
        logMsg (stderr, ERROR, GAME, "Failed to open the items db!");
        return 1;
    }

    else {
        #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "Connected to items db.");
        #endif
    }

    // get enemy info from enemies db    
    if (!connectEnemiesDB ()) {
        #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "Done loading enemies data.");
        #endif
    }

    else {
        logMsg (stderr, ERROR, GAME, "Failed to load enemies data!");
        return 1;
    }

    #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "Done loading enemy data from db.");
    #endif

    return 0;

}

#pragma endregion

#pragma region BLACKROCK GAME

// TODO: where do we want to keep track of player positions?
// TODO: how do we want to keep track of scores?
    // maybe a dictionary like quill18 or probably just an entry iniside each player?

typedef struct BrGameData {

    World *world;
    List *enemyData;

    // TODO: scores

} BrGameData;

World *newWolrd (void) {

    World *world = (World *) malloc (sizeof (World));
    if (world) {
        world->level = NULL;
    }

    return world;

}

// FIXME: pass an enemy data destroy function
// does each world needs to have its own enemy data structure?
BrGameData *newBrGameData (void) {

    BrGameData *brdata = (BrGameData *) malloc (sizeof (BrGameData));
    if (brdata) {
        brdata->world = newWolrd ();
        brdata->enemyData = initList (free);
    }

    return brdata;

}

/****
 * 15/11/2018 - what does the server needs to keep track of?
 * 
 * - player positions
 * - players stats
 * - players scores
 * - enemy positions
 * - enemy stats
 * - loot generation
 * - track items positions
 * ***/

// FIXME: init game data structures 
u8 initGameData () {

    // init game data 
    // init structures of what we need to keep track of

}

// FIXME:
void generateLevel (World *world) {

    // FIXME: check if we have to actually create the walls or the server
    // only needs to know about their position
    // randomly generate the map data
    initMap (world->level->mapCells);

    // TODO: generate other map structures such as stairs

    // generate the monsters to spawn
    // FIXME: how do we manage how many monsters to spawn?
    u8 monCount = 15;
    u8 count;
    for (u8 i = 0; i < monCount; i++) {
        // FIXME: genetare a random monster --> what data does the server needs to know?
            // position, health (combat), movement (ai), physics?
        
        // FIXME: place the monster in a random position in the map
    }

    #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, 
        createString ("%i / %i monsters created successfully.", count, monCount));
    #endif

}

// FIXME:
// inits the world data inside the lobby
u8 initWorld (World *world) {

    // this must be called once -> the first time we enter the map
    if (!world->level) {
        world->level = (Level *) malloc (sizeof (Level));
        world->level->levelNum = 1;
        world->level->mapCells = (bool **) calloc (MAP_WIDTH, sizeof (bool *));
        for (u8 i = 0; i < MAP_WIDTH; i++)
            world->level->mapCells[i] = (bool *) calloc (MAP_HEIGHT, sizeof (bool));

    } 

    // generate a random world froms scratch
    // TODO: maybe later we want to specify some parameters based on difficulty?
    // or based on the type of terrain that we want to generate.. we don't want to have the same algorithms
    // to generate rooms and for generating caves or open fields

    // create physical level and level elements, also add monsters
    generateLevel (world);

    // TODO: spawn players
    // place each player in a near spot to each other

    // TODO: calculate fov

    return 0;
    
}

// FIXME: support a retry function -> we don't need to generate this things again!
    // first init the in game structures

// TODO: make sure that all the players inside the lobby are in sync before starting the game!!!
// this is called from by the owner of the lobby

// the game admin should set this function to init the desired game type
// 14/11/2018 - we make use of the cerver framework to create the multiplayer for our game
u8 blackrock_start_arcade (void *data) {

    if (!data) {
        logMsg (stderr, ERROR, GAME, "No sl data recieved! Failed to init a new game.");
        return 1;
    }

    // we need this to send our game packets to the lobby players
    ServerLobby *sl = (ServerLobby *) data;

    // init our own game data inside the cerver lobby
    BrGameData *brdata = newBrGameData ();
    if (!brdata) {
        logMsg (stderr, ERROR, GAME, "Failed to create new Blackrock game data!");
        return 1;
    }

    sl->lobby->gameData = brdata;

    // server side
    // init map
    // init enemies
    // place stairs and other map elements
    // initWorld (lobby->world);

    if (!initGameData ()) {
        if (!initWorld (brdata->world)) {
            // FIXME: before this, when the owner started the game, 
            // we need to sync all of the players and they should be waiting
            // for our game init packets

            // the server data structures have been generated,
            // now we can send the players our world data so that they
            // can generate their own world

            // after we have sent the game data, we need to wait until all the
            // players have generated their own levels and we need to be sure
            // they are on sync

            // FIXME: send messages to the client log
            // TODO: add different texts here!!
            // logMessage ("You have entered the dungeon!", 0xFFFFFFFF);

            // if all goes well, we can now start the new game, so we signal the players
            // to start their game and start sending and recieving packets

            #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "Players have entered the dungeon!");
            #endif

            // init the game
            // start calculating pathfinding
            // sync fov of all players
            // sync player data like health
            // keep track of players score
            // sync players movement

            // FIXME: we need to return 0 to mark as a success!!
        }

        else {
            logMsg (stderr, ERROR, GAME, "Failed to init BR world!");
            return 1;
        }
    }

    else {
        logMsg (stderr, ERROR, GAME, "Failed to init BR game data!");
        return 1;
    }

}

#pragma endregion