#ifndef _ERMIRY_H_
#define _ERMIRY_H_

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include <mongoc/mongoc.h>
#include <bson/bson.h>

#include "models.h"

// ermiry error codes
#define SERVER_ERROR                100
#define NO_ERRORS                   0
#define NOT_USER_FOUND              1
#define WRONG_PASSWORD              2
#define PROFILE_NOT_FOUND           3

// init ermiry processes
extern int ermiry_init (void);

// clean up ermiry data
extern int ermiry_end (void);

// search for a user with the given username
// if we find one, check if the password match
User *ermiry_user_get (const char *username, const char *password, int *errors);

// we know that a user exists, so get the associated black profile
BlackProfile *ermiry_black_profile_get (const bson_oid_t user_oid, int *errors);

/*** ermiry achievements ***/

typedef struct ErmiryAchievement {

    String *name;
    String *description;
    // TODO: image

} ErmiryAchievement;

typedef struct SErmiryAchievement {

    SStringM name;
    SStringXL description;
    // TODO: image

} SErmiryAchievement;

#endif