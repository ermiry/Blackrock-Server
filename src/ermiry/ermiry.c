#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ermiry/ermiry.h"
#include "ermiry/users.h"
#include "ermiry/black.h"

#include "mongo/mongo.h"

#include "cerver/collections/dllist.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

const char *uri_string = "mongodb://localhost:27017";
const char *db_name = "ermiry";

// init ermiry data & processes
int ermiry_init (void) {

    int errors = 0;

    // TODO: do we need to pass the username and the db?
    if (!mongo_connect ()) {
        // open handle to user collection
        users_collection = mongoc_client_get_collection (mongo_client, db_name, USERS_COLL_NAME);
        if (!users_collection) {
            cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to get handle to users collection!");
            errors = 1;
        }
        
        // open handle to blackrock profile collection
        black_collection = mongoc_client_get_collection (mongo_client, db_name, BLACK_COLL_NAME);
        if (!black_collection) {
            cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to get handle to black collection!");
            errors = 1;
        }

        // open handle to black guild collection
        black_guild_collection = mongoc_client_get_collection (mongo_client, db_name, BLACK_GUILD_COLL_NAME);
        if (!black_guild_collection) {
            cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to get handle to black guild collection!");
            errors = 1;
        }
    }

    else {
        cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to connect to mongo");
        errors = 1;
    }

    return errors;  

}

// clean up ermiry data
int ermiry_end (void) {

    // close our collections handles
    if (users_collection) mongoc_collection_destroy (users_collection);
    if (black_collection) mongoc_collection_destroy (black_collection);
    if (black_guild_collection) mongoc_collection_destroy (black_guild_collection);

    mongo_disconnect ();

}

// search for a user with the given username
// if we find one, check if the password match
User *ermiry_user_get (const char *username, const char *password, int *errors) {

    // FIXME:

    /* User *user = NULL;

    if (username && password) {
        user = user_get (username);
        if (user) {
            // check that the password is correct
            if (!strcmp (password, user->password)) *errors = NO_ERRORS;
            else {
                // password is incorrect
                *errors = WRONG_PASSWORD;
                user_destroy (user);
                user = NULL;
            }
        }

        else {
            #ifdef ERMIRY_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_DEBUG, 
                    c_string_create ("Not user find with username: %s", username));
            #endif
            *errors = NOT_USER_FOUND;
        }
    }

    else *errors = SERVER_ERROR;

    return user; */

}

// FIXME:
// we know that a user exists, so get the associated black profile
BlackProfile *ermiry_black_profile_get (const bson_oid_t user_oid, int *errors) {

    // BlackProfile *profile = black_profile_get (user_oid);
    // if (!profile) *errors = PROFILE_NOT_FOUND;

    // return profile;

}

/*** TODO: add public funcs to... ***/

/*** PLAYERS / PROFILES ***/

// update values in the db
    // for example best scores or player stats
// create a new black profile when someone first login in the blackrock game
    // how do we want to control security?
// how can we save a battle log??

/*** FRIENDS ***/

// add a friend from blackrcok
// remove a friend from blackrock
// how do we want to see friend activity?
// send private messages
// invite a friend to join a game