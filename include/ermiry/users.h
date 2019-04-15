#ifndef _USERS_H_
#define _USERS_H_

#include "ermiry/models.h"

#include "mongo/mongo.h"

#define USERS_COLL_NAME         "users"

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

#endif