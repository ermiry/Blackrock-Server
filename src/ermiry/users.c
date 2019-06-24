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

// FIXME: add friends!!
static bson_t *user_bson_create (User *user) {

    bson_t *doc = NULL;

    if (user) {
        doc = bson_new ();

        bson_oid_init (&user->oid, NULL);
        bson_append_oid (doc, "_id", -1, &user->oid);

        bson_append_utf8 (doc, "name", -1, user->name, -1);
        bson_append_utf8 (doc, "email", -1, user->email, -1);
        bson_append_utf8 (doc, "username", -1, user->username, -1);
        bson_append_utf8 (doc, "password", -1, user->password, -1);

        bson_append_date_time (doc, "memberSince", -1, mktime (user->memberSince) * 1000);
        bson_append_date_time (doc, "lastTime", -1, mktime (user->lastTime) * 1000);

        bson_append_utf8 (doc, "location", -1, user->location, -1);
    }

    return doc;

}

// FIXME: add friends!!
static bson_t *user_bson_create_update (User *user) {

    bson_t *doc = NULL;

    if (user) {
        doc = bson_new ();

        bson_t set_doc;
        BSON_APPEND_DOCUMENT_BEGIN (doc, "$set", &set_doc);

        bson_append_utf8 (&set_doc, "name", -1, user->name, -1);
        bson_append_utf8 (&set_doc, "email", -1, user->email, -1);
        bson_append_utf8 (&set_doc, "username", -1, user->username, -1);
        bson_append_utf8 (&set_doc, "password", -1, user->password, -1);

        bson_append_date_time (&set_doc, "memberSince", -1, mktime (user->memberSince) * 1000);
        bson_append_date_time (&set_doc, "lastTime", -1, mktime (user->lastTime) * 1000);

        if (user->location) bson_append_utf8 (&set_doc, "location", -1, user->location, -1);
        else bson_append_utf8 (&set_doc, "location", -1, "", -1);
        
        bson_append_document_end (doc, &set_doc); 
    }

    return doc;

}

// parses a bson doc into a user model
static User *user_doc_parse (const bson_t *user_doc, bool populate) {

    User *user = NULL;

    if (user_doc) {
        user = user_new ();

        bson_iter_t iter;
        bson_type_t type;

        if (bson_iter_init (&iter, user_doc)) {
            while (bson_iter_next (&iter)) {
                const char *key = bson_iter_key (&iter);
                const bson_value_t *value = bson_iter_value (&iter);

                if (!strcmp (key, "_id")) {
                    bson_oid_copy (&value->value.v_oid, &user->oid);
                    // const bson_oid_t *oid = bson_iter_oid (&iter);
                    // memcpy (&user->oid, oid, sizeof (bson_oid_t));
                }

                else if (!strcmp (key, "name") && value->value.v_utf8.str) {
                    user->name = (char *) calloc (value->value.v_utf8.len + 1, sizeof (char));
                    strcpy (user->name, value->value.v_utf8.str);
                }

                else if (!strcmp (key, "email") && value->value.v_utf8.str) {
                    user->email = (char *) calloc (value->value.v_utf8.len + 1, sizeof (char));
                    strcpy (user->email, value->value.v_utf8.str);
                }

                else if (!strcmp (key, "username") && value->value.v_utf8.str) {
                    user->username = (char *) calloc (value->value.v_utf8.len + 1, sizeof (char));
                    strcpy (user->username, value->value.v_utf8.str);
                } 

                else if (!strcmp (key, "password") && value->value.v_utf8.str) {
                    user->password = (char *) calloc (value->value.v_utf8.len + 1, sizeof (char));
                    strcpy (user->password, value->value.v_utf8.str);
                }

                else if (!strcmp (key, "memberSince")) {
                    time_t secs = (time_t) bson_iter_date_time (&iter) / 1000;
                    memcpy (user->memberSince, gmtime (&secs), sizeof (struct tm));
                }

                else if (!strcmp (key, "lastTime")) {
                    time_t secs = (time_t) bson_iter_date_time (&iter) / 1000;
                    memcpy (user->lastTime, gmtime (&secs), sizeof (struct tm));
                }
                
                else if (!strcmp (key, "location")) {
                    if (value->value.v_utf8.str) {
                        if (!strcmp (value->value.v_utf8.str, " ")) {
                            user->location = (char *) calloc (16, sizeof (char));
                            strcpy (user->location, "No location");
                        }
                        
                        else {
                            user->location = (char *) calloc (value->value.v_utf8.len + 1, sizeof (char));
                            strcpy (user->location, value->value.v_utf8.str);
                        }
                    }
                }

                // FIXME: iter array of friends!! and get how many they are
                else if (!strcmp (key, "friends")) {

                }

                else cerver_log_msg (stdout, LOG_WARNING, LOG_NO_TYPE, c_string_create ("Got unknown key %s when parsing user doc.", key));
            }
        }
    }

    return user;

}

// get a user doc from the db by oid
static const bson_t *user_find_by_oid (const bson_oid_t *oid) {

    if (oid) {
        bson_t *user_query = bson_new ();
        bson_append_oid (user_query, "_id", -1, oid);

        return mongo_find_one (users_collection, user_query);
    }

    return NULL;

}

// gets a user doc from the db by its email
static const bson_t *user_find_by_email (const char *email, unsigned int email_len) {

    if (email) {
        bson_t *user_query = bson_new ();
        bson_append_utf8 (user_query, "email", 6, email, email_len);

        return mongo_find_one (users_collection, user_query);
    }

}

// get a user doc from the db by username
static const bson_t *user_find_by_username (const char *username) {

    if (username) {
        bson_t *user_query = bson_new ();
        bson_append_utf8 (user_query, "username", 9, username, -1);

        return mongo_find_one (users_collection, user_query);
    }

    return NULL;    

}

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
User *user_get_by_email (const char *email, unsigned int email_len, bool populate) {

    User *user = NULL;

    if (email) {
        const bson_t *user_doc = user_find_by_email (email, email_len);
        if (user_doc) {
            user = user_doc_parse (user_doc, populate);
            bson_destroy ((bson_t *) user_doc);
        }
    }

    return user;

}

User *user_get_by_username (const char *username, bool populate) {

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

// updates a user in the db with new values
int user_update_with_model (const char *username, User *user_update_model) {

    int retval = 1;

    if (username && user_update_model) {
        // get the user by query (username)
        bson_t *user_query = bson_new ();
        bson_append_utf8 (user_query, "username", -1, username, -1);

        // create a bson with the update info
        bson_t *update = user_bson_create_update (user_update_model);

        retval = mongo_update_one (users_collection, user_query, update);
    }

    return retval;

}

/*** FRIENDS ***/

static User *user_get_friend_by_username (const User *user, const char *friend_username) {

    User *friend = NULL;
    for (ListElement *le = dlist_start (user->friends); le; le = le->next) {
        friend = (User *) le->data;
        if (!strcmp (friend->username, friend_username)) return friend;
    }

    return NULL;

}

static inline void user_add_friend_to_friend_list (const User *user, const User *friend) {

    if (user && friend) dlist_insert_after (user->friends, dlist_end (user->friends), (void *) friend);

}

static int user_remove_friend_from_friend_list (const User *user, const User *friend) {

    int retval = 1;

    if (user && friend) {
        User *f = NULL;
        for (ListElement *le = dlist_start (user->friends); le; le = le->next) {
            f = (User *) le->data;
            if (!strcmp (f->username, friend->username)) {
                dlist_remove_element (user->friends, le);
                retval = 0;
                break;
            }
        }
    }

    return retval;

}

// TODO: handle if an error occur in any user doc
int user_add_friend (const char *username, const char *friend_username) {

    int errors = 0;

    if (username && friend_username) {
        const bson_t *user_doc = user_find_by_username (username);
        User *user = user_doc_parse (user_doc, true);

        if (user) {
            const bson_t *friend_doc = user_find_by_username (friend_username);
            User *friend = user_doc_parse (friend_doc, true);

            if (friend) {
                if (!user_get_friend_by_username (user, friend_username) && !user_get_friend_by_username (friend, username)) {
                    // add each other to each friend list
                    user_add_friend_to_friend_list (user, friend);
                    user_add_friend_to_friend_list (friend, user);

                    // update each user in the db
                    errors = user_update_with_model (username, user);
                    errors = user_update_with_model (friend_username, friend);
                }

                else cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "One user already has the other in its friend list!");

                user_destroy (friend);
            }

            else cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, c_string_create ("Failed to get friend %s", friend_username));
            
            user_destroy (user);
        }

        else cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, c_string_create ("Failed to get user %s", username));
    }

    return errors;

}

// TODO: handle if an error occur in any user doc
int user_remove_friend (const char *username, const char *friend_username) {

    int errors = 0;

    if (username && friend_username) {
        const bson_t *user_doc = user_find_by_username (username);
        User *user = user_doc_parse (user_doc, true);

        if (user) {
            const bson_t *friend_doc = user_find_by_username (friend_username);
            User *friend = user_doc_parse (friend_doc, true);

            if (friend) {
                if (user_get_friend_by_username (user, friend_username) && user_get_friend_by_username (friend, username)) {
                    // remove each other from their friend list
                    errors = user_remove_friend_from_friend_list (user, friend);
                    errors = user_remove_friend_from_friend_list (friend, user);

                    if (!errors) {
                        errors = user_update_with_model (username, user);
                        errors = user_update_with_model (friend_username, friend);
                    }

                    else cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to remove user from friends list!");
                }

                else cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Both users dont have each other in ther friends list!");
                
                user_destroy (friend);
            }

            user_destroy (user);
        }
    }

    return errors;

}

/*** serialization ***/

static inline SUser *suser_new (void) {

    SUser *suser = (SUser *) malloc (sizeof (SUser));
    if (suser) memset (suser, 0, sizeof (SUser));
    return suser;

}

static inline void suser_delete (SUser *suser) { if (suser) free (suser); }

// FIXME:
// serializes a user
static SUser *user_serialize (const User *user) {

    if (user) {
        SUser *suser = suser_new ();
        if (suser) {
            
        }
    }

    return NULL;

}