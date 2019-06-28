#ifndef _BLACK_PLAYER_H_
#define _BLACK_PLAYER_H_

#include "cerver/types/types.h"
#include "cerver/collections/dllist.h"

#include "cengine/game/go.h"

#include "blackrock/entities/entity.h"

typedef enum PlayerState {

    PLAYER_IDLE = 0,
    PLAYER_MOVING,
    PLAYER_ATTACK,

} PlayerState;

typedef struct Character {

    LivingEntity *entity;

    u16 money [3];          // gold, silver, copper
    GameObject ***inventory;
    GameObject **weapons;
    GameObject **equipment;

    DoubleList *animations;

} Character;

typedef struct Player {

    u32 goID;

    PlayerState currState;
    // PlayerProfile *profile;
    Character *character;

} Player;

extern void *player_comp_new (u32 goID);
extern void player_comp_delete (void *ptr);

#define MAIN_HAND       0
#define OFF_HAND        1

#define EQUIPMENT_ELEMENTS      10

// head         0
// necklace     1
// shoulders    2
// cape         3
// chest        4

// hands        5
// belt         6
// legs         7
// shoes        8
// ring         9

extern GameObject *player_init (void);
extern void player_update (void *data);

extern GameObject *main_player_go;

#endif