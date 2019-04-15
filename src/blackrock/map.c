/*** MAP ***/

// This code handles the algorithms for generating random map levels and nothing more
// we will eventually handle the creation of random dungeons, caves and forests and maybe other scenes

// 25/10/2018 -- this is the same code as in the Blackrock client

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "blackrock/blackrock.h"
#include "blackrock/room.h"
#include "blackrock/map.h"

#include "collections/dllist.h"

#include "utils/myUtils.h"


/*** OTHER ***/

Point getFreeSpot (bool **mapCells) {

    Point freeSpot;

    for (;;) {
        u32 freeX = (u32) randomInt (0, MAP_WIDTH - 1);
        u32 freeY = (u32) randomInt (0, MAP_HEIGHT - 1);

        if (mapCells[freeX][freeY] == false) {
            freeSpot.x = freeX;
            freeSpot.y = freeY;
            break;
        }
    }

    return freeSpot;

}

Point randomRoomPoint (Room *room) {

    u32 px = (u32) (rand () % (room->w - 1)) + room->x;
    u32 py = (u32) (rand () % (room->h - 1)) + room->y;

    Point randPoint = { px, py };
    return randPoint;

}

i32 roomWithPoint (Point pt, Room *first) {

    i32 retVal = 0;
    Room *ptr = first;
    // if (ptr == NULL) fprintf (stderr, "\nPassing a NULL rooms list!\n");
    while (ptr != NULL) {
        if ((ptr->x <= pt.x) && ((ptr->x + ptr->w) > pt.x) &&
        (ptr->y <= pt.y) && ((ptr->y + ptr->h) > pt.y)) 
            return retVal;            

        ptr = ptr->next;
        retVal++;
    }

    return -1;

}

/*** CARVING ***/

bool carveRoom (u32 x, u32 y, u32 w, u32 h, bool **mapCells) {

    for (u8 i = x - 1; i < x + (w + 1); i++) 
        for (u8 j = y - 1; j < y + (h + 1); j++)
            if (mapCells[i][j] == false) return false;

    // carve the room
    for (u8 i = x; i < x + w; i++) 
        for (u8 j = y; j < y + h; j++)
            mapCells[i][j] = false;

    return true;

}

void carveCorridorHor (Point from, Point to, bool **mapCells) {

    u32 first, last;
    if (from.x < to.x) {
        first = from.x;
        last = to.x;
    }

    else {
        first = to.x;
        last = from.x;
    }

    for (u32 x = first; x <= last; x++) 
        mapCells[x][from.y] = false;

}

void carveCorridorVer (Point from, Point to, bool **mapCells) {

    u32 first, last;
    if (from.y < to.y) {
        first = from.y;
        last = to.y;
    }

    else {
        first = to.y;
        last = from.y;
    }

    for (u32 y = first; y <= last; y++)
        mapCells[from.x][y] = false;

}

/*** SEGMENTS ***/ 

void getSegments (DoubleList *segments, Point from, Point to, Room *firstRoom) {

    // if (firstRoom == NULL) fprintf (stderr, "\nPassing a NULL room list!\n");

    bool usingWayPoint = false;
    Point waypoint = to;
    if (from.x != to.x && from.y != to.y) {
        usingWayPoint = true;
        if (rand() % 2 == 0) {
            waypoint.x = to.x;
            waypoint.y = from.y;
        }

        else {
            waypoint.x = from.x;
            waypoint.y = to.y;
        }
    }

    Point curr = from;
    bool horizontal = false;
    i8 step = 1;
    if (from.y == waypoint.y) {
        horizontal = true;
        if (from.x > waypoint.x) step = -1;
    }

    else if (from.y > waypoint.y) step = -1;

    i32 currRoom = roomWithPoint (curr, firstRoom);
    Point lastPoint = from;
    bool done = false;
    Segment *turnSegment = NULL;
    while (!done) {
        i32 rm = roomWithPoint (curr, firstRoom);
        if (usingWayPoint && curr.x == waypoint.x && curr.y == waypoint.y) {
            // check if we are in a room
            if (rm != -1) {
                if (rm != currRoom) {
                    // we have a new segment between currRoom and rm
                    Segment *s = (Segment *) malloc (sizeof (Segment));
                    s->start = lastPoint;
                    s->end = curr;
                    s->roomFrom = currRoom;
                    s->roomTo = rm;
                    s->hasWayPoint = false;
                    insertAfter (segments, NULL, s);
                    currRoom = rm;
                }

                else lastPoint = waypoint;
            }

            else {
                turnSegment = (Segment *) malloc (sizeof (Segment));
                turnSegment->start = lastPoint;
                turnSegment->mid = curr;
                turnSegment->hasWayPoint = true;
                turnSegment->roomFrom = currRoom;
            }

            from = curr;
            horizontal = false;
            step = 1;
            if (from.y == to.y) {
                horizontal = true;
                if (from.x > to.x) step = -1;
            }

            else if (from.y > to.y) step = -1;

            if (horizontal) curr.x += step;
            else curr.y += step;
        }

        else if (curr.x == to.x && curr.y == to.y) {
            // we have hit our end point... 
            // check if we are inside another room or in the same one
            if (rm != currRoom) {
                if (turnSegment != NULL) {
                    // we already have a partial segment, so now complete it
                    turnSegment->end = curr;
                    turnSegment->roomTo = rm;
                    insertAfter (segments, NULL, turnSegment);
                    turnSegment = NULL;
                }

                else {
                    // we have a new segment between currRoom and rm
                    Segment *s = (Segment *) malloc (sizeof (Segment));
                    s->start = lastPoint;
                    s->end = curr;
                    s->roomFrom = currRoom;
                    s->roomTo = rm;
                    s->hasWayPoint = false;
                    insertAfter (segments, NULL, s);
                }
            }

            done = true;
        }

        else {
            if (rm != -1 && rm != currRoom) {
                if (turnSegment != NULL) {
                    // complete the partial segment
                    turnSegment->end = curr;
                    turnSegment->roomTo = rm;
                    insertAfter (segments, NULL, turnSegment);
                    turnSegment = NULL;
                }

                else {
                    // we have a new segment 
                    Segment *s = (Segment *) malloc (sizeof (Segment));
                    s->start = lastPoint;
                    s->end = curr;
                    s->roomFrom = currRoom;
                    s->roomTo = rm;
                    s->hasWayPoint = false;
                    insertAfter (segments, NULL, s);
                }

                currRoom = rm;
                lastPoint = curr;
            }

            if (horizontal) curr.x += step;
            else curr.y += step;
        }
    }

}

void carveSegments (DoubleList *hallways, bool **mapCells) {

    ListElement *ptr = LIST_START (hallways);
    while (ptr != NULL) {
        Segment *seg = (Segment *) ptr->data;

        if (seg->hasWayPoint) {
            Point p1 = seg->start;
            Point p2 = seg->mid;

            if (p1.x == p2.x) carveCorridorVer (p1, p2, mapCells);
            else carveCorridorHor (p1, p2, mapCells);

            p1 = seg->mid;
            p2 = seg->end;

            if (p1.x == p2.x) carveCorridorVer (p1, p2, mapCells);
            else carveCorridorHor (p1, p2, mapCells);
        }

        else {
            Point p1 = seg->start;
            Point p2 = seg->end;

            if (p1.x == p2.x) carveCorridorVer (p1, p2, mapCells);
            else carveCorridorHor (p1, p2, mapCells);
        }

        ptr = ptr->next;
    }

}


/*** DRAWING ***/

unsigned int wallCount = 0;

Wall walls[MAX_WALLS];
// Wall walls[MAP_WIDTH][MAP_HEIGHT];

// TODO: what color do we want for walls?
void createWall (u32 x, u32 y) {

    Wall *new = &walls[wallCount]; 

    new->x = x;
    new->y = y;
    // new->glyph = '#';
    // new->visibleOutsideFov = true;
    new->hasBeenSeen = false;
    new->blocksMovement = true;
    new->blocksSight = true;

}


/*** GENERATION ***/

// Controls the algorithms for generating random levels with the desired data
void generateMap (bool **mapCells) {

    // carve out non-overlaping rooms that are randomly placed, and of random size
    bool roomsDone = false;
    Room *firstRoom = NULL;
    unsigned int roomCount = 0;
    u32 cellsUsed = 0;
    
    // create room data
    // fprintf (stdout, "Generating rooms...\n");
    while (!roomsDone) {
        // generate a random width/height for a room
        u32 w = (u32) randomInt (5, 12);
        u32 h = (u32) randomInt (5, 12);

        // generate random positions
        u32 x = (u32) randomInt (1, MAP_WIDTH - w - 1);
        u32 y = (u32) randomInt (1, MAP_HEIGHT - h - 1);

        if (carveRoom (x, y, w, h, mapCells)) {
            Room roomData = { x, y, w, h, NULL };
            if (roomCount == 0) firstRoom = createRoomList (firstRoom, &roomData);
            else addRoom (firstRoom, &roomData);
            
            roomCount++;
            cellsUsed += (w * h);
        }

        if (((float) cellsUsed / (float) (MAP_HEIGHT * MAP_WIDTH)) > 0.45) roomsDone = true;

    }

    // This is a simple way of drawing corridors, is this a good way, 
    // or do we need a more advanced system??
    // join all the rooms with corridors
    // fprintf (stdout, "Generating corridors...\n");
    
    // creating corridors using the list of rooms
    // 08/08/2018 -- 7:55
    // I think we got it working the same way as the array, but we still need to tweak
    // how the map generates in general..
    DoubleList *hallways = dlist_init (free);

    Room *ptr = firstRoom->next, *preptr = firstRoom;
    while (ptr != NULL) {
        Room *from = preptr;
        Room *to = ptr;

        Point fromPt = randomRoomPoint (from);
        Point toPt = randomRoomPoint (to);

        DoubleList *segments = dlist_init (free);

        // break the proposed hallway into segments
        getSegments (segments, fromPt, toPt, firstRoom);

        // traverse the segment's list and skip adding any segments
        // that join rooms that are already joined
        for (ListElement *e = LIST_START (segments); e != NULL; e = e->next) {
            i32 rm1 = ((Segment *) (e->data))->roomFrom;
            i32 rm2 = ((Segment *) (e->data))->roomTo;

            Segment *uSeg = NULL;
            if (hallways->size == 0) uSeg = (Segment *) e->data;
            else {
                bool unique = true;
                for (ListElement *h = LIST_START (hallways); h != NULL; h = h->next) {
                    Segment *seg = (Segment *) (h->data);
                    if (((seg->roomFrom == rm1) && (seg->roomTo == rm2)) ||
                    ((seg->roomTo == rm1) && (seg->roomFrom == rm2))) {
                        unique = false;
                        break;
                    }
                }

                if (unique) uSeg = (Segment *) e->data;
            }

            if (uSeg != NULL) {
                Segment *segCopy = (Segment *) malloc (sizeof (Segment));
                memcpy (segCopy, uSeg, sizeof (Segment));
                insertAfter (hallways, NULL, segCopy);
            }
        }    

        // clean up lists
        dlist_destroy (segments);

        // continue looping through the rooms
        preptr = preptr->next;
        ptr = ptr->next;
    }

    // carve out new segments and add them to the hallways list
    carveSegments (hallways, mapCells);

    // cleanning up 
    firstRoom = deleteList (firstRoom);
    dlist_destroy (hallways);

}

/*** THREAD ***/

// TODO: add the floor
void initMap (bool **mapCells) {

    // mark all the cells as filled
    for (u8 x = 0; x < MAP_WIDTH; x++) 
        for (u8 y = 0; y < MAP_HEIGHT; y++) 
            mapCells[x][y] = true;

    wallCount = 0;

    generateMap (mapCells);

    // draw the map
    for (u8 x = 0; x < MAP_WIDTH; x++)
        for (u8 y = 0; y < MAP_HEIGHT; y++)
            if (mapCells[x][y]) createWall (x, y), wallCount++;

}