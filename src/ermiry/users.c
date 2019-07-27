#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "ermiry/errors.h"
#include "ermiry/ermiry.h"
#include "ermiry/users.h"
#include "ermiry/black/profile.h"

#include "mongo/mongo.h"

#include "cerver/errors.h"

#include "cerver/collections/dllist.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

mongoc_collection_t *users_collection = NULL;

void user_delete (void *ptr);
u8 user_send (const User *user, const i32 sock_fd, Protocol protocol);
u8 user_send_inbox (const User *user, const i32 sock_fd, const Protocol protocol);
u8 user_send_requests (const User *user, const i32 sock_fd, const Protocol protocol);

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

// TODO: add unique id
// prints user values from a user structure
void user_print (User *user) {

    if (user) {
        printf ("User: \n");
        printf ("\tName: %s\n", user->name->str);
        printf ("\tEmail: %s\n", user->email->str);
        printf ("\tUsername: %s\n", user->username->str);
        printf ("\tPassword: %s\n", user->password->str);
        printf ("\tAvatar: %s\n", user->avatar->str);

        char date_buffer[80];
        strftime (date_buffer, 80, "%d/%m/%y - %T - GMT", user->member_since);
        printf ("\tMember since: %s\n", date_buffer);
        strftime (date_buffer, 80, "%d/%m/%y - %T - GMT", user->last_time);
        printf ("\tLast time on: %s\n", date_buffer);

        printf ("\tBio: %s\n", user->bio->str);
        printf ("\tLocation: %s\n", user->location->str);

        printf ("\tInbox: %s\n", user->inbox->str);
        printf ("\tRequests: %s\n", user->requests->str);
    }

}

// TODO: what about the other fileds that are in ermiry-website user model?
// FIXME: add friends!!
static bson_t *user_bson_create (User *user) {

    bson_t *doc = NULL;

    if (user) {
        doc = bson_new ();

        bson_oid_init (&user->oid, NULL);
        bson_append_oid (doc, "_id", 4, &user->oid);

        bson_append_utf8 (doc, "name", 5, user->name->str, user->name->len);
        bson_append_utf8 (doc, "email", 6, user->email->str, user->email->len);
        bson_append_utf8 (doc, "username", 9, user->username->str, user->username->len);
        bson_append_utf8 (doc, "password", 9, user->password->str, user->password->len);
        bson_append_utf8 (doc, "avatar", 7, user->avatar->str, user->avatar->len);

        bson_append_date_time (doc, "memberSince", 12, mktime (user->member_since) * 1000);
        bson_append_date_time (doc, "lastTime", 9, mktime (user->last_time) * 1000);

        bson_append_utf8 (doc, "bio", 4, user->bio->str, user->bio->len);
        bson_append_utf8 (doc, "location", 9, user->location->str, user->location->len);

        // TODO: add friends

        bson_append_utf8 (doc, "inbox", 6, user->inbox->str, user->inbox->len);
        bson_append_utf8 (doc, "requests", 9, user->requests->str, user->requests->len);

        // TODO: add achievements
    }

    return doc;

}

// TODO: add unique id
// parses a bson doc into a user model
User *user_doc_parse (const bson_t *user_doc, bool populate) {

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

                else if (!strcmp (key, "name") && value->value.v_utf8.str) 
                    user->name = str_new (value->value.v_utf8.str);

                else if (!strcmp (key, "email") && value->value.v_utf8.str) 
                    user->email = str_new (value->value.v_utf8.str);

                else if (!strcmp (key, "username") && value->value.v_utf8.str) 
                    user->username = str_new (value->value.v_utf8.str);

                else if (!strcmp (key, "password") && value->value.v_utf8.str) 
                    user->password = str_new (value->value.v_utf8.str);

                else if (!strcmp (key, "avatar") && value->value.v_utf8.str)
                    user->avatar = str_new (value->value.v_utf8.str);

                else if (!strcmp (key, "memberSince")) {
                    time_t secs = (time_t) bson_iter_date_time (&iter) / 1000;
                    memcpy (user->member_since, gmtime (&secs), sizeof (struct tm));
                }

                else if (!strcmp (key, "lastTime")) {
                    time_t secs = (time_t) bson_iter_date_time (&iter) / 1000;
                    memcpy (user->last_time, gmtime (&secs), sizeof (struct tm));
                }
                
                else if (!strcmp (key, "bio") && value->value.v_utf8.str)
                    user->bio = str_new (value->value.v_utf8.str);

                else if (!strcmp (key, "location") && value->value.v_utf8.str) 
                    user->location = str_new (value->value.v_utf8.str);

                // FIXME: iter array of friends!! and get how many they are
                else if (!strcmp (key, "friends")) {

                }

                else if (!strcmp (key, "inbox") && value->value.v_utf8.str) 
                    user->inbox = str_new (value->value.v_utf8.str);

                else if (!strcmp (key, "requests") && value->value.v_utf8.str) 
                    user->requests = str_new (value->value.v_utf8.str);

                // TODO: get array of achievements
                else if (!strcmp (key, "achievements")) {

                }

                else {
                    cerver_log_msg (stdout, LOG_WARNING, LOG_NO_TYPE, 
                        c_string_create ("Got unknown key %s when parsing user doc.", key));
                } 
            }
        }
    }

    return user;

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
        const bson_t *user_doc = user_find_by_email (email);
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

// searches a user by emaila nd authenticates it using the provided password
// on success, returns the user associated with the credentials
User *user_authenticate (const Packet *packet, const SErmiryAuth *ermiry_auth) {

    User *retval = NULL;

    if (ermiry_auth) {
        // get the user by email
        String *email = str_new (ermiry_auth->email.string);
        User *user = user_get_by_email (email, true);
        if (user) {
            // check for user password
            if (!strcmp (user->password->str, ermiry_auth->password.string)) {
                #ifdef ERMIRY_DEBUG
                cerver_log_msg (stdout, LOG_SUCCESS, LOG_NO_TYPE,
                    c_string_create ("Authenticated user with email %s.",
                    user->email));
                #endif

                // check that the user has a blackrock profile -- that has purchased the game
                BlackProfile *black_profile = black_profile_get_by_oid (&user->black_profile_oid, false);
                if (black_profile) {
                    user->black_profile = black_profile;

                    // serialize and send user 
                    user_send (user, packet->sock_fd, packet->cerver->protocol);

                    // send inbox file
                    user_send_inbox (user, packet->sock_fd, packet->cerver->protocol);

                    // send friends request file
                    user_send_requests (user, packet->sock_fd, packet->cerver->protocol);

                    // serialize and send blackrock profile
                    black_profile_send (black_profile, packet->sock_fd, packet->cerver->protocol);

                    retval = user;
                }

                else {
                    #ifdef ERMIRY_DEBUG
                    cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE,
                        c_string_create ("User with email %s does not have a black profile!",
                        user->email));
                    #endif
                    // send custom error packet with custom ermiry error packet
                    Packet *error_packet = error_packet_generate (ERR_NO_PROFILE, 
                        "No valid blackrock profile associated with your account.");
                    if (error_packet) {
                        packet_set_network_values (error_packet, packet->sock_fd, packet->cerver->protocol);
                        packet_send (error_packet, 0, NULL);
                        packet_delete (error_packet);
                    }
                }
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

        str_delete (email);
    }

    return retval;

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

/*** user packets ***/

// serializes a user and sends it to the client
u8 user_send (const User *user, const i32 sock_fd, const Protocol protocol) {

    u8 retval = 1;

    if (user) {
        SUser *suser = user_serialize (user);
        if (suser) {
            Packet *user_packet = packet_generate_request (APP_PACKET, ERMIRY_USER, suser, sizeof (SUser));
            if (user_packet) {
                packet_set_network_values (user_packet, sock_fd, protocol);
                retval = packet_send (user_packet, 0, NULL);
                packet_delete (user_packet);
            }

            else {
                #ifdef ERMIRY_DEBUG
                cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to generate user packet!");
                #endif
            }
        }

        else {
            #ifdef ERMIRY_DEBUG
            cerver_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to serialize user!");
            #endif
        }
    }

    return retval;

}

// send user's inbox file
u8 user_send_inbox (const User *user, const i32 sock_fd, const Protocol protocol) {

    // TODO:

}

// send user's friend requests file
u8 user_send_requests (const User *user, const i32 sock_fd, const Protocol protocol) {

    // TODO:

}