#ifndef _BLACK_ENEMY_H_
#define _BLACK_ENEMY_H_

#include "blackrock.h"

#include "cengine/animation.h"
#include "cengine/collections/dlist.h"

#include "game/game.h"
#include "game/world.h"
#include "game/entities/entity.h"

#include "collections/llist.h"

#define N_MONSTER_TYPES       9

typedef struct EnemyLoot {

    u8 minGold;
    u8 maxGold;
    u32 *drops;
    u8 dropCount;

} EnemyLoot;

#define DB_COL_ENEMY_HEALTH             1
#define DB_COL_ENEMY_STAMINA            2
#define DB_COL_ENEMY_STRENGTH           3

#define DB_COL_ENEMY_ARMOUR             4
#define DB_COL_ENEMY_DODGE              5
#define DB_COL_ENEMY_PARRY              6
#define DB_COL_ENEMY_BLOCK              7

#define DB_COL_ENEMY_HITCHANCE          8
#define DB_COL_ENEMY_DPS                9
#define DB_COL_ENEMY_ATTACK_SPEED       10
#define DB_COL_ENEMY_SPELL_POWER        11
#define DB_COL_ENEMY_CRITICAL           12

// basic enemy data that we only need to load once
typedef struct EnemyData {

    u32 dbId;
    String *name;
    double probability;
    EnemyLoot loot;

    SpriteSheet *sprite_sheet;
    AnimData *anim_data;

} EnemyData;

// basic info of all of oour enemies
extern LList *enemyData;

extern void enemy_data_delete_all (void);

typedef enum EnemyState {

    ENEMY_IDLE = 0,
    ENEMY_MOVING,
    ENEMY_FOLLOWING,
    ENEMY_ATTACK,

} EnemyState;

// component for a game object
typedef struct Enemy {

    u32 goID;

    u32 dbID;
    LivingEntity *entity;
    EnemyState curr_state;
    DoubleList *animations;
    GameObject *target;

} Enemy;

extern u8 enemies_connect_db (void);
extern void enemies_disconnect_db (void);

extern void *enemy_create_comp (u32 goID);
extern void enemy_destroy_comp (void *ptr);

extern void enemy_update (void *data);

extern GameObject *enemy_create (u32 dbID);

extern void enemies_spawn_all (World *world, u8 monNum);

#endif