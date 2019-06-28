#include <stdlib.h>

#include "game/map/room.h"

Room *createRoomList (Room *first, Room *data) {

    Room *new, *ptr;

    new = (Room *) malloc (sizeof (Room));

    new->x = data->x;
    new->y = data->y;
    new->w = data->w;
    new->h = data->h;

    if (first == NULL) {
        new->next = NULL;
        first = new;
    }

    else {
        ptr = first;
        while (ptr->next != NULL)
            ptr = ptr->next;

        ptr->next = new;
        new->next = NULL;
    }

    return new;

}

void addRoom (Room *first, Room *data) {

    Room *ptr = first;
    Room *new = (Room *) malloc (sizeof (Room));

    new->x = data->x;
    new->y = data->y;
    new->w = data->w;
    new->h = data->h;
    new->next = NULL;

    while (ptr->next != NULL)
        ptr = ptr->next;

    ptr->next = new;

}

Room *deleteBeginning (Room *first) {

    Room *ptr = first;
    first = first->next;
    free (ptr);
    return first;

}

Room *deleteList (Room *first) {

    Room *ptr;
    if (first != NULL) {
        ptr = first;
        while (ptr != NULL) {
            first = deleteBeginning (ptr);
            ptr = first;
        }
    }

    return NULL;

}