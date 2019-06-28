#include <stdlib.h>

#include "blackrock/world.h"

World *world = NULL;

World *world_create (void) {

    World *new_world = (World *) malloc (sizeof (World));
    if (new_world) {
        new_world->game_map = NULL;

        new_world->players = dlist_init (game_object_destroy_ref, NULL);
        new_world->enemies = dlist_init (game_object_destroy_ref, NULL);
    }

    return new_world;
}

void world_destroy (World *world) {

    if (world) {
        if (world->game_map) map_destroy (world->game_map);

        dlist_destroy (world->players);
        dlist_destroy (world->enemies);

        free (world);
    }

}