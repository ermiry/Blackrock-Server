#include <stdlib.h>
#include <string.h>

#include "game/entities/entity.h"

char *entity_get_genre_name (Genre genre) {

    char retGenre[15];

    switch (genre) {
        case MALE: strcpy ("Male", retGenre); break;
        case FEMALE: strcpy ("Female", retGenre); break;
        case OTHER: strcpy ("Other", retGenre); break;

        default: break;
    }

    char *retVal = (char *) calloc (strlen (retGenre) + 1, sizeof (char));
    strcpy (retVal, retGenre);

    return retVal;

}

char *entity_get_race_name (CharRace charRace) {

    char race[15];

    switch (charRace) {
        case HUMAN: strcpy (race, "Human"); break;

        default: break;
    }

    char *retVal = (char *) calloc (strlen (race) + 1, sizeof (char));
    strcpy (retVal, race);

    return retVal;

}

char *entity_get_class_name (CharClass charClass) {

    char class[15];

    switch (charClass) {
        case WARRIOR: strcpy (class, "Warrior"); break;
        case PALADIN: strcpy (class, "Paladin"); break;
        case ROGUE: strcpy (class, "Rogue"); break;
        case PRIEST: strcpy (class, "Priest"); break;
        case DEATH_KNIGHT: strcpy (class, "Death Knight"); break;
        case MAGE: strcpy (class, "Mage"); break;
        default: break;    
    }

    char *retVal = (char *) calloc (strlen (class) + 1, sizeof (char));
    strcpy (retVal, class);

    return retVal;

}

// TODO: maybe load base stats based on the class and race?
LivingEntity *entity_new (void) {

    LivingEntity *entity = (LivingEntity *) malloc (sizeof (LivingEntity));
    if (entity) {
        memset (entity, 0, sizeof (entity));
        entity->name = NULL;
    } 

    return entity;

}

void entity_destroy (LivingEntity *entity) {

    if (entity) {
        str_delete (entity->name);
        free (entity);
    }

}