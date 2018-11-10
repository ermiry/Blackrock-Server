#ifndef MAP_H_
#define MAP_H_

#include "blackrock.h"

typedef struct Point {

    i32 x, y;

}  Point;

typedef struct Segment {

    Point start, mid, end;
    i32 roomFrom, roomTo;
    bool hasWayPoint;

} Segment;

#define MAX_WALLS   2000

typedef struct Wall {

    u32 x, y; // position

    bool hasBeenSeen;

    bool blocksMovement;
    bool blocksSight;

} Wall;

extern Wall walls[MAX_WALLS];

extern void initMap (bool **mapCells);
extern Point getFreeSpot (bool **mapCells);

#endif