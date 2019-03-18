#ifndef __ERMIRY_H__
#define __ERMIRY_H__

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

#endif