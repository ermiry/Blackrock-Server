#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>

#include "cerver/types/types.h"

#include "cerver/collections/llist.h"
#include "cerver/collections/dllist.h"

#include "cerver/utils/utils.h"

#include "cengine/game/go.h"

#include "blackrock/map/room.h"
#include "blackrock/map/map.h"

static Segment *segment_new (void) {

    Segment *seg = (Segment *) malloc (sizeof (Segment));
    if (seg) memset (seg, 0, sizeof (Segment));
    return seg;

}

static void segment_delete (void *data) {

    if (data) free (data);

}

#pragma region DUNGEON

static Coord dungeon_get_free_spot (Dungeon *dungeon) {

    Coord freespot;

    for (;;) {
        u32 freeX = (u32) random_int_in_range (0, dungeon->width - 1);
        u32 freeY = (u32) random_int_in_range (0, dungeon->height - 1);

        if (dungeon->map[freeX][freeY] == 0) {
            freespot.x = freeX;
            freespot.y = freeY;
            break;
        }
    }

    return freespot;

}

static Coord dungeon_random_room_point (Room *room) {

    u32 px = (u32) (rand () % (room->w - 1)) + room->x;
    u32 py = (u32) (rand () % (room->h - 1)) + room->y;

    Coord randPoint = { px, py };
    return randPoint;

}

static i32 dungeon_room_with_point (Coord pt, Room *first) {

    i32 retVal = 0;
    Room *ptr = first;
    while (ptr != NULL) {
        if ((ptr->x <= pt.x) && ((ptr->x + ptr->w) > pt.x) &&
        (ptr->y <= pt.y) && ((ptr->y + ptr->h) > pt.y)) 
            return retVal;            

        ptr = ptr->next;
        retVal++;
    }

    return -1;

}

static bool dungeon_carve_room (u32 x, u32 y, u32 w, u32 h, u8 **mapCells) {

    for (u8 i = x - 1; i < x + (w + 1); i++) 
        for (u8 j = y - 1; j < y + (h + 1); j++)
            if (mapCells[i][j] == false) return false;

    // carve the room
    for (u8 i = x; i < x + w; i++) 
        for (u8 j = y; j < y + h; j++)
            mapCells[i][j] = false;

    return true;

}

static void dungeon_carve_corridor_hor (Coord from, Coord to, u8 **mapCells) {

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

static void dungeon_carve_corridor_ver (Coord from, Coord to, u8 **mapCells) {

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

static void dungeon_get_segments (DoubleList *segments, Coord from, Coord to, Room *firstRoom) {

    bool usingWayPoint = false;
    Coord waypoint = to;
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

    Coord curr = from;
    bool horizontal = false;
    i8 step = 1;
    if (from.y == waypoint.y) {
        horizontal = true;
        if (from.x > waypoint.x) step = -1;
    }

    else if (from.y > waypoint.y) step = -1;

    i32 currRoom = dungeon_room_with_point (curr, firstRoom);
    Coord lastPoint = from;
    bool done = false;
    Segment *turnSegment = NULL;
    while (!done) {
        i32 rm = dungeon_room_with_point (curr, firstRoom);
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
                    dlist_insert_after (segments, NULL, s);
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
                    dlist_insert_after (segments, NULL, turnSegment);
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
                    dlist_insert_after (segments, NULL, s);
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
                    dlist_insert_after (segments, NULL, turnSegment);
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
                    dlist_insert_after (segments, NULL, s);
                }

                currRoom = rm;
                lastPoint = curr;
            }

            if (horizontal) curr.x += step;
            else curr.y += step;
        }
    }

}

static void dungeon_carve_segments (DoubleList *hallways, u8 **mapCells) {

    Segment *seg = NULL;
    ListElement *ptr = dlist_start (hallways);
    while (ptr != NULL) {
        seg = (Segment *) ptr->data;

        if (seg->hasWayPoint) {
            Coord p1 = seg->start;
            Coord p2 = seg->mid;

            if (p1.x == p2.x) dungeon_carve_corridor_ver (p1, p2, mapCells);
            else dungeon_carve_corridor_hor (p1, p2, mapCells);

            p1 = seg->mid;
            p2 = seg->end;

            if (p1.x == p2.x) dungeon_carve_corridor_ver (p1, p2, mapCells);
            else dungeon_carve_corridor_hor (p1, p2, mapCells);
        }

        else {
            Coord p1 = seg->start;
            Coord p2 = seg->end;

            if (p1.x == p2.x) dungeon_carve_corridor_ver (p1, p2, mapCells);
            else dungeon_carve_corridor_hor (p1, p2, mapCells);
        }

        ptr = ptr->next;
    }

}

static void dungeon_create (Dungeon *dungeon) {

    // carve out non-overlaping rooms that are randomly placed, and of random size
    bool roomsDone = false;
    Room *firstRoom = NULL;
    u32 roomCount = 0;
    u32 cellsUsed = 0;
    
    // create room data
    while (!roomsDone) {
        // generate a random width/height for a room
        u32 w = (u32) random_int_in_range (DUNGEON_ROOM_MIN_WIDTH, DUNGEON_ROOM_MAX_WIDTH);
        u32 h = (u32) random_int_in_range (DUNGEON_ROOM_MIN_HEIGHT, DUNGEON_ROOM_MAX_HEIGHT);

        // generate random positions
        u32 x = (u32) random_int_in_range (1, dungeon->width - w - 1);
        u32 y = (u32) random_int_in_range (1, dungeon->height - h - 1);

        if (dungeon_carve_room (x, y, w, h, dungeon->map)) {
            Room roomData = { x, y, w, h, NULL };
            if (roomCount == 0) firstRoom = createRoomList (firstRoom, &roomData);
            else addRoom (firstRoom, &roomData);
            
            roomCount++;
            cellsUsed += (w * h);
        }

        if (((float) cellsUsed / (float) (dungeon->width * dungeon->height)) >= dungeon->fillPercent) 
            roomsDone = true;
    }

    // join all the rooms with corridors
    DoubleList *hallways = dlist_init (segment_delete, NULL);

    Room *ptr = firstRoom->next, *preptr = firstRoom;
    while (ptr != NULL) {
        Room *from = preptr;
        Room *to = ptr;

        Coord fromPt = dungeon_random_room_point (from);
        Coord toPt = dungeon_random_room_point (to);

        DoubleList *segments = dlist_init (segment_delete, NULL);

        // break the proposed hallway into segments
        dungeon_get_segments (segments, fromPt, toPt, firstRoom);

        // traverse the segment's list and skip adding any segments
        // that join rooms that are already joined
        for (ListElement *e = dlist_start (segments); e != NULL; e = e->next) {
            i32 rm1 = ((Segment *) (e->data))->roomFrom;
            i32 rm2 = ((Segment *) (e->data))->roomTo;

            Segment *uSeg = NULL;
            if (hallways->size == 0) uSeg = (Segment *) e->data;
            else {
                bool unique = true;
                for (ListElement *h = dlist_start (hallways); h != NULL; h = h->next) {
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
                dlist_insert_after (hallways, NULL, segCopy);
            }
        }    

        dlist_destroy (segments);

        // continue looping through the rooms
        preptr = preptr->next;
        ptr = ptr->next;
    }

    // carve out new segments and add them to the hallways list
    dungeon_carve_segments (hallways, dungeon->map);

    // cleanning up 
    firstRoom = deleteList (firstRoom);
    dlist_destroy (hallways);

}

// FIXME: add the correct sprites with the resources manager
static void dungeon_draw (Map *map, Dungeon *dungeon) {

    /* GameObject *go = NULL;
    Transform *transform = NULL;
    Graphics *graphics  = NULL;
    for (u32 y = 0; y < dungeon->height; y++) {
        for (u32 x = 0; x < dungeon->width; x++) {
            if (dungeon->map[x][y]) {
                go = game_object_new (NULL, NULL);
                graphics = game_object_add_component (go, GRAPHICS_COMP);
                if (graphics)
                    graphics_set_sprite (graphics,
                        createString ("%s%s", ASSETS_PATH, "artwork/mapTile_087.png"));
                transform = game_object_add_component (go, TRANSFORM_COMP);
                if (transform) {
                    transform->position.x = graphics->sprite->w * x;
                    transform->position.y = graphics->sprite->h * y;
                }

                map->go_map[x][y] = go;    
            }
        }
    }

    for (u32 y = 0; y < dungeon->height; y++) {
        for (u32 x = 0; x < dungeon->width; x++) {
            printf ("%i", dungeon->map[x][y]); 
        }
        printf ("\n");
    } */

}

Dungeon *dungeon_generate (Map *map, u32 width, u32 height, u32 seed, float fillPercent) {

    Dungeon *dungeon = (Dungeon *) malloc (sizeof (Dungeon));
    if (dungeon) {
        dungeon->width = width;
        dungeon->height = height;

        dungeon->map = (u8 **) calloc (width, sizeof (u8 *));
        for (u16 i = 0; i < width; i++) dungeon->map[i] = (u8 *) calloc (height, sizeof (u8));

        // mark all the cells as filled
        for (u16 x = 0; x < width; x++) 
            for (u16 y = 0; y < height; y++) 
                dungeon->map[x][y] = 1;

        if ((fillPercent < 0) || (fillPercent > 1))
            dungeon->fillPercent = DUNGEON_FILL_PERCENT;

        else dungeon->fillPercent = fillPercent;

        if (seed <= 0) {
            srand ((unsigned) time (NULL));
            dungeon->seed = random_int_in_range (0, 1000000);
            dungeon->useRandomSeed = true;
        }

        else {
            dungeon->seed = seed;
            random_set_seed (dungeon->seed);
            dungeon->useRandomSeed = false;
        } 

        dungeon_create (dungeon);
        dungeon_draw (map, dungeon);
    }

    return dungeon;

}

void dungeon_destroy (Dungeon *dungeon) {

    if (dungeon) {
        if (dungeon->map) free (dungeon->map);
        free (dungeon);
    }

}

#pragma endregion

#pragma region CAVE ROOM

// TODO:
// FIXME: we need to fix how to free the tiles that are in multiple lists at a time
static void cave_room_destroy (void *data) {

    if (data) {
        Room *room = (Room *) data;
        free (room);
    }

}

static CaveRoom *cave_room_create (LList *roomTiles, u32 **map) {

    CaveRoom *room = (CaveRoom *) malloc (sizeof (CaveRoom));
    if (room) {
        room->tiles = roomTiles;
        room->roomSize = llist_size (room->tiles);
        room->connectedRooms = llist_init (cave_room_destroy);
        room->edgeTiles = llist_init (free);

        Coord *tile = NULL;
        for (ListNode *n = dlist_start (room->tiles); n != NULL; n = n->next) {
            tile = llist_data (n);
            for (u8 x = tile->x - 1; x <= tile->x + 1; x++) {
                for (u8 y = tile->y - 1; y <= tile->y + 1; y++) {
                    if (x == tile->x || y == tile->y)
                        if (map[x][y] == 1)
                            llist_insert_next (room->edgeTiles, llist_end (room->edgeTiles), tile);
                }
            }
        }
    }

    return room;

}

#pragma endregion

#pragma region CAVE

static Coord cave_get_free_spot (Cave *cave) {

    Coord freespot;

    for (;;) {
        u32 freeX = (u32) random_int_in_range (0, cave->width - 1);
        u32 freeY = (u32) random_int_in_range (0, cave->heigth - 1);

        if (cave->map[freeX][freeY] == 0) {
            freespot.x = freeX;
            freespot.y = freeY;
            break;
        }
    }

    return freespot;

}

static void cave_random_fill_map (Cave *cave) {

    random_set_seed (cave->seed);

    for (u32 x = 0; x < cave->width; x++) {
        for (u32 y = 0; y < cave->heigth; y++) {
            if (x == 0 || x == cave->width - 1 || y == 0 || y == cave->heigth - 1)
                cave->map[x][y] = 1;

            else 
                cave->map[x][y] = (random_int_in_range (0, 100) < cave->fillPercent) ? 1 : 0;
            
        }
    }

}

static bool cave_is_in_map_range (Cave *cave, i32 x, i32 y) {

    return (x >= 0 && x < cave->width && y >= 0 && y < cave->heigth);

}

static u8 cave_get_surrounding_wall_count (Cave *cave, i32 gridX, i32 gridY) {

    u8 wallCount = 0;
    for (i32 neighbourX = gridX - 1; neighbourX <= gridX + 1; neighbourX++) {
        for (i32 neighbourY = gridY - 1; neighbourY <= gridY + 1; neighbourY++) {
            if (cave_is_in_map_range (cave, neighbourX, neighbourY)) {
                if (neighbourX != gridX || neighbourY != gridY)
                    wallCount += cave->map[neighbourX][neighbourY];
            }

            else wallCount++;
                
        }
    }

    return wallCount;

}

static void cave_smooth_map (Cave *cave) {

    for (u32 x = 0; x < cave->width - 1; x++){
        for (u32 y = 0; y < cave->heigth - 1; y++) {
            u8 neighbourWallTiles = cave_get_surrounding_wall_count (cave, x, y);

            if (neighbourWallTiles > 4) cave->map[x][y] = 1;
            else if (neighbourWallTiles < 4) cave->map[x][y] = 0;
        }
    }

}

// FIXME: draw the correct tiles
// TODO: create a more advanced system based on c sharp accent resources manager
// TODO: also create a more advance system for multiple sprites and gameobjects for
// destroying the map
// TODO: create a parent gameobejct and names based on position
// create a game object for each cave
static void cave_draw (Map *map, Cave *cave) {

    /* GameObject *go = NULL;
    Transform *transform = NULL;
    Graphics *graphics = NULL;
    for (u32 y = 0; y < cave->heigth; y++) {
        for (u32 x = 0; x < cave->width; x++) {
            if (cave->map[x][y] == 1) {
                go = game_object_new (NULL, "map");
                graphics = game_object_add_component (go, GRAPHICS_COMP);
                if (graphics) 
                    graphics_set_sprite (graphics, 
                        createString ("%s%s", ASSETS_PATH, "artwork/mapTile_087.png"));
                
                transform = game_object_add_component (go, TRANSFORM_COMP);
                if (transform) {
                    transform->position.x = graphics->sprite->w * x;
                    transform->position.y = graphics->sprite->h * y;
                }

                map->go_map[x][y] = go;
            }
        }
    } */

}

Cave *cave_generate (Map *map, u32 width, u32 heigth, u32 seed, u32 fillPercent) {

    Cave *cave = (Cave *) malloc (sizeof (Cave));
    if (cave) {
        cave->width = width;
        cave->heigth = heigth;

        cave->map = (u8 **) calloc (width, sizeof (u8 *));
        for (u16 i = 0; i < width; i++) cave->map[i] = (u8 *) calloc (heigth, sizeof (u8));

        cave->fillPercent = fillPercent;

        if (seed <= 0) {
            srand ((unsigned) time (NULL));
            cave->seed = random_int_in_range (0, 100000);
            cave->useRandomSeed = true;
        } 

        else {
            cave->seed = seed;
            cave->useRandomSeed = false;
        } 
        
        cave_random_fill_map (cave);

        for (u8 i = 0; i < 3; i++) cave_smooth_map (cave);

        cave_draw (map, cave);
    }

    return cave;

}

void cave_destroy (Cave *cave) {

    if (cave) {
        if (cave->map) free (cave->map);
        free (cave);
    }

}

#pragma endregion

#pragma region MAP

Map *map_create (u32 width, u32 heigth) {

    Map *new_map = (Map *) malloc (sizeof (Map));
    if (new_map) {
        new_map->width = width;
        new_map->heigth = heigth;

        new_map->go_map = (GameObject ***) calloc (width, sizeof (GameObject **));
        for (u16 i = 0; i < new_map->width; i++) 
            new_map->go_map[i] = (GameObject **) calloc (new_map->heigth, sizeof (GameObject *));

        new_map->dungeon = NULL;
        new_map->cave = NULL;
    }

    return new_map;

}

void map_destroy (Map *map) {

    if (map) {
        if (map->go_map) free (map->go_map);

        if (map->dungeon) dungeon_destroy (map->dungeon);
        if (map->cave) cave_destroy (map->cave);

        free (map);
    }

}

Coord map_get_free_spot (Map *map) {

    if (map) {
        // srand ((unsigned) time (NULL));

        if (map->dungeon) return dungeon_get_free_spot (map->dungeon);
        else return cave_get_free_spot (map->cave);
    }

}

#pragma endregion