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
        /* char *black_str = bson_as_canonical_extended_json (black_doc, NULL);
        if (black_str) {
            // TODO: parse the bson into our c model
            
        } */

        // FIXME: 18/03/2019 -- try using bson iter to retrive the data from bson
        // bson_iter_init
        /* bson_t *b;
        bson_iter_t iter;

        if ((b = bson_new_from_data (my_data, my_data_len))) {
            if (bson_iter_init (&iter, b)) {
                while (bson_iter_next (&iter)) {
                    printf ("Found element key: \"%s\"\n", bson_iter_key (&iter));
                }
            }
            bson_destroy (b);
        } */

        bson_destroy (black_doc);
    }

    return profile;

}

#pragma endregion

#pragma region Black Guilds

/*** GUILDS ***/

// create a guild
// join a black guild
// leave a black guild
// the leader can be able to edit the guild from blackrock -> such as in brawl
    // logic may vary depending of the rank of the player
// search for guilds and return possible outcomes
// request to join a guild
    // only for guilds of certain access restrictions
// send guild messages

// invite a friend to a guild

#define BLACK_GUILD_COLL_NAME       "blackguild"
static mongoc_collection_t *black_guild_collection = NULL;

static BlackGuild *black_guild_new (void) {

    BlackGuild *guild = (BlackGuild *) malloc (sizeof (BlackGuild));
    if (guild) memset (guild, 0, sizeof (BlackGuild));
    return guild;

}

static void black_guild_destroy (BlackGuild *guild) {

    if (guild) {
        if (guild->name) free (guild->name);
        if (guild->description) free (guild->description);
        if (guild->creation_date) free (guild->creation_date);
        // FIXME: location?

        free (guild);
    }

}

static bson_t *black_guild_bson_create (BlackGuild *guild) {

    bson_t *doc = NULL;

    if (guild) {
        doc = bson_new ();

        bson_oid_init (&guild->oid, NULL);
        bson_append_oid (doc, "_id", -1, &guild->oid);

        // common guild info
        bson_append_utf8 (doc, "name", -1, guild->name, -1);
        bson_append_utf8 (doc, "description", -1, guild->description, -1);
        bson_append_utf8 (doc, "trophies", -1, createString ("%i", guild->trophies), -1);
        bson_append_utf8 (doc, "location", -1, guild->location, -1);
        bson_append_date_time (doc, "creationDate", -1, mktime (guild->creation_date) * 1000);

        // requirements
        switch (guild->type) {
            case GUILD_TYPE_OPEN: bson_append_utf8 (doc, "type", -1, "open", -1); break;
            case GUILD_TYPE_INVITE: bson_append_utf8 (doc, "type", -1, "invite", -1); break;
            case GUILD_TYPE_CLOSED: bson_append_utf8 (doc, "type", -1, "closed", -1); break;
        }
        bson_append_utf8 (doc, "requiredTrophies", -1, createString ("%i", guild->required_trophies), -1);

        // members
        bson_append_oid (doc, "leader", -1, &guild->leader->oid);

        char buf[16];
        const char *key;
        size_t keylen;
        bson_t members_array;
        bson_append_array_begin (doc, "members", -1, &members_array);
        for (int i = 0; i < guild->n_members; i++) {
            keylen = bson_uint32_to_string (i, &key, buf, sizeof (buf));
            bson_append_oid (&members_array, key, (int) keylen, &guild->members[i]->oid);
        }
        bson_append_array_end (doc, &members_array);
    }

}

// transform a serialized guild into our local models
static BlackGuild *black_guild_deserialize (S_BlackGuild *s_guild) {

    BlackGuild *guild = NULL;

    if (s_guild) {
        guild = black_guild_new ();
        if (guild) {
            guild->name = (char *) calloc (strlen (s_guild->name + 1), sizeof (char));
            strcpy (guild->name, s_guild->name);
            guild->description = (char *) calloc (strlen (s_guild->description + 1), sizeof (char));
            strcpy (guild->description, s_guild->description);
            guild->trophies = guild->trophies;
            guild->location = (char *) calloc (strlen (s_guild->location + 1), sizeof (char));
            strcpy (guild->location, s_guild->location);

            time_t rawtime;
            struct tm *timeinfo;
            time (&rawtime);
            guild->creation_date = gmtime (rawtime);

            guild->type = s_guild->type;
            guild->required_trophies = s_guild->required_trophies;

            guild->n_members = 1;
            guild->leader = user_new ();
            guild->leader->oid = s_guild->leader->oid;
            guild->members = (User **) calloc (guild->n_members, sizeof (User *));
            guild->members[0] = user_new ();
            guild->members[0]->oid = guild->leader->oid;
        }
    }

    return guild;

}

// a user creates a new guild based on the guild data he sent
int black_guild_create (S_BlackGuild *s_guild) {

    int retval = 1;

    if (s_guild) {
        BlackGuild *guild = black_guild_deserialize (s_guild);
        if (guild) {
            // create the guild bson
            bson_t *guild_doc = black_guild_bson_create (guild);
            if (guild_doc) {
                // insert the bson into the db
                if (!mongo_insert_document (black_guild_collection, guild_doc)) {
                    // update the s_guild for return 
                    bson_oid_to_string (&guild->oid, s_guild->guild_oid); 
                    memcpy (&s_guild->creation_date, guild->creation_date, sizeof (struct tm));

                    // delete our temp data
                    bson_destroy (guild_doc);
                    black_guild_destroy (guild);

                    retval = 0;
                }

                else {
                    #ifdef ERMIRY_DEBUG
                    logMsg (stderr, ERROR, DEBUG_MSG, "Failed to insert black guild into mongo!");
                    #endif
                }
            }
        }
    }

    return retval;

}

int black_guild_update () {}

// add a user to a black guild
int black_guild_add () {}

#pragma endregion

#pragma region Public

// init ermiry data & processes
int ermiry_init (void) {

    int errors = 0;

    // TODO: do we need to pass the username and the db?
    if (!mongo_connect ()) {
        // open handle to user collection
        user_collection = mongoc_client_get_collection (mongo_client, db_name, USERS_COLL_NAME);
        if (!user_collection) {
            logMsg (stderr, ERROR, NO_TYPE, "Failed to get handle to users collection!");
            errors = 1;
        }
        
        // open handle to blackrock profile collection
        black_collection = mongoc_client_get_collection (mongo_client, db_name, BLACK_COLL_NAME);
        if (!black_collection) {
            logMsg (stderr, ERROR, NO_TYPE, "Failed to get handle to black collection!");
            errors = 1;
        }

        // open handle to black guild collection
        black_guild_collection = mongoc_client_get_collection (mongo_client, db_name, BLACK_GUILD_COLL_NAME);
        if (!black_guild_collection) {
            logMsg (stderr, ERROR, NO_TYPE, "Failed to get handle to black guild collection!");
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
    if (black_guild_collection) mongoc_collection_destroy (black_guild_collection);

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

// we know that a user exists, so get the associated black profile
BlackProfile *ermiry_black_profile_get (const bson_oid_t user_oid, int *errors) {

    BlackProfile *profile = black_profile_get (user_oid);
    if (!profile) *errors = PROFILE_NOT_FOUND;

    return profile;

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

#pragma endregion