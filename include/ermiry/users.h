#ifndef _ERMIRY_USERS_H_
#define _ERMIRY_USERS_H_

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "mongo/mongo.h"

#include "cerver/collections/dllist.h"

#define USERS_COLL_NAME         "users"

// this is how we manage a user in blackrock
typedef struct User {

    String *name;
    String *email;
    String *username;

    struct tm *member_since;
    struct tm *last_time;

    String *bio;
    String *location;
    // TODO: avatar

    DoubleList *friends;

    // TODO: inbox
    // TODO: friend requests

    // TODO: ermiry achievements

} User;


extern mongoc_collection_t *users_collection;

extern User *user_new (void);
extern void user_destroy (void *data);

extern User *user_get_by_username (const char *username, bool populate);
extern User *user_get_by_oid (const bson_oid_t *oid, bool populate);

// FIXME: do we rather ant to change this to take user models instead?
// updates a user in the db with new values
extern int user_update_with_model (const char *username, User *user_update_model);

// FIXME: do we rather ant to change this to take user models instead?
extern int user_add_friend (const char *username, const char *friend_username);
extern int user_remove_friend (const char *username, const char *friend_username);

// serialized user
// this how we recieve the users from the cerver
typedef struct SUser {

    SStringS name;
    SStringS email;
    SStringS username;

    time_t member_since;
    time_t last_time;

    SStringL bio;
    SStringS location;
    // TODO: avatar

    // FIXME:
    // DoubleList *friends;

    // only for current auth user
    // TODO: inbox
    // TODO: friend requests

    // TODO: ermiry achievements

} SUser;

#endif