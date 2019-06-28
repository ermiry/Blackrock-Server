#ifndef _BLACK_MAP_H_
#define _BLACK_MAP_H_

#include <stdbool.h>

#include "cerver/types/types.h"

#include "cengine/game/go.h"

#include "cerver/collections/llist.h"

typedef struct Coord {

    i32 x;
    i32 y;

} Coord;

typedef struct Segment {

    Coord start, mid, end;
    i32 roomFrom, roomTo;
    bool hasWayPoint;

} Segment;

#define DUNGEON_ROOM_MIN_WIDTH      5
#define DUNGEON_ROOM_MAX_WIDTH      12

#define DUNGEON_ROOM_MIN_HEIGHT     5
#define DUNGEON_ROOM_MAX_HEIGHT     12

#define DUNGEON_FILL_PERCENT        0.45

typedef struct Dungeon {

    u32 width, height;
    bool useRandomSeed;
    u32 seed;
    float fillPercent;
    u8 **map;

} Dungeon;

typedef struct CaveRoom {

    LList *tiles;
    LList *edgeTiles;
    LList *connectedRooms;

    u16 roomSize;
    bool isMainRoom;
    bool isAccessibleFromMain;

} CaveRoom;

typedef struct Cave {

    u32 width, heigth;
    bool useRandomSeed;
    u32 seed;
    u32 fillPercent;
    u8 **map;

} Cave;

typedef struct Map {

    u32 width, heigth;
    GameObject ***go_map;
    Dungeon *dungeon;
    Cave *cave;

} Map;

extern Dungeon *dungeon_generate (Map *map, u32 width, u32 heigth, u32 seed, float fillPercent);
extern void dungeon_destroy (Dungeon *dungeon);

extern Cave *cave_generate (Map *map, u32 width, u32 heigth, u32 seed, u32 fillPercent);
extern void cave_destroy (Cave *cave);

extern Map *map_create (u32 width, u32 height);
extern void map_destroy (Map *map);

extern Coord map_get_free_spot (Map *map);

#endif