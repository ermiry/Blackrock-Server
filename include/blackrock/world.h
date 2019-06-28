#ifndef _BLACK_WORLD_H_
#define _BLACK_WORLD_H_

#include "blackrock/map/map.h"

#include "cerver/collections/dllist.h"

typedef struct World {

    Map *game_map;

    DoubleList *players;
    DoubleList *enemies;

} World;

extern World *world;

extern World *world_create (void);
extern void world_destroy (World *world);

#endif