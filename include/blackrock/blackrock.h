#ifndef BLACKROCK_H_
#define BLACKROCK_H_

#include <stdint.h>

/*** BLACKROCK TYPES ***/

#define SCREEN_WIDTH    1280    
#define SCREEN_HEIGHT   720

#define MAP_WIDTH   80
#define MAP_HEIGHT  40

#define NUM_COLS    80
#define NUM_ROWS    45

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int32_t i32;
typedef int64_t i64;

typedef unsigned char asciiChar;

#define THREAD_OK   0

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

    List *enemyData;    // reference to a global list of enemy data

    ScoreBoard *sb;

} BrGameData;

// TODO: maybe add an assist value, just to give some variety
// blackrock keeps track of 3 score types -> kills, deaths, score
#define BR_SCORES_NUM      3

/*** BLACKROCK PUBLIC FUNCTIONS ***/

extern u8 blackrock_init_arcade (void *data);
extern u8 blackrock_loadGameData (void);

/*** BLACKROCK SERIALIZED DATA ***/

#endif