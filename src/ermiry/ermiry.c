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

// TODO: move to user
// searches a user by emaila nd authenticates it using the provided password
// on success, returns the user associated with the credentials
static User *user_authenticate (SErmiryAuth *ermiry_auth) {

    User *retval = NULL;

    if (ermiry_auth) {
        // get the user by email
        User *user = user_get_by_email (ermiry_auth->email.string, ermiry_auth->email.len, true);
        if (user) {
            // check for user password
            if (!strcmp (user->password->str, ermiry_auth->password.string)) {
                #ifdef ERMIRY_DEBUG
                cerver_log_msg (stdout, LOG_SUCCESS, LOG_NO_TYPE,
                    c_string_create ("Authenticated user with email %s.",
                    user->email));
                #endif

                // FIXME: also check if the user has a valid blackrock profile!!
                retval = user;
            }

            else {
                #ifdef ERMIRY_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                    c_string_create ("Wrong password for user with email %s.",
                    user->email));
                #endif
            }
        }

        else {
            #ifdef ERMIRY_DEBUG
            cerver_log_msg (stderr, LOG_WARNING, LOG_NO_TYPE, 
                c_string_create ("Couldn't find a user with email %s.", 
                ermiry_auth->email.string));
            #endif
        }
    }

    return retval;

}

// authenticates an ermiry user with its email and password
// 23/06/2019 -- 23:22 -- email is unique, but w ewant to have duplicate usernames with a unique id
u8 ermiry_authenticate_method (void *auth_data_ptr) {

    u8 retval = 1;

    if (auth_data_ptr) {
        AuthData *auth_data = (AuthData *) auth_data_ptr;

        if (auth_data->auth_data_size >= sizeof (SErmiryAuth)) {
            SErmiryAuth *ermiry_auth = (SErmiryAuth *) auth_data->auth_data;

            User *user = user_authenticate (ermiry_auth);
            if (user) {
                auth_data->data = user;
                auth_data->delete_data = user_delete;

                retval = 0;
            }
        }

        else {
            #ifdef ERMIRY_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, 
                "Failed to authenticate ermiry user -- auth data to small!");
            #endif
        }
    }

    return retval;

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