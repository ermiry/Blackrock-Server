#ifndef BLACKROCK_H_
#define BLACKROCK_H_

#include <stdint.h>

#include "cerver/types/types.h"
#include "cerver/game/game.h"
#include "cerver/game/score.h"
#include "cerver/collections/dllist.h"

/*** BLACKROCK TYPES ***/

#define SCREEN_WIDTH    1280    
#define SCREEN_HEIGHT   720

#define MAP_WIDTH   80
#define MAP_HEIGHT  40

#define NUM_COLS    80
#define NUM_ROWS    45

typedef unsigned char asciiChar;

#define THREAD_OK   0

/*** BLACKROCK ERROS ***/

typedef enum BlackErrorType {

    BLACK_ERROR_SERVER = 0,

    BLACK_ERROR_WRONG_CREDENTIALS,
    BLACK_ERROR_USERNAME_TAKEN,

} BlackErrorType;

typedef struct BlackError {

    BlackErrorType errorType;
    char msg[128];

} BlackError;

/*** GAME OBJECTS ***/

#include "blackrock/map.h"

#define COMP_COUNT      7

typedef enum GameComponent {

    POSITION = 0,
    GRAPHICS,
    PHYSICS,
    MOVEMENT,
    COMBAT,
    EVENT,
    LOOT

} GameComponent;

typedef struct GameObject {
    
    u32 id;
    u32 dbId;
    void *components[COMP_COUNT];

} GameObject;

typedef struct Position {

    u32 objectId;
    u8 x, y;
    u8 layer;   

} Position;

typedef struct Physics {

    u32 objectId;
    bool blocksMovement;
    bool blocksSight;

} Physics;

typedef struct Movement {

    u32 objectId;
    u32 speed;
    u32 frecuency;
    u32 ticksUntilNextMov;
    Point destination;
    bool hasDestination;
    bool chasingPlayer;
    u32 turnsSincePlayerSeen;

} Movement;

// This are the general stats for every living entity
typedef struct Stats {

    u32 maxHealth;      // base health
    i32 health;
    u32 power;          // this represents the mana or whatever
    u32 powerRegen;     // regen power/(ticks or turns)
    u32 strength;       // this modifies the damage dealt 

} Stats;

typedef struct Attack {

    u32 hitchance;          // chance to not miss the target
    u32 baseDps;            // this is mostly for npcs
    u32 attackSpeed;        // how many hits per turn
    u32 spellPower;         // similar to attack power but for mages, etc
    u32 criticalStrike;     // chance to hit a critical (2x more powerful than normal)

} Attack;

typedef struct Defense {

    u32 armor;  // based on level, class, and equipment
    u32 dodge;  // dodge chance -> everyone can dodge
    u32 parry;  // parry chance -> only works with certain weapons and classes
    u32 block;  // block chance -> this only works with a certain class than can handle shields

} Defense;

typedef struct Combat  {

	u32 objectId;	
    Stats baseStats;	
    Attack attack;
    Defense defense;

} Combat;

/*** Players ***/

#define SQL_PROFILE_ID_COL          0
#define SQL_USERNAME_COL            1
#define SQL_PASSWORD_COL            2
#define SQL_KILLS_COL               3
#define SQL_GAMES_PLAYED_COL        4
#define SQL_HIGHSCORE_COL           5
#define SQL_N_FRIENDS_COL           6
#define SQL_FRIENDS_COL             7

typedef struct PlayerProfile {

    u32 profileID;
    char *username;
    char *password;

    u32 kills;
    u32 gamesPlayed;
    u32 highscore;

    u32 n_friends;
    char *friends;
    // char *clan;

} PlayerProfile;

/*** GAME DATA ***/

typedef struct Level {

    u8 levelNum;
    bool **mapCells;    // dungeon map

} Level;

// TODO: maybe game objects such as enemies with positions and items 
// go inside here?
typedef struct World {

	Level *level;

} World;

typedef struct BrGameData {

    World *world;

    DoubleList *enemyData;    // reference to a global list of enemy data

    ScoreBoard *sb;

} BrGameData;

// TODO: maybe add an assist value, just to give some variety
// blackrock keeps track of 3 score types -> kills, deaths, score
#define BR_SCORES_NUM      3

/*** BLACKROCK PUBLIC FUNCTIONS ***/

extern u8 blackrock_loadGameData (void);
extern u8 blackrock_deleteGameData (void);

extern u8 blackrock_init_arcade (void *data);

extern void deleteBrGameData (void *data);

extern u8 blackrock_authMethod (void *data);

/*** BLACKROCK PACKETS ***/

typedef enum BlackPacketType {

    PLAYER_PROFILE,

} BlackPacketType;

typedef struct BlackPacketData {

    BlackPacketType blackPacketType;

} BlackPacketData;

/*** BLACKROCK SERIALIZED DATA ***/

typedef struct BlackCredentials {

    bool login;
    char username[64];
    char password[64];

} BlackCredentials;

typedef struct SPlayerProfile {

    u32 profileID;
    char username[64];

    u32 kills;
    u32 gamesPlayed;
    u32 highscore;

    u32 n_friends;
    // char friends[64];
    // char clan[64];

} SPlayerProfile;

#endif