#ifndef __ERMIRY_H__
#define __ERMIRY_H__

#include <mongoc/mongoc.h>
#include <bson/bson.h>

// ermiry error codes
#define SERVER_ERROR        100
#define NO_ERRORS           0
#define NOT_USER_FOUND      1
#define WRONG_PASSWORD      2

// init ermiry processes
extern int ermiry_init (void);

typedef struct User {

    bson_oid_t oid;

    char *name;
    char *email;
    char *username;
    char *password;
    struct tm *memberSince;
    struct tm *lastTime;
    char *location;

    int n_friends;
    struct User **friends;

} User;

// search for a user with the given username
// if we find one, check if the password match
User *ermiry_user_get (const char *username, const char *password, int *errors);

#endif