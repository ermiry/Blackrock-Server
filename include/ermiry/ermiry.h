#ifndef __ERMIRY_H__
#define __ERMIRY_H__

#include <mongoc/mongoc.h>
#include <bson/bson.h>

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

#endif