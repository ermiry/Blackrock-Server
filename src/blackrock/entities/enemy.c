#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "cerver/collections/llist.h"
#include "cerver/collections/dllist.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

#include "blackrock/entities/entity.h"
#include "blackrock/entities/enemy.h"

const char *enemiesDbPath = "enemies.db";
static sqlite3 *enemiesDb;

#pragma region Enemy Data

LList *enemy_data = NULL;

static u8 enemy_loot_load (u32 monId, EnemyLoot *loot) {

    // get the db data
    sqlite3_stmt *res;
    char *sql = "SELECT * FROM Drops WHERE Id = ?";

    if (sqlite3_prepare_v2 (enemiesDb, sql, -1, &res, 0) == SQLITE_OK) sqlite3_bind_int (res, 1, monId);
    else {
        fprintf (stderr, "Error! Failed to execute statement: %s\n", sqlite3_errmsg (enemiesDb));
        return 1;
    } 

    int step = sqlite3_step (res);

    loot->minGold = (u32) sqlite3_column_int (res, 1);
    loot->maxGold = (u32) sqlite3_column_int (res, 2);

    // get the possible drop items
    const char *c = sqlite3_column_text (res, 3);
    if (c != NULL) {
        char *itemsTxt = (char *) calloc (strlen (c) + 1, sizeof (char));
        strcpy (itemsTxt, c);

        // parse the string by commas
        char **tokens = c_string_split (itemsTxt, ',');
        if (tokens) {
            // count how many elements will be extracted
            u32 count = 0;
            while (*itemsTxt++) if (',' == *itemsTxt) count++;

            count += 1;     // take into account the last item after the last comma

            loot->dropCount = count;
            loot->drops = (u32 *) calloc (count, sizeof (u32));
            u32 idx = 0;
            for (u32 i = 0; *(tokens + i); i++) {
                loot->drops[idx] = atoi (*(tokens + i));
                idx++;
                free (*(tokens + i));
            }

            free (tokens);
        }

        // no drop items in the db or an error somewhere retrieving the data
        // so only hanlde the loot with the money
        else {
            loot->drops = NULL;
            loot->dropCount = 0;
        } 
    }
    
    else {
        loot->drops = NULL;
        loot->dropCount = 0;
    } 

    sqlite3_finalize (res);

    return 0;

}

// Getting the enemies data from the db into memory
static int enemy_data_load_all (void *data, int argc, char **argv, char **azColName) {

    EnemyData *edata = (EnemyData *) malloc (sizeof (EnemyData));

    edata->dbId = atoi (argv[0]);
    edata->name = str_new (argv[1]);
    edata->probability = atof (argv[2]);

    // edata->anim_data = animation_file_parse (argv[3]);
    // edata->sprite_sheet = sprite_sheet_load (argv[4], main_renderer);
    // sprite_sheet_set_sprite_size (edata->sprite_sheet, edata->anim_data->w, edata->anim_data->h);
    // sprite_sheet_set_scale_factor (edata->sprite_sheet, edata->anim_data->scale);
    // sprite_sheet_crop (edata->sprite_sheet);

    if (enemy_loot_load (edata->dbId, &edata->loot)) {
        #ifdef DEV
        logMsg (stderr, ERROR, GAME, createString ("Failed getting enemy loot. Enemy id: %i", edata->dbId));
        #endif
    }

    llist_insert_next (enemy_data, llist_end (enemy_data), edata);

    return 0;

}

static void enemy_data_delete (void *data) {

    if (data) {
        EnemyData *edata = (EnemyData *) data;
        str_delete (edata->name);
        if (edata->loot.drops) free (edata->loot.drops);

        // anim_data_delete (edata->anim_data);
        // sprite_sheet_destroy (edata->sprite_sheet);

        free (edata);
    }

}

void enemy_data_delete_all (void) {

    if (enemy_data) llist_destroy (enemy_data);

}

// FIXME:
u8 enemies_connect_db (void) {

    // connect to the enemies db
    /* if (sqlite3_open (createString ("%s%s", DATA_PATH, enemiesDbPath), &enemiesDb) != SQLITE_OK) {
        #ifdef DEV
        logMsg (stderr, ERROR, GAME, "Problems connecting to enemies db!");
        logMsg (stderr, ERROR, GAME, createString ("%s", sqlite3_errmsg (enemiesDb)));
        #elif PRODUCTION
        logMsg (stderr, ERROR, NO_TYPE, "Failed to load game data!");
        #endif
        return 1;
    } 

    else {
        #ifdef DEV
        logMsg (stdout, DEBUG_MSG, GAME, "Connected to enemies db.");
        #endif

        // load basic enemy data into memory
        enemy_data = llist_init (enemy_data_delete);

        char *err = 0;
        char *sql = "SELECT * FROM Monsters";
            
        if (sqlite3_exec (enemiesDb, sql, enemy_data_load_all, NULL, &err) != SQLITE_OK) {
            #ifdef DEV
            logMsg (stderr, ERROR, GAME, "Problems loading enemy data from db!");
            logMsg (stderr, ERROR, GAME, createString ("%s", err));
            #elif PRODUCTION
            logMsg (stderr, ERROR, NO_TYPE, "Failed to load game data!");
            #endif
            return 1;
        } 
        
        else {
            #ifdef DEV
            logMsg (stdout, DEBUG_MSG, GAME, "Done loading basic enemy data from db.");
            #endif
            return 0;
        }
    } */

}

void enemies_disconnect_db (void) { sqlite3_close (enemiesDb); }

EnemyData *enemy_data_get_by_id (u32 id) {

    EnemyData *edata = NULL;
    for (ListNode *n = dlist_start (enemy_data); n != NULL; n = n->next) {
        edata = (EnemyData *) n->data;
        if (edata->dbId == id) return edata;
    }

    return NULL;

}

#pragma endregion

#pragma region Enemy Component

void *enemy_create_comp (u32 goID) {

    Enemy *enemy = (Enemy *) malloc (sizeof (Enemy));
    if (enemy) {
        memset (enemy, 0, sizeof (Enemy));
        enemy->goID = goID;
        enemy->entity = entity_new ();
    }

    return enemy;

}

void enemy_destroy_comp (void *ptr) {

    if (ptr) {
        Enemy *enemy = (Enemy *) ptr;
        entity_destroy (enemy->entity);
        dlist_destroy (enemy->animations);
        free (enemy);
    }

}

#pragma endregion

#pragma region Enemy

// FIXME:
void enemy_update (void *data) {

    GameObject *enemy_go = (GameObject *) data;
    Transform *trans = (Transform *) game_object_get_component (enemy_go, TRANSFORM_COMP);
    // Graphics *graphics = (Graphics *) game_object_get_component (enemy_go, GRAPHICS_COMP);
    // Animator *anim = (Animator *) game_object_get_component (enemy_go, ANIMATOR_COMP);
    Enemy *enemy = (Enemy *) game_object_get_user_component (enemy_go, "enemy");

    enemy->curr_state = ENEMY_IDLE;

    // TODO: handle movement
    Vector2D new_vel = { 0, 0 };

    // handle enemy logic based on state
    /* switch (enemy->curr_state) {
        case ENEMY_IDLE: 
            animator_set_current_animation (anim, animation_get_by_name (enemy->animations, "idle")); 
            break;
        case ENEMY_MOVING: 
            vector_add_equal (&trans->position, new_vel);
            animator_set_current_animation (anim, animation_get_by_name (enemy->animations, "run"));
            break;
        case ENEMY_ATTACK: 
            animator_play_animation (anim, animation_get_by_name (enemy->animations, "attack"));
            break;

        default: break;
    } */

}

static void enemy_add_transform (GameObject *enemy_go) {

    if (enemy_go) game_object_add_component (enemy_go, TRANSFORM_COMP);

}

static void enemy_add_graphics (GameObject *enemy_go, u32 dbID) {

    /* if (enemy_go) {
        Graphics *graphics = game_object_add_component (enemy_go, GRAPHICS_COMP);

        // get common graphics data
        EnemyData *edata = enemy_data_get_by_id (dbID);
        graphics_ref_sprite_sheet (graphics, edata->sprite_sheet);
    } */

}

static void enemy_add_animator (GameObject *enemy_go, u32 dbID) {

    /* if (enemy_go) {
        Animator *anim = game_object_add_component (enemy_go, ANIMATOR_COMP);

        // get common animator data
        Enemy *enemy = (Enemy *) game_object_get_user_component (enemy_go, "enemy");
        enemy->animations = (enemy_data_get_by_id (dbID))->anim_data->animations;

        animator_set_current_animation (anim, animation_get_by_name (enemy->animations, "idle"));
        animator_set_default_animation (anim, animation_get_by_name (enemy->animations, "idle"));
    } */

}

static void enemy_add_enemy_comp (GameObject *enemy_go, u32 dbID) {

    if (enemy_go) {
        Enemy *enemy = game_object_add_user_component (enemy_go, "enemy");
        enemy->dbID = dbID;

        // get enemy stats from enemy db
        sqlite3_stmt *res;
        char *sql = "SELECT * FROM Stats WHERE Id = ?";

        if (sqlite3_prepare_v2 (enemiesDb, sql, -1, &res, 0) == SQLITE_OK) {
            sqlite3_bind_int (res, 1, dbID);
            int step = sqlite3_step (res);

            enemy->entity->maxHealth = (u32) sqlite3_column_int (res, DB_COL_ENEMY_HEALTH);
            enemy->entity->currHealth = enemy->entity->maxHealth;
            enemy->entity->stats.stamina = (u32) sqlite3_column_int (res, DB_COL_ENEMY_STAMINA);
            enemy->entity->stats.strength = (u32) sqlite3_column_int (res, DB_COL_ENEMY_STRENGTH);

            enemy->entity->defense.armor = (u32) sqlite3_column_int (res, DB_COL_ENEMY_ARMOUR);
            enemy->entity->defense.dodge = (u32) sqlite3_column_int (res, DB_COL_ENEMY_DODGE);
            enemy->entity->defense.parry = (u32) sqlite3_column_int (res, DB_COL_ENEMY_PARRY);
            enemy->entity->defense.block = (u32) sqlite3_column_int (res, DB_COL_ENEMY_BLOCK);

            enemy->entity->attack.hitchance = (u32) sqlite3_column_int (res, DB_COL_ENEMY_HITCHANCE);
            enemy->entity->attack.baseDps = (u32) sqlite3_column_int (res, DB_COL_ENEMY_DPS);
            enemy->entity->attack.attackSpeed = (u32) sqlite3_column_int (res, DB_COL_ENEMY_ATTACK_SPEED);
            enemy->entity->attack.spellPower = (u32) sqlite3_column_int (res, DB_COL_ENEMY_SPELL_POWER);
            enemy->entity->attack.criticalStrike = (u32) sqlite3_column_int (res, DB_COL_ENEMY_CRITICAL);

            sqlite3_finalize (res);
        } 

        else {
            #ifdef DEV
            logMsg (stderr, ERROR, NO_TYPE, 
                "Failed to execute enemy db statement -- enemy_get_stats");
            #endif
        }
    }

}

GameObject *enemy_create (u32 dbID) {

    GameObject *enemy_go = game_object_new (NULL, "enemy");
    if (enemy_go) {
        enemy_add_enemy_comp (enemy_go, dbID);

        // enemy_add_transform (enemy_go);
        // enemy_add_graphics (enemy_go, dbID);
        // FIXME:
        // enemy_add_animator (enemy_go, dbID);
    }

    return enemy_go;

}

// // 22/08/2018 -- 7:11 -- this is our first random monster generation function
// // TODO: maybe later also take into account the current level
// // FIXME: create a better system
/* u32 getMonsterId (void) {

    // number of monster per type
    u8 weak = 2;
    u8 medium = 2;
    u8 strong = 3;
    u8 veryStrong = 2;

    u32 choice;

    switch (randomInt (1, 4)) {
        case 1:
            switch (randomInt (1, weak)) {
               case 1: choice = 101; break; 
               case 2: choice = 102; break; 
            }
            break;
        case 2:
            switch (randomInt (1, medium)) {
                case 1: choice = 201; break;
                case 2: choice = 202; break;
            }
            break;
        case 3:
            switch (randomInt (1, strong)) {
                case 1: choice = 301; break;
                case 2: choice = 302; break;
                case 3: choice = 303; break;
            }
            break;
        case 4:
            switch (randomInt (1, veryStrong)) {
                case 1: choice = 401; break;
                case 2: choice = 402; break;
            }
            break;
        default: break;
    }

    return choice;

} */

// spawn n monsters at the same time
void enemies_spawn_all (World *world, u8 monNum) {

    u8 count = 0;
    GameObject *newMon = NULL;
    Transform *transform = NULL;
    for (u8 i = 0; i < monNum; i++) {
        // generate a random monster
        newMon = enemy_create (102);
        if (newMon) {
            // get spawn point
            transform = game_object_get_component (newMon, TRANSFORM_COMP);
            Coord spawnPoint = map_get_free_spot (world->game_map);
            // FIXME: fix wolrd scale!!!
            // printf ("Spawn point %d - %d\n", spawnPoint.x, spawnPoint.y);
            transform->position.x = spawnPoint.x * 64;
            transform->position.y = spawnPoint.y * 64;

            // add monster to list
            dlist_insert_after (world->enemies, dlist_end (world->enemies), newMon);

            count++;
        }
    }

    #ifdef DEV
    logMsg (stdout, DEBUG_MSG, GAME, 
        createString ("%i / %i monsters created successfully.", count, monNum));
    #endif

}

#pragma endregion

/* u8 addMovementToMon (u32 monId, Monster *monData, GameObject *mon) {

    // get the db data
    sqlite3_stmt *res;
    char *sql = "SELECT * FROM Movement WHERE Id = ?";

    if (sqlite3_prepare_v2 (enemiesDb, sql, -1, &res, 0) == SQLITE_OK) sqlite3_bind_int (res, 1, monId);
    else {
        fprintf (stderr, "Error! Failed to execute statement: %s\n", sqlite3_errmsg (enemiesDb));
        return 1;
    } 

    int step = sqlite3_step (res);

    u32 speed = (u32) sqlite3_column_int (res, 1);
    u32 frecuency = (u32) sqlite3_column_int (res, 2);
    Movement mv = { .speed = speed, .frecuency = frecuency, .ticksUntilNextMov = 1, .chasingPlayer = false, .turnsSincePlayerSeen = 0 };
    addComponent (mon, MOVEMENT, &mv);

    sqlite3_finalize (res);

    return 0;

} */