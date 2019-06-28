#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "cerver/types/types.h"
#include "cerver/collections/dllist.h"

#include "cerver/game/gameType.h"

#include "cengine/game/go.h"

#include "blackrock/world.h"
#include "blackrock/map/map.h"
#include "blackrock/entities/enemy.h"
#include "blackrock/entities/player.h"

// TODO:
static u8 load_game_data (void) {

    // connect to items db
    // items_init ();
    // connect to enemies db and load enemy data
    // if (!enemies_connect_db ()) 
    //     return 0;

    // return 0;

}

// FIXME: move this from here
static u8 game_init (void) {

    // FIXME: this should be called when the engine starts
    // game_objects_init_all ();

    // if (!load_game_data ()) {
    //     // init world
    //     world = world_create ();

    //     // init map
    //     // FIXME: from where do we get this values?
    //     // single player -> random values
    //     // multiplayer -> we get it from the server
    //     world->game_map = map_create (100, 40);
    //     world->game_map->dungeon = dungeon_generate (world->game_map, 
    //         world->game_map->width, world->game_map->heigth, 100, .45);

    //     // spawn enemies
    //     enemies_spawn_all (world, random_int_in_range (5, 10));

    //     // spawn items

    //     // init player(s)
    //     dlist_insert_after (world->players, dlist_start (world->players), player_init ());

    //     // spawn players
    //     GameObject *go = NULL;
    //     Transform *transform = NULL;
    //     for (ListElement *le = dlist_start (world->players); le != NULL; le = le->next) {
    //         go = (GameObject *) le->data;
    //         transform = (Transform *) game_object_get_component (go, TRANSFORM_COMP);
    //         Coord spawnPoint = map_get_free_spot (world->game_map);
    //         // FIXME: fix wolrd scale!!!
    //         transform->position.x = spawnPoint.x * 64;
    //         transform->position.y = spawnPoint.y * 64;
    //     }

    //     // update camera
    //     GameObject *main_player = (GameObject *) (dlist_start (world->players)->data );
    //     transform = (Transform *) game_object_get_component (main_player, TRANSFORM_COMP);
    //     main_camera->center = transform->position;

    //     // init score

    //     // FIXME: we are ready to start updating the game
    //     game_state->update = game_update;
    //     // manager->curr_state->update = game_update;

    //     return 0;
    // } 

    return 1;

}

// destroy all game data
static u8 game_end (void) {

    // FIXME: destroy world 
    // enemy_data_delete_all ();

    // enemies_disconnect_db ();

    // #ifdef DEV
    // logMsg (stdout, DEBUG_MSG, GAME, "Game end!");
    // #endif

}

void *blackrock_arcade_start (void *data_ptr) {



}

void *blackrock_arcade_end (void *data_ptr) {



}

// register blackrock game types to the cerver
void blackrock_register_game_types (void) {

    // create and register arcade game type
    GameType *arcade = game_type_create ("arcade", NULL, NULL, 
        blackrock_arcade_start, blackrock_arcade_end);
    game_type_register (arcade);

}