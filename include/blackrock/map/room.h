#ifndef _BLACK_ROOM_H_
#define _BLACK_ROOM_H_

#include "cerver/types/types.h"

typedef struct Room {

    i32 x, y;
    i32 w, h;

    struct Room *next;

} Room;

extern Room *createRoomList (Room *first, Room *data);
extern void addRoom (Room *first, Room *data);
extern Room *deleteList (Room *first);

#endif