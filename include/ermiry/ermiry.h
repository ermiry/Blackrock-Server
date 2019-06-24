#ifndef _ERMIRY_H_
#define _ERMIRY_H_

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include <mongoc/mongoc.h>
#include <bson/bson.h>

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

// serialized ermiry credentials
typedef struct SErmiryAuth {

    SStringS email;
    SStringS password;

} SErmiryAuth;

// authenticates an ermiry user with its email and password
// 23/06/2019 -- 23:22 -- email is unique, but w ewant to have duplicate usernames with a unique id
extern u8 ermiry_authenticate_method (void *auth_data_ptr);

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