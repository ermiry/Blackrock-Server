#ifndef ROOM_H
#define ROOM_H

#include "blackrock.h"

typedef struct Room {

    i32 x, y;
    i32 w, h;

    struct Room *next;

} Room;

extern Room *createRoomList (Room *first, Room *data);
extern void addRoom (Room *first, Room *data);
extern Room *deleteList (Room *first);

#endif