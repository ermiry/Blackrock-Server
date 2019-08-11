#include <stdlib.h>

#include "cerver/types/types.h"

#include "cengine/game/go.h"

#include "blackrock/entities/player.h"

// TODO: do we still need this reference? how do we handle multiplayer?
static Player *mainPlayer = NULL;

// TODO: this should be modified by player class and race
static u8 moveSpeed = 8;

#pragma region CHARACTER

static GameObject ***character_init_inventory (void) {

    GameObject ***inventory = (GameObject ***) calloc (7, sizeof (GameObject **));

    for (u8 i = 0; i < 7; i++)
        inventory[i] = (GameObject **) calloc (3, sizeof (GameObject *));

    for (u8 y = 0; y < 3; y++)
        for (u8 x = 0; x < 7; x++)
            inventory[x][y] = NULL;

    return inventory;

}

// create an empty character
static Character *character_new (void) {

    Character *new_character = (Character *) malloc (sizeof (Character));
    if (new_character) {
        new_character->entity = entity_new ();
        
        new_character->money[0] = new_character->money[1] = new_character->money[2] = 0;

        new_character->inventory = character_init_inventory ();

        new_character->equipment = (GameObject **) calloc (EQUIPMENT_ELEMENTS, sizeof (GameObject *));
        for (u8 i = 0; i < EQUIPMENT_ELEMENTS; i++) new_character->equipment[i] = NULL;

        new_character->weapons = (GameObject **) calloc (2, sizeof (GameObject **));
        for (u8 i = 0; i < 2; i++) new_character->weapons[i] = NULL;

        new_character->animations = NULL;
    }

    return new_character;

}

static void character_destroy (Character *character) {

    if (character) {
        if (character->inventory) {
            // clean up inventory
            for (u8 y = 0; y < 3; y++) 
                for (u8 x = 0; x < 7; x++) 
                    character->inventory[x][y] = NULL;

            free (character->inventory);
        }

        // clean up weapons
        if (character->weapons) {
            for (u8 i = 0; i < 2; i++) character->weapons[i] = NULL;
            free (character->weapons);
        }

        // clean up equipment
        if (character->equipment) {
            for (u8 i = 0; i < EQUIPMENT_ELEMENTS; i++) character->equipment[i] = NULL;
            free (character->equipment);
        }

        // dlist_delete (character->animations);

        free (character);
    }

}

#pragma endregion

// FIXME:
#pragma region PLAYER PROFILE

// PlayerProfile *main_player_profile = NULL;

// FIXME:
// TODO: how do we request the friend list from the server?
/* void player_profile_get_from_server (SPlayerProfile *s_profile) {

    if (s_profile) {
        main_player_profile = (PlayerProfile *) malloc (sizeof (PlayerProfile));

        if (main_player_profile) {
            main_player_profile->profileID = s_profile->profileID;
            main_player_profile->username = createString ("%s", s_profile->username);
            main_player_profile->kills = s_profile->kills;
            main_player_profile->gamesPlayed = s_profile->gamesPlayed;
            main_player_profile->highscore = s_profile->highscore;
            main_player_profile->n_friends = s_profile->n_friends;
            // if (main_player_profile->n_friends > 0)
            //     main_player_profile->friends = createString ("%s", s_profile->friends);
            // else main_player_profile->friends = NULL;

            #ifdef BLACK_DEBUG
            logMsg (stdout, DEBUG_MSG, NO_TYPE, "Got a player profile from the server:");
            printf ("Username: %s\n", main_player_profile->username);
            printf ("Kills: %i\n", main_player_profile->kills);
            printf ("Games played: %i\n", main_player_profile->gamesPlayed);
            printf ("Highscore: %i\n", main_player_profile->highscore);
            printf ("No. friends: %i\n", main_player_profile->n_friends);
            #endif
        }
    }

} */

#pragma endregion

#pragma region PLAYER COMPONENT

void *player_comp_new (u32 goID) {

    Player *new_player = (Player *) malloc (sizeof (Player));
    if (new_player) {
        new_player->goID = goID;

        new_player->currState = PLAYER_IDLE;
        // new_player->profile = NULL;
        new_player->character = character_new ();
    }

    return new_player;

}

void player_comp_delete (void *ptr) {

    if (ptr) {
        Player *player = (Player *) ptr;
        // TODO: destroy player profile
        character_destroy (player->character);

        free (player);
    }

}

GameObject *player_init (void) {

    /* GameObject *new_player_go = game_object_new ("player", "player");
    if (new_player_go) {
        Transform *trans = (Transform *) game_object_add_component (new_player_go, TRANSFORM_COMP);
        Graphics *graphics = (Graphics *) game_object_add_component (new_player_go, GRAPHICS_COMP);
        Animator *anim = (Animator *) game_object_add_component (new_player_go, ANIMATOR_COMP);
        mainPlayer = (Player *) game_object_add_user_component (new_player_go, "player");

        AnimData *anim_data = animation_file_parse ("./data/animations/player/player.json");

        graphics_set_sprite_sheet (graphics, 
            createString ("%s%s", ASSETS_PATH, "artwork/characters/adventurer-sheet.png"));
        sprite_sheet_set_sprite_size (graphics->spriteSheet, anim_data->w, anim_data->h);
        sprite_sheet_set_scale_factor (graphics->spriteSheet, anim_data->scale);
        sprite_sheet_crop (graphics->spriteSheet);

        // set up animations
        // player idle without sword
        // player_idle_anim = animation_create (4,
        //     my_graphics->spriteSheet->individualSprites[0][0], my_graphics->spriteSheet->individualSprites[1][0], 
        //     my_graphics->spriteSheet->individualSprites[2][0], my_graphics->spriteSheet->individualSprites[3][0]);
        // animation_set_speed (player_idle_anim, 300);  

        mainPlayer->character->animations = anim_data->animations;

        animator_set_current_animation (anim, animation_get_by_name (mainPlayer->character->animations, "idle"));
        animator_set_default_animation (anim, animation_get_by_name (mainPlayer->character->animations, "idle"));
    }

    return new_player_go; */

}

// FIXME: normalize the vector when moving in diagonal!!
// updates the main player
void player_update (void *data) {

    /* GameObject *player_go = (GameObject *) data;
    Transform *trans = (Transform *) game_object_get_component (player_go, TRANSFORM_COMP);
    Graphics *graphics = (Graphics *) game_object_get_component (player_go, GRAPHICS_COMP);
    Animator *anim = (Animator *) game_object_get_component (player_go, ANIMATOR_COMP);

    mainPlayer->currState = PLAYER_IDLE;

    // update player position
    Vector2D new_vel = { 0, 0 };
    if (input_is_key_down (SDL_SCANCODE_D)) {
        new_vel.x = moveSpeed;
        graphics->flip = NO_FLIP;
        mainPlayer->currState = PLAYER_MOVING;
    }

    if (input_is_key_down (SDL_SCANCODE_A)) {
        new_vel.x = -moveSpeed;
        graphics->flip = FLIP_HORIZONTAL;
        mainPlayer->currState = PLAYER_MOVING;
    }

    if (input_is_key_down (SDL_SCANCODE_S)) {
        new_vel.y = moveSpeed;
        mainPlayer->currState = PLAYER_MOVING;
    } 

    if (input_is_key_down (SDL_SCANCODE_W)) {
        new_vel.y = -moveSpeed;
        mainPlayer->currState = PLAYER_MOVING;
    } 

    if (input_is_key_down (SDL_SCANCODE_F)) mainPlayer->currState = PLAYER_ATTACK;

    switch (mainPlayer->currState) {
        case PLAYER_IDLE: 
            animator_set_current_animation (anim, animation_get_by_name (mainPlayer->character->animations, "idle")); 
            break;
        case PLAYER_MOVING: 
            vector_add_equal (&trans->position, new_vel);
            animator_set_current_animation (anim, animation_get_by_name (mainPlayer->character->animations, "run"));
            break;
        case PLAYER_ATTACK: 
            animator_play_animation (anim, animation_get_by_name (mainPlayer->character->animations, "attack"));
            break;

        default: break;
    } */

}

#pragma endregion

/*** OLD CODE ***/
/* #pragma region PLAYER

Player *main_player = NULL;
Config *playerConfig = NULL;
Config *classesConfig = NULL;

u8 inventoryItems = 0;

void player_load_data (void) {

    playerConfig = parseConfigFile ("./data/player.cfg");
    if (!playerConfig) 
        die ("Critical Error! No player config!\n");


    classesConfig = parseConfigFile ("./data/classes.cfg");
    if (!classesConfig) 
        die ("Critical Error! No classes config!\n");

}

u32 player_get_class_color (u8 c) {

    u32 retVal;

    switch (c) {
        case WARRIOR: retVal = 0xD63031FF; break;
        case PALADIN: retVal = 0xFD79A8FF; break;
        case ROGUE: retVal = 0xFFC048FF; break;
        case PRIEST: retVal = 0x05C46B; break;
        case DEATH_KNIGHT: retVal = 0x0A3D62FF; break;
        case MAGE: retVal = 0x7158E2FF; break;
        default: break;    
    }

    return retVal;

}

// TODO: check for a save file to retrive the information freom there instead
// because the player is an special GO, we want to initialize him differently
void player_init (Player *p) {

    player_load_data ();

    p->pos->objectId = 0;
    p->pos->layer = TOP_LAYER;

    p->physics->objectId = 0;
    p->physics->blocksMovement = true;
    p->physics->blocksSight = true;

    ConfigEntity *playerEntity = getEntityWithId (playerConfig, 1);

    p->genre = atoi (getEntityValue (playerEntity, "genre"));
    p->level = atoi (getEntityValue (playerEntity, "level"));

    // get class
    CharClass cClass = atoi (getEntityValue (playerEntity, "class"));
    p->cClass = cClass;
    ConfigEntity *classEntity = getEntityWithId (classesConfig, p->cClass);
    p->color = xtoi (getEntityValue (classEntity, "color"));

    // money
    p->money[0] = atoi (getEntityValue (playerEntity, "gold"));
    p->money[1] = atoi (getEntityValue (playerEntity, "silver"));
    p->money[2] = atoi (getEntityValue (playerEntity, "copper"));

    // As of 18/08/2018 -- 23-21 -- the color of the glyph is based on the class
    p->graphics->objectId = 0;
    p->graphics->bgColor = 0x000000FF;
    p->graphics->fgColor = p->color;
    p->graphics->hasBeenSeen = false;
    p->graphics->visibleOutsideFov = false;
    p->graphics->glyph = atoi (getEntityValue (playerEntity, "glyph"));
    p->graphics->name = NULL;

    // TODO: modify the combat component based on the class
    // we need to have a file where we can read the stats we have saved
    // also we need to take into account that every class has different stats
    p->combat->baseStats.power = atoi (getEntityValue (playerEntity, "power"));
    p->combat->baseStats.powerRegen = atoi (getEntityValue (playerEntity, "powerRegen"));
    p->combat->baseStats.strength = atoi (getEntityValue (playerEntity, "strength"));
    p->combat->attack.baseDps = atoi (getEntityValue (playerEntity, "baseDps"));
    p->combat->attack.hitchance = atoi (getEntityValue (playerEntity, "hitchance"));
    p->combat->attack.attackSpeed = atoi (getEntityValue (playerEntity, "attack_speed"));
    p->combat->attack.spellPower = atoi (getEntityValue (playerEntity, "spellPower"));
    p->combat->attack.criticalStrike = atoi (getEntityValue (playerEntity, "critical"));

    p->combat->defense.armor = atoi (getEntityValue (playerEntity, "armor"));
    p->combat->defense.block = atoi (getEntityValue (playerEntity, "block"));
    p->combat->defense.dodge = atoi (getEntityValue (playerEntity, "dodge"));
    p->combat->defense.parry = atoi (getEntityValue (playerEntity, "block"));

    p->combat->baseStats.maxHealth = (atoi (getEntityValue (playerEntity, "baseHP"))) + p->combat->defense.armor;
    p->combat->baseStats.health = p->combat->baseStats.maxHealth;

    // TODO: depending on the class, we have a different starting weapon
    u16 startingWeapon = atoi (getEntityValue (playerEntity, "startingWeapon"));
    Item *weapon = createWeapon (startingWeapon);
    if (weapon != NULL) {
        Weapon *w = (Weapon *) getItemComponent (weapon, WEAPON);
        if (w != NULL) fprintf (stdout, "Found the weapon component!\n");
        p->weapons[w->slot] = weapon;
        w->isEquipped = true;
        Graphics *g = (Graphics *) getGameComponent (weapon, GRAPHICS);
        fprintf (stdout, "Done creating: %s\n", g->name);
    }

    else fprintf (stderr, "Problems creating player weapon.");

    // we don't need to have this two in memory
    clearConfig (playerConfig);
    clearConfig (classesConfig);

    fprintf (stdout, "Init player done!\n");

}

void player_reset (Player *player) {

    if (player) {
        // reset inventory
        if (player->inventory) {
            for (u8 y = 0; y < 3; y++) 
                for (u8 x = 0; x < 7; x++) 
                    player->inventory[x][y] = NULL;

            inventoryItems = 0;
        }

        // reset weapons
        if (player->weapons) 
            for (u8 i = 0; i < 2; i++) player->weapons[i] = NULL;
        
        // reset equipment
        if (player->equipment)
            for (u8 i = 0; i < EQUIPMENT_ELEMENTS; i++) player->equipment[i] = NULL;

    }

}

#pragma endregion */