#ifndef _ERMIRY_USERS_H_
#define _ERMIRY_USERS_H_

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "ermiry/ermiry.h"
#include "ermiry/black/profile.h"

#include "mongo/mongo.h"

#include "cerver/packets.h"

#include "cerver/collections/dllist.h"

#define USERS_COLL_NAME         "users"

extern mongoc_collection_t *users_collection;

// this is how we manage a user in blackrock
typedef struct User {

    bson_oid_t oid;

    String *name;
    String *email;
    String *username;               // useful to have duplicate users
    String *unique_id;
    String *password;
    String *avatar;                 // avatar filename

    struct tm *member_since;
    struct tm *last_time;

    String *bio;
    String *location;

    // TODO: maybe change this to an array of oids?
    DoubleList *friends;

    String *inbox;                  // inbox filename
    String *requests;               // friends requests filename

    // TODO: maybe change this to an array of oids?
    DoubleList *achievements;       // ermiry achievements

    // TODO: correctly delete this!
    // black rock data
    bson_oid_t black_profile_oid;
    BlackProfile *black_profile;

} User;

extern User *user_new (void);
extern void user_delete (void *ptr);

// parses a bson doc into a user model
extern User *user_doc_parse (const bson_t *user_doc, bool populate);

// gets a user from the db by an oid
extern User *user_get_by_oid (const bson_oid_t *oid, bool populate);

// gets a user from the db by its email
extern User *user_get_by_email (const String *email, bool populate);

// gets a user form the db by its username
extern User *user_get_by_username (const String *username, bool populate);

// searches a user by emaila nd authenticates it using the provided password
// on success, returns the user associated with the credentials
extern User *user_authenticate (const Packet *packet, const SErmiryAuth *ermiry_auth);

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

// serializes a user and sends it to the client
extern u8 user_send (const User *user, const i32 sock_fd, const Protocol protocol);

#endif