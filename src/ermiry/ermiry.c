#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ermiry/ermiry.h"
#include "ermiry/users.h"
#include "ermiry/black/profile.h"
#include "ermiry/black/guild.h"

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
        black_profile_collection = mongoc_client_get_collection (mongo_client, db_name, BLACK_COLL_NAME);
        if (!black_profile_collection) {
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
    if (black_profile_collection) mongoc_collection_destroy (black_profile_collection);
    if (black_guild_collection) mongoc_collection_destroy (black_guild_collection);

    mongo_disconnect ();

}

// authenticates an ermiry user with its email and password
// 23/06/2019 -- 23:22 -- email is unique, but w ewant to have duplicate usernames with a unique id
u8 ermiry_authenticate_method (void *auth_packet_ptr) {

    u8 retval = 1;

    if (auth_packet_ptr) {
        AuthPacket *auth_packet = (AuthPacket *) auth_packet_ptr;
        Packet *packet = auth_packet->packet;
        AuthData *auth_data = auth_packet->auth_data;

        if (auth_data->auth_data_size >= sizeof (SErmiryAuth)) {
            SErmiryAuth *ermiry_auth = (SErmiryAuth *) auth_data->auth_data;

            User *user = user_authenticate (packet, ermiry_auth);
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