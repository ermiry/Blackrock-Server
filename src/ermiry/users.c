#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "ermiry/ermiry.h"
#include "ermiry/users.h"

#include "mongo/mongo.h"

#include "cerver/collections/dllist.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

mongoc_collection_t *users_collection = NULL;

void user_delete (void *ptr);

User *user_new (void) {

    User *user = (User *) malloc (sizeof (User));
    if (user) {
        memset (user, 0, sizeof (User));

        user->name = NULL;
        user->email = NULL;
        user->username = NULL;
        user->unique_id = NULL;
        user->password = NULL;
        user->avatar = NULL;

        user->member_since = NULL;
        user->last_time = NULL;

        user->bio = NULL;
        user->location = NULL;

        user->friends = dlist_init (user_delete, NULL);

        user->inbox = NULL;
        user->requests = NULL;

        // FIXME:
        user->achievements = dlist_init (NULL, NULL);
    }

}

void user_delete (void *ptr) {

    if (ptr) {
        User *user = (User *) ptr;

        str_delete (user->name);
        str_delete (user->email);
        str_delete (user->username);
        str_delete (user->unique_id);
        str_delete (user->password);
        str_delete (user->avatar);

        if (user->member_since) free (user->member_since);
        if (user->last_time) free (user->last_time);

        dlist_destroy (user->friends);

        str_delete (user->inbox);
        str_delete (user->requests);

        dlist_destroy (user->achievements);

        free (user);
    }

}

// compares users based on their username (a - z)
int user_comparator_by_username (const void *a, const void *b) {

    if (a && b) return str_compare (((User *) a)->username, ((User *) b)->username);

}

// get a user doc from the db by oid
static const bson_t *user_find_by_oid (const bson_oid_t *oid) {

    if (oid) {
        bson_t *user_query = bson_new ();
        bson_append_oid (user_query, "_id", 4, oid);

        return mongo_find_one (users_collection, user_query);
    }

    return NULL;

}

// gets a user doc from the db by its email
static const bson_t *user_find_by_email (const String *email) {

    if (email) {
        bson_t *user_query = bson_new ();
        bson_append_utf8 (user_query, "email", 6, email->str, email->len);

        return mongo_find_one (users_collection, user_query);
    }

}

// get a user doc from the db by username
static const bson_t *user_find_by_username (const String *username) {

    if (username) {
        bson_t *user_query = bson_new ();
        bson_append_utf8 (user_query, "username", 9, username->str, username->len);

        return mongo_find_one (users_collection, user_query);
    }

    return NULL;    

}

// gets a user from the db by an oid
User *user_get_by_oid (const bson_oid_t *oid, bool populate) {

    User *user = NULL;

    if (oid) {
        const bson_t *user_doc = user_find_by_oid (oid);
        if (user_doc)  {
            user = user_doc_parse (user_doc, populate);
            bson_destroy ((bson_t *) user_doc);
        }
    }

    return user;

}

// gets a user from the db by its email
User *user_get_by_email (const String *email, bool populate) {

    User *user = NULL;

    if (email) {
        const bson_t *user_doc = user_find_bemail_leny_email (email);
        if (user_doc) {
            user = user_doc_parse (user_doc, populate);
            bson_destroy ((bson_t *) user_doc);
        }
    }

    return user;

}

// gets a user form the db by its username
User *user_get_by_username (const String *username, bool populate) {

    User *user = NULL;

    if (username) {
        const bson_t *user_doc = user_find_by_username (username);
        if (user_doc) {
            user = user_doc_parse (user_doc, populate);
            bson_destroy ((bson_t *) user_doc);
        }
    }

    return user;

}

/*** serialization ***/

static inline SUser *suser_new (void) {

    SUser *suser = (SUser *) malloc (sizeof (SUser));
    if (suser) memset (suser, 0, sizeof (SUser));
    return suser;

}

static inline void suser_delete (SUser *suser) { if (suser) free (suser); }

// TODO: send avatar and friends and achievements, also inbox and requests
// serializes a user
static SUser *user_serialize (const User *user) {

    if (user) {
        SUser *suser = suser_new ();
        if (suser) {
            strncpy (suser->name.string, user->name->str, SS_SMALL);
            suser->name.len = user->name->len > SS_SMALL ? SS_SMALL : user->name->len;

            strncpy (suser->email.string, user->email->str, SS_SMALL);
            suser->email.len = user->name->len > SS_SMALL ? SS_SMALL : user->email->len;

            strncpy (suser->username.string, user->username->str, SS_SMALL);
            suser->username.len = user->username->len > SS_SMALL ? SS_SMALL : user->username->len;

            suser->member_since = mktime (user->member_since);
            suser->last_time = mktime (user->last_time);

            strncpy (suser->bio.string, user->bio->str, SS_LARGE);
            suser->bio.len = user->bio->len > SS_LARGE ? SS_LARGE : user->bio->len;

            strncpy (suser->location.string, user->location->str, SS_SMALL);
            suser->location.len = user->location->len > SS_SMALL ? SS_SMALL : user->location->len;  
        }
    }

    return NULL;

}