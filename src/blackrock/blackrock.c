// Here all goes the independent logic of the Blackrock game!
// This is a sample file of how to use the framework with your own game functions

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sqlite3.h>

#include "cerver/game.h"

#include "utils/thpool.h"
#include "collections/avl.h"

#include "utils/myUtils.h"
#include "collections/dllist.h"
#include "utils/objectPool.h"
#include "utils/config.h"
#include "utils/log.h"

#include "blackrock/blackrock.h"      // blackrock dependent types --> same as in the client
#include "blackrock/map.h"

#pragma region ERRORS 

void *generate_black_error_packet (BlackErrorType type, const char *msg) {

    size_t packetSize = sizeof (PacketHeader) + sizeof (BlackError);
    
    void *begin = generatePacket (APP_ERROR_PACKET, packetSize);
    char *end = begin + sizeof (PacketHeader); 

    BlackError *edata = (BlackError *) end;
    edata->errorType = type;
    if (msg) {
        if (strlen (msg) > sizeof (edata->msg)) {
            // clamp the value to fit inside edata->msg
            u16 i = 0;
            while (i < sizeof (edata->msg) - 1) {
                edata->msg[i] = msg[i];
                i++;
            }

            edata->msg[i] = '\0';
        }

        else strcpy (edata->msg, msg);
    }

    return begin;

}

u8 send_black_error (Server *server, Client *client, u32 sock_fd, 
    BlackError black_error) {

    if (server && client) {
        size_t packet_size = sizeof (PacketHeader) + sizeof (BlackError);
        void *error = generate_black_error_packet (black_error.errorType, black_error.msg);

        if (error) {
            server_sendPacket (server, sock_fd, client->address, error, packet_size);
            free (error);
            return 0;
        }
    }

    return 1;

}

#pragma endregion

#pragma region BLACKROCK SERIALIZATION 

// TODO: how do we want to serialize the friends list?
void black_serialize_player_profile (SPlayerProfile *s_profile, PlayerProfile *profile) {

    if (s_profile && profile) {
        s_profile->profileID = profile->profileID;
        strcpy (s_profile->username, profile->username);
        s_profile->gamesPlayed = profile->gamesPlayed;
        s_profile->kills = profile->kills;
        s_profile->highscore = profile->highscore;
        s_profile->n_friends = profile->n_friends;
    }

}

#pragma endregion

#pragma region BLACKROCK PLAYER DATA

const char *playersDBPath = "./data/players.db";
sqlite3 *playersDB;

// this is only used for development purposes
void createPlayersDB (void) {

    char *err_msg = 0;

    char *sql = "DROP TABLE IF EXISTS Players;"
                "CREATE TABLE Players(Id INT, Username TEXT, Password TEXT,"
                "Kills INT, GamesPlayed INT, HighScore INT,"
                "NoFriends INT, Friends TEXT)";

    if (sqlite3_exec (playersDB, sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf (stderr, "Error! Failed to create Players table!\n");
        fprintf (stderr, "SQL error: %s\n", err_msg);
        sqlite3_free (err_msg);
    }

    else logMsg (stdout, SUCCESS, NO_TYPE, "Created players db table!");

}

// TODO: try loading the db from the backup sever if we don't find it
u8 connectPlayersDB (void) {

    if (sqlite3_open (playersDBPath, &playersDB) != SQLITE_OK) {
        // TODO: try loading the file from a backup
        return 1;
    }

    // createPlayersDB ();

    return 0;

}

// FIXME: we need to store the new value in a file!!!
// FIXME: how do we want to mange unique ids?
u32 player_profile_id = 0;

u8 player_profile_add_to_db (PlayerProfile *player_profile) {

    if (player_profile) {
        if (player_profile->username && player_profile->password) {
            char *err_msg = NULL;
            char *query = NULL;

            player_profile_id += 1;

            // prepare the query
            asprintf (&query, "insert into Players (id, username, password, kills, gamesPlayed, highscore, nofriends, friends)"
                "values ('%d', '%s', '%s', 0, 0, 0, 0, 0);", 
                player_profile_id, player_profile->username, player_profile->password);
            
            // prepare the statement
            sqlite3_stmt *stmt;
            sqlite3_prepare_v2 (playersDB, query, strlen(query), &stmt, NULL);
            
            // execute the statement
            int rc = sqlite3_step (stmt);
            
            if (rc != SQLITE_DONE) {
                logMsg (stderr, ERROR, GAME, "Failed to insert new player into db!");
                fprintf (stderr, "Error: %s\n", sqlite3_errmsg (playersDB));
                free(query);
                return 1;
            }
            
            player_profile->profileID = player_profile_id;
            player_profile->kills = 0;
            player_profile->highscore = 0;
            player_profile->gamesPlayed = 0;
            player_profile->n_friends = 0;
            player_profile->friends = NULL;

            sqlite3_finalize(stmt);
            free(query);

            return 0;
        }
    }

    return 1;

}

u8 player_profile_remove_from_db_by_id (const int id) {

    char *err_msg = NULL;
    char *query = NULL;

    // prepare the query
    asprintf (&query, "delete from Players where id = '%i';", id);
    
    // prepare the statement
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(playersDB, query, strlen(query), &stmt, NULL);
    
    // execute the statement
    int rc = sqlite3_step (stmt);
    
    if (rc != SQLITE_DONE) {
        logMsg (stderr, ERROR, GAME, "Failed to remove new player into db!");
        fprintf (stderr, "Error: %s\n", sqlite3_errmsg (playersDB));
        free(query);
        return 1;
    }
    
    sqlite3_finalize(stmt);
    free(query);

    return 0;

}

PlayerProfile *player_profile_get_from_db_by_id (const int id) {

    // get the db data
    sqlite3_stmt *stmt;
    char *sql = "SELECT * FROM Players WHERE Id = ?";

    if (sqlite3_prepare_v2 (playersDB, sql, -1, &stmt, 0) == SQLITE_OK) 
        sqlite3_bind_int (stmt, 1, id);
    else {
        logMsg (stderr, ERROR, NO_TYPE, "Failed to prepare sqlite stmt!");
        return NULL;
    } 

    sqlite3_step (stmt);

    PlayerProfile *profile = NULL;

    const char *found_username = sqlite3_column_text (stmt, 1);
    if (!found_username) {
        logMsg (stderr, ERROR, GAME, 
            createString ("Failed to get player with id: %i!", id));
        sqlite3_finalize(stmt);
    }

    else {
        profile = (PlayerProfile *) malloc (sizeof (PlayerProfile));
        if (profile) {
            profile->profileID = id;
            profile->username = createString ("%s", sqlite3_column_text (stmt, SQL_USERNAME_COL));
            profile->password = createString ("%s", sqlite3_column_text (stmt, SQL_PASSWORD_COL));
            profile->kills = sqlite3_column_int (stmt, SQL_KILLS_COL);
            profile->gamesPlayed = sqlite3_column_int (stmt, SQL_GAMES_PLAYED_COL);
            profile->highscore = sqlite3_column_int (stmt, SQL_HIGHSCORE_COL);
            profile->n_friends = sqlite3_column_int (stmt, SQL_N_FRIENDS_COL);
            profile->friends = createString ("%s", sqlite3_column_text (stmt, SQL_FRIENDS_COL));
        }
    }

    sqlite3_finalize(stmt);
    return profile;

}

PlayerProfile *player_profile_get_from_db_by_username (const char *username) {

    if (username) {
        char *err_msg = NULL;
        char *query = NULL;

        // prepare the query
        asprintf (&query, "SELECT * FROM Players WHERE Username = '%s'", username);
        
        // prepare the statement
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(playersDB, query, strlen(query), &stmt, NULL);
        
        sqlite3_step (stmt);    // execute the statement

        PlayerProfile *profile = NULL;

        const char *found_username = sqlite3_column_text (stmt, 1);
        if (!found_username) {
            logMsg (stderr, ERROR, GAME, 
                createString ("Failed to get player with username: %s!", username));
            sqlite3_finalize(stmt);
        }

        else {
            profile = (PlayerProfile *) malloc (sizeof (PlayerProfile));
            if (profile) {
                profile->profileID = sqlite3_column_int (stmt, SQL_PROFILE_ID_COL);
                profile->username = (char *) calloc (64, sizeof (char));
                strcpy (profile->username, found_username);
                profile->password = createString ("%s", sqlite3_column_text (stmt, SQL_PASSWORD_COL));
                profile->kills = sqlite3_column_int (stmt, SQL_KILLS_COL);
                profile->gamesPlayed = sqlite3_column_int (stmt, SQL_GAMES_PLAYED_COL);
                profile->highscore = sqlite3_column_int (stmt, SQL_HIGHSCORE_COL);
                profile->n_friends = sqlite3_column_int (stmt, SQL_N_FRIENDS_COL);
                profile->friends = createString ("%s", sqlite3_column_text (stmt, SQL_FRIENDS_COL));
            }
        }
        
        sqlite3_finalize(stmt);
        free(query);

        return profile;
    }

    return NULL;

}

// TODO: 
u8 player_profile_update_password (const char *new_password) {}

// TODO: how do we want to send the friends list?
void player_profile_send (Server *server, Client *client, u32 sock_fd, PlayerProfile *profile) {

    if (profile) {
        size_t packet_size = sizeof (PacketHeader) + sizeof (BlackPacketData) + sizeof (SPlayerProfile);
        void *profile_packet = generatePacket (APP_PACKET, packet_size);

        if (profile_packet) {
            char *end = profile_packet;

            BlackPacketData *black_data = (BlackPacketData *) (end += sizeof (PacketHeader));
            black_data->blackPacketType = PLAYER_PROFILE;

            SPlayerProfile *s_profile = (SPlayerProfile *) (end += sizeof (BlackPacketData));
            black_serialize_player_profile (s_profile, profile);

            server_sendPacket (server, sock_fd, client->address, profile_packet, packet_size);
            free (profile_packet);
        }
    }

}

void *blackrock_check_credentials (BlackCredentials *black_credentials, BlackError *black_error) {

    if (black_credentials) {
        PlayerProfile *search_profile = 
            player_profile_get_from_db_by_username (black_credentials->username);

        // check for a user in our database based on the credentials
        if (black_credentials->login) {
            if (search_profile) {
                if (!strcmp (black_credentials->password, search_profile->password)) 
                    return search_profile;

                else {
                    #ifdef BLACK_DEBUG
                    logMsg (stderr, ERROR, GAME, "Player provided wrong password.");
                    #endif
                    black_error->errorType = BLACK_ERROR_WRONG_CREDENTIALS;
                    strcpy (black_error->msg, "Wrong credentials!");
                }
            }

            else {
                #ifdef BLACK_DEBUG
                logMsg (stderr, ERROR, GAME, "No player profile found with the credentials!");
                #endif
                black_error->errorType = BLACK_ERROR_WRONG_CREDENTIALS;
                strcpy (black_error->msg, "Wrong credentials!");
            }
        }

        // we have a new user, so add it to the database
        else {
            // we al ready have registered that username
            if (search_profile) {
                #ifdef BLACK_DEBUG
                logMsg (stderr, WARNING, GAME, 
                    createString ("Username: %s is already taken!", black_credentials->username));
                #endif
                black_error->errorType = BLACK_ERROR_USERNAME_TAKEN;
                strcpy (black_error->msg, "Username already taken!");
            }

            else {
                // create a new profile and add it to our database
                PlayerProfile *new_profile = (PlayerProfile *) malloc (sizeof (PlayerProfile));
                if (new_profile) {
                    new_profile->username = createString ("%s", black_credentials->username);
                    new_profile->password = createString ("%s", black_credentials->password);

                    if (!player_profile_add_to_db (new_profile)) {
                        #ifdef BLACK_DEBUG
                        logMsg (stdout, DEBUG_MSG, GAME, "Added a new player profile!");
                        #endif
                        return new_profile;
                    }

                    else {
                        #ifdef BLACK_DEBUG
                        logMsg (stderr, ERROR, GAME, "Failed to add a new profile to players db!");
                        #endif
                        black_error->errorType = BLACK_ERROR_SERVER;
                        strcpy (black_error->msg, "Internal server error!");
                    }
                }
            }
        }
    }

    return NULL;

}

u8 blackrock_authMethod (void *data) {

    if (data) {
        PacketInfo *pack_info = (PacketInfo *) data;

        // check if the server supports sessions
        if (pack_info->server->useSessions) {
            // check if we have recieved a token or auth credentials
            bool isToken = false;
            if (pack_info->packetSize < (sizeof (PacketHeader) + sizeof (RequestData) + 
                sizeof (BlackCredentials)))
                isToken = true;

            if (isToken) {
                char *end = pack_info->packetData;
                Token *tokenData = (Token *) (end + sizeof (PacketHeader) + sizeof (RequestData));

                // verify the token and search for a client with that session id
                Client *c = getClientBySession (pack_info->server->clients, tokenData->token);
                if (c) {
                    client_set_sessionID (pack_info->client, tokenData->token);
                    return 0;
                }

                else {
                    #ifdef CERVER_DEBUG
                    logMsg (stderr, ERROR, CLIENT, "Wrong session id provided by client!");
                    #endif
                    return 1;      // the session id is wrong -> error!
                }
            }

            // we have recieved blackrock credentials, so validate them
            else {
                char *end = pack_info->packetData;
                BlackCredentials *black_credentials = (BlackCredentials *) (end + sizeof (PacketHeader) +
                    sizeof (RequestData));

                BlackError black_error;
                PlayerProfile *player_profile = 
                    blackrock_check_credentials (black_credentials, &black_error);

                if (player_profile) {
                    // TODO: generate a better session id
                    char *sessionID = session_default_generate_id (pack_info->clientSock,
                        pack_info->client->address);

                    if (sessionID) {
                        #ifdef CERVER_DEBUG
                        logMsg (stdout, DEBUG_MSG, CLIENT, 
                            createString ("Generated client session id: %s", sessionID));
                        #endif

                        client_set_sessionID (pack_info->client, sessionID);

                        // send back the player profile
                        player_profile_send (pack_info->server, pack_info->client, pack_info->clientSock,
                            player_profile);

                        return 0;
                    }

                    else {
                        logMsg (stderr, ERROR, CLIENT, "Failed to generate session id!");
                        black_error.errorType = BLACK_ERROR_SERVER;
                        strcpy (black_error.msg, "Failed to generate session id!");
                        send_black_error (pack_info->server, pack_info->client, pack_info->clientSock, 
                            black_error);
                    } 
                }

                // wrong credentials -- server automatically sends an error packet
                else {
                    #ifdef BLACK_DEBUG
                        logMsg (stdout, ERROR, GAME, "Blackrock player failed to authenticate!");
                    #endif
                    send_black_error (pack_info->server, pack_info->client, pack_info->clientSock, 
                        black_error);
                }
            }
        }

        // if not, just check for client credentials
        else {
            char *end = pack_info->packetData;
            BlackCredentials *black_credentials = (BlackCredentials *) (end + sizeof (PacketHeader) +
                sizeof (RequestData));

            // FIXME: verify the credentials
            // FIXME: check if they are signing in or they are a new user

            bool success_auth = true;

            if (success_auth) return 0;
            else return 1;
        }
    }

    return 1;

}

// TODO: add functions to update our game stats
// TODO: add functions to add and remove friends from our list
// TODO: how do we want to manage friends requests?

#pragma endregion

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

typedef struct {

    u32 id;
    char *name;
    double probability;
    MonsterLoot loot;

} Monster;

#define MONSTER_COUNT       9

DoubleList *enemyData = NULL;

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

    dlist_insert_after (enemyData, LIST_END (enemyData), mon);

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
    /* enemyData = dlist_init (free);

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

    // connect to the players db
    if (!connectPlayersDB ()) {
        #ifdef DEBUG
            logMsg (stdout, DEBUG_MSG, GAME, "Connedted to the players db!");
        #endif
    }

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

// FIXME:!!!
u8 blackrock_deleteGameData (void) {

}

// TODO: where do we want to keep track of player positions?
// TODO: how do we want to keep track of scores?
    // maybe a dictionary like quill18 or probably just an entry iniside each player?

World *newWolrd (void) {

    World *world = (World *) malloc (sizeof (World));
    if (world) {
        world->level = NULL;
    }

    return world;

}

// TODO: we will only have an enemy data, so do we just have this pointing to that list?
// does each world needs to have its own enemy data structure?
BrGameData *newBrGameData (void) {

    BrGameData *brdata = (BrGameData *) malloc (sizeof (BrGameData));
    if (brdata) {
        brdata->world = newWolrd ();
        brdata->enemyData = NULL;
        brdata->sb = NULL;
    }

    return brdata;

}

// FIXME:
void deleteBrGameData (void *data) {

    if (data) {

    }

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

// init game data structures 
u8 initGameData (BrGameData *brdata, Lobby *lobby) {

    if (brdata) {
        if (enemyData) brdata->enemyData = enemyData;
        else {
            logMsg (stdout, WARNING, GAME, "NULL reference to enemy data list. Creating a new one.");
            // TODO: init enmy data list
        }

        // add players to the scoreboard
        if (!brdata->sb) brdata->sb = game_score_new (lobby->players_nfds, 
            BR_SCORES_NUM, "kills", "deaths", "score");
        
        // FIXME:
        // this already sets the initial scores to 0
        game_score_add_lobby_players (brdata->sb, lobby->players->root);
    }

    else {
        logMsg (stderr, ERROR, GAME, "Can't init a NULL BR game data!");
        return 1;
    }

}

// FIXME:
void generateLevel (World *world) {

    // FIXME: check if we have to actually create the walls or the server
    // only needs to know about their position
    // randomly generate the map data
    // initMap (world->level->mapCells);

    // TODO: generate other map structures such as stairs
    // FIXME: place stairs

    // generate the monsters to spawn
    u8 monToSpawn = 15;
    u8 monCount = 0;
    for (u8 i = 0; i < monToSpawn; i++) {
        // generate a random monster
        // FIXME:
        // GameObject *monster = createMonster (getMonsterId ());
        /* if (monster) {
            // spawn in a random position
            Point monsterSpawnPos = getFreeSpot (world->level->mapCells);

            // FIXME: will we have the same ecs here in the server?
            // Position *monsterPos = (Position *) getComponent (monster, POSITION);
            // monsterPos->x = (u8) monsterSpawnPos.x;
            // monsterPos->y = (u8) monsterSpawnPos.y;
            monCount++;
        } */
    }

    #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, 
        createString ("%i / %i monsters created successfully.", monCount, monCount));
    #endif

}

// 15/11/2018 -- we spawn the players in the same position
void spawnPlayers (AVLNode *node, Point playerSpawnPos) {

     if (node) {
        spawnPlayers (node->right, playerSpawnPos);

        // send packet to curent player
        if (node->id) {
            Player *player = (Player *) node->id;
            if (player) {
                // FIXME: set the position!!

                // Position *monsterPos = (Position *) getComponent (monster, POSITION);
                // monsterPos->x = (u8) monsterSpawnPos.x;
                // monsterPos->y = (u8) monsterSpawnPos.y;
            }
        }

        spawnPlayers (node->left, playerSpawnPos);
    }

}

// inits the world data inside the lobby
u8 initWorld (AVLNode *players, World *world) {

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

    // spawn players -> just mark their starting position
    // 15/11/2018 -- we spawn the players in the same position
    // Point playerSpawnPos = getFreeSpot (world->level->mapCells);
    // spawnPlayers (players, playerSpawnPos);

    // TODO: calculate fov

    return 0;
    
}

// the game admin should set this function to init the desired game type
// 14/11/2018 - we make use of the cerver framework to create the multiplayer for our game
u8 blackrock_init_arcade (void *data) {

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

    if (!initGameData (brdata, sl->lobby)) {
        if (!initWorld (sl->lobby->players->root, brdata->world)) {
            // we need to sync all of the players -> send them the game data from which
            // they should generate their own game
            // FIXME: send all the game data 
            size_t packetSize = sizeof (PacketHeader) + sizeof (RequestData);
            void *initPacket = generatePacket (GAME_PACKET, packetSize);
            char *end = initPacket + sizeof (PacketHeader); 
            RequestData *reqdata = (RequestData *) end;
            reqdata->type = GAME_INIT;

            broadcastToAllPlayers (sl->lobby->players->root, sl->server, initPacket, packetSize);

            // after we have sent the game data, we need to wait until all the
            // players have generated their own levels and we need to be sure
            // they are on sync
            // FIXME: be sure that all the players are on the same page

            // if all goes well, we can now start the new game, so we signal the players
            // to start their game and start sending and recieving packets
            void blackrock_update_arcade (void *data);
            thpool_add_work (sl->server->thpool, (void *) blackrock_update_arcade, brdata);

            return 0;   // we return and the new game should start in a separte thread
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

// TODO: 
// we don't need to init all the structures like in the start game
u8 blackrock_retry_arcade (void *data) {}

#pragma endregion

/*** RUNNING GAME ***/

#pragma region BLACKROCK GAME

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

// FIXME:
void blackrock_update_arcade (void *data) {

    // FIXME:
    // send a packet to the players to init their game
    // so that they can start sending and recieving packets!

    // FIXME: send messages to the client log
    // TODO: add different texts here!!
    // TODO: this should go inside the packet above
    // logMessage ("You have entered the dungeon!", 0xFFFFFFFF);

    #ifdef DEBUG
        logMsg (stdout, DEBUG_MSG, GAME, "Players have entered the dungeon!");
    #endif

    // TODO: start the game -> this should be similar to the update game in blackrcok client
    // start calculating pathfinding
    // sync fov of all players
    // sync player data like health
    // keep track of players score
    // sync players movement

}

#pragma endregion