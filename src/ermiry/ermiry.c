#include "ermiry/ermiry.h"

#include "mongo/mongo.h"

#include "utils/log.h"
#include "utils/jsmn.h"

// FIXME:
#define USERS_COLL_NAME         "users"
const char *uri_string = "mongodb://localhost:27017";
const char *db_name = "test";

// init ermiry processes
int ermiry_init (void) {

    int errors = 0;

    // TODO: do we need to pass the username and the db?
    if (mongo_connect ()) {
        logMsg (stderr, ERROR, NO_TYPE, "Failed to init ermiry!");
        errors = 1;
    }

    return errors;  

}

#pragma region Users

// FIXME: where do we free the json string??
// FIXME: get dates
// parse a user json and returns a user structure
// has the option to populate friends user structures
static User *user_json_parse (char *user_json, bool populate) {

    User *user = NULL;

    if (user_json) {
        user = (User *) malloc (sizeof (User));
        memset (user, 0, sizeof (User));

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
static bson_t *user_find (mongoc_collection_t *user_collection, const char *username, bool closeHanlde) {

    // open handle to user collection
    user_collection = mongoc_client_get_collection (client, db_name, USERS_COLL_NAME);
    if (user_collection) {
        // get the desired user
        bson_t *user_query = bson_new ();
        bson_append_utf8 (user_query, "username", -1, username, -1);

        bson_t *user_doc = (bson_t *) mongo_find_one (user_collection, user_query);
        if (closeHanlde) mongoc_collection_destroy (user_collection);
        return user_doc;
    }

    else logMsg (stderr, ERROR, NO_TYPE, "Failed to get handle to user collection!");

    return NULL;

}

static User *user_get (const char *username) {

    User *user = NULL;

    mongoc_collection_t *user_collection = NULL;
    bson_t *user_doc = user_find (user_collection, username, true);
    if (user_doc) {
        if (user) {
            char *user_str = bson_as_canonical_extended_json (user_doc, NULL);
            if (user_str) {
                user = user_json_parse (user_str, false);
                free (user_str);
            }
        }

        bson_destroy (user_doc);
    }

    return user;

}

// connect to my ermiry account
    // get user by username and password

#pragma endregion