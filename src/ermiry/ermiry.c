#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ermiry/ermiry.h"

#include "mongo/mongo.h"

#include "utils/myUtils.h"
#include "utils/log.h"
#include "utils/jsmn.h"

// FIXME: 
const char *uri_string = "mongodb://localhost:27017";
const char *db_name = "test";

#pragma region Users

#define USERS_COLL_NAME         "users"
static mongoc_collection_t *user_collection = NULL;

static User *user_new (void) {

    User *user = (User *) malloc (sizeof (User));
    if (user) memset (user, 0, sizeof (User));
    return user;

}

// FIXME: destroy the list of friends
static void user_destroy (User *user) {

    if (user) {
        if (user->name) free (user->name);
        if (user->email) free (user->email);
        if (user->username) free (user->username);
        if (user->password) free (user->password);
        if (user->location) free (user->location);

        if (user->memberSince) free (user->memberSince);
        if (user->lastTime) free (user->lastTime);

        free (user);
    }

}

// FIXME: where do we free the json string??
// FIXME: get dates
// parse a user json and returns a user structure
// has the option to populate friends user structures
static User *user_json_parse (char *user_json, bool populate) {

    User *user = NULL;

    if (user_json) {
        user = user_new ();

        jsmntok_t t[128];   // FIXME: is this true? We expect no more than 128 tokens

        jsmn_parser p;
        jsmn_init (&p);
        
        int ret = jsmn_parse (&p, user_json, strlen (user_json), t, sizeof (t) / sizeof (t[0]));
        if (ret < 0) {
            logMsg (stderr, ERROR, NO_TYPE, "Failed to pasre user JSON!");
            return NULL;
        }

        // Assume the top-level element is an object
        if (ret < 1 || t[0].type != JSMN_OBJECT) {
            logMsg (stderr, ERROR, NO_TYPE, "User JSON Object expected!");
            return NULL;
        }

        for (int i = 0; i < ret; i++) {
            if (jsoneq (user_json, &t[i], "$oid") == 0) {
                char *oid_str = createString ("%.*s\n", t[i+1].end-t[i+1].start, user_json + t[i+1].start);
                bson_oid_init_from_string (&user->oid, oid_str);
                i++;
            }

            else if (jsoneq (user_json, &t[i], "name") == 0) {
                user->name = createString ("%.*s", t[i+1].end-t[i+1].start, user_json + t[i+1].start);
                i++;
            }

            else if (jsoneq (user_json, &t[i], "email") == 0) {
                user->email = createString ("%.*s", t[i+1].end-t[i+1].start, user_json + t[i+1].start);
                i++;
            }

            else if (jsoneq (user_json, &t[i], "password") == 0) {
                user->password = createString ("%.*s", t[i+1].end-t[i+1].start, user_json + t[i+1].start);
                i++;
            }

            else if (jsoneq (user_json, &t[i], "username") == 0) {
                user->username = createString ("%.*s", t[i+1].end-t[i+1].start, user_json + t[i+1].start);
                i++;
            }

            // parse actions array
            /* else if (jsoneq (role_json, &t[i], "actions") == 0) {
        		if (t[i+1].type != JSMN_ARRAY) continue; 
  
                role->n_actions = t[i+1].size;
                role->actions = (char **) calloc (role->n_actions, sizeof (char *));

        		for (int j = 0; j < t[i+1].size; j++) {
        			jsmntok_t *g = &t[i+j+2];
                    role->actions[j] = createString ("%.*s\n", g->end - g->start, role_json + g->start);
        		}

        		i += t[i+1].size + 1;
        	} 

            else if (jsoneq (role_json, &t[i], "users") == 0) {
        		if (t[i+1].type != JSMN_ARRAY) continue; 
  
                // role->n_actions = t[i+1].size;
                // role->actions = (char **) calloc (role->n_actions, sizeof (char *));

        		for (int j = 0; j < t[i+1].size; j++) {
        			jsmntok_t *g = &t[i+j+2];
                    char *hola = createString ("%.*s\n", g->end - g->start, role_json + g->start);
                    printf ("%s\n", hola);
                    // role->actions[j] = createString ("%.*s\n", g->end - g->start, role_json + g->start);
        		}

        		i += t[i+1].size + 1;
        	}  */
            
            // else {
        	// 	printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
        	// 			role_json + t[i].start);
        	// }
        }
    }

    return user;

}

// get a user doc from the db by username
static bson_t *user_find (mongoc_collection_t *user_collection, const char *username) {

    if (user_collection) {
        // get the desired user
        bson_t *user_query = bson_new ();
        bson_append_utf8 (user_query, "username", -1, username, -1);

        return (bson_t *) mongo_find_one (user_collection, user_query);
    }

    else logMsg (stderr, ERROR, NO_TYPE, "Failed to get handle to user collection!");

    return NULL;

}

static User *user_get (const char *username) {

    User *user = NULL;

    bson_t *user_doc = user_find (user_collection, username);
    if (user_doc) {
        char *user_str = bson_as_canonical_extended_json (user_doc, NULL);
        if (user_str) {
            user = user_json_parse (user_str, false);
            free (user_str);
        }

        bson_destroy (user_doc);
    }

    return user;

}

#pragma endregion

#pragma region Blackrock 

#define BLACK_COLL_NAME         "blackrock"
static mongoc_collection_t *black_collection = NULL;

static BlackProfile *black_profile_new (void) {

    BlackProfile *profile = (BlackProfile *) malloc (sizeof (BlackProfile));
    if (profile) memset (profile, 0, sizeof (BlackProfile));
    return profile;

}

// FIXME: what happens with achievemnts??
static void black_profile_destroy (BlackProfile *profile) {

    if (profile) {
        if (profile->user) user_destroy (profile->user);
        if (profile->guild) free (profile->guild);

        free (profile);
    }

}

static bson_t *black_profile_find (mongoc_collection_t *black_collection, const bson_oid_t user_oid) {

    if (black_collection) {
        bson_t *black_query = bson_new ();
        bson_append_oid (black_query, "user", -1, &user_oid);

        return (bson_t *) mongo_find_one (black_collection, black_query);
    }

    else logMsg (stderr, ERROR, NO_TYPE, "Failed to get handle to black collection!");

    return NULL;

}

// get a blackrock associated with user 
static BlackProfile *black_profile_get (const bson_oid_t user_oid) {

    BlackProfile *profile = NULL;

    bson_t *black_doc = black_profile_find (black_collection, user_oid);
    if (black_doc) {
        char *black_str = bson_as_canonical_extended_json (black_doc, NULL);
        if (black_str) {
            // TODO: parse the bson into our c model
        }

        bson_destroy (black_doc);
    }

    return profile;

}

#pragma endregion

#pragma region Public

// init ermiry data & processes
int ermiry_init (void) {

    int errors = 0;

    // TODO: do we need to pass the username and the db?
    if (!mongo_connect ()) {
        // open handle to user collection
        user_collection = mongoc_client_get_collection (client, db_name, USERS_COLL_NAME);
        if (!user_collection) {
            logMsg (stderr, ERROR, NO_TYPE, "Failed to get handle to users collection!");
            errors = 1;
        }
        
        // open handle to blackrock profile collection
        black_collection = mongoc_client_get_collection (client, db_name, BLACK_COLL_NAME);
        if (!black_collection) {
            logMsg (stderr, ERROR, NO_TYPE, "Failed to get handle to black collection!");
            errors = 1;
        }
    }

    else {
        logMsg (stderr, ERROR, NO_TYPE, "Failed to connect to mongo");
        errors = 1;
    }

    return errors;  

}

// clean up ermiry data
int ermiry_end (void) {

    // close our collections handles
    if (user_collection) mongoc_collection_destroy (user_collection);
    if (black_collection) mongoc_collection_destroy (black_collection);

    mongo_disconnect ();

}

// search for a user with the given username
// if we find one, check if the password match
User *ermiry_user_get (const char *username, const char *password, int *errors) {

    User *user = NULL;

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
                logMsg (stderr, ERROR, DEBUG_MSG, 
                    createString ("Not user find with username: %s", username));
            #endif
            *errors = NOT_USER_FOUND;
        }
    }

    else *errors = SERVER_ERROR;

    return user;

}

BlackProfile *ermiry_black_profile_get (const char *username, const char *password, int *errors) {

    BlackProfile *profile = NULL;

    if (username && password) {
        User *user = ermiry_user_get (username, password, errors);
        if (user) {
            // we have got a valid user, so find the associated black profile
            profile = black_profile_get ((const bson_oid_t) user->oid);
            if (!profile) *errors = PROFILE_NOT_FOUND;
        }
    }

    else *errors = SERVER_ERROR;

    return profile;

}

#pragma endregion