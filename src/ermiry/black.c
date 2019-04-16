#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ermiry/ermiry.h"
#include "ermiry/models.h"
#include "ermiry/users.h"
#include "ermiry/black.h"

#include "mongo/mongo.h"

#include "collections/dllist.h"

#include "utils/myUtils.h"
#include "utils/log.h"

#pragma region Blackrock 

mongoc_collection_t *black_collection = NULL;

static Achievement *black_achievement_new (void) {

    Achievement *a = (Achievement *) malloc (sizeof (Achievement));
    if (a) {
        a->name = NULL;
        a->description = NULL;
        a->date = NULL;
        a->img = NULL;
    }

    return a;

}

static void black_achievement_destroy (void *ptr) {

    if (ptr) {
        Achievement *a = (Achievement *) ptr;
        if (a->name) free (a->name);
        if (a->description) free (a->description);
        if (a->date) free (a->date);
        if (a->img) free (a->img);

        free (a);
    }

}

static inline BlackPVEStats *black_pve_stats_new (void) {

    BlackPVEStats *pve_stats = (BlackPVEStats *) malloc (sizeof (BlackPVEStats));
    if (pve_stats) memset (pve_stats, 0, sizeof (BlackPVEStats));
    return pve_stats;

}

static inline void black_pve_stats_destroy (BlackPVEStats *pve_stats) { if (pve_stats) free (pve_stats); }

static inline BlackPVPStats *black_pvp_stats_new (void) {

    BlackPVPStats *pvp_stats = (BlackPVPStats *) malloc (sizeof (BlackPVPStats));
    if (pvp_stats) memset (pvp_stats, 0, sizeof (BlackPVPStats));
    return pvp_stats;

}

static inline void black_pvp_stats_destroy (BlackPVPStats *pvp_stats) { if (pvp_stats) free (pvp_stats); }

static BlackProfile *black_profile_new (void) {

    BlackProfile *profile = (BlackProfile *) malloc (sizeof (BlackProfile));
    if (profile) {
        memset (profile, 0, sizeof (BlackProfile));

        profile->user = user_new ();

        profile->datePurchased = NULL;
        profile->lastTime = NULL;

        profile->guild = NULL;
        profile->achievements = dlist_init (black_achievement_destroy, NULL);

        profile->pveStats = black_pve_stats_new ();
        profile->pvpStats = black_pvp_stats_new ();
    } 

    return profile;

}

static void black_profile_destroy (BlackProfile *profile) {

    if (profile) {
        if (profile->user) user_destroy (profile->user);

        if (profile->datePurchased) free (profile->datePurchased);
        if (profile->lastTime) free (profile->lastTime);

        if (profile->guild) free (profile->guild);

        dlist_destroy (profile->achievements);

        free (profile);
    }

}

// get a black profile from the db by oid
static const bson_t *black_profile_find_by_oid (const bson_oid_t *oid) {

    if (oid) {
        bson_t *profile_query = bson_new ();
        bson_append_oid (profile_query, "_id", -1, oid);

        return mongo_find_one (black_collection, profile_query);
    }

    return NULL;

}

// get a black profile from the db by the associated user
static const bson_t *black_profile_find_by_user (const bson_oid_t *user_oid) {

    if (user_oid) {
        bson_t *profile_query = bson_new ();
        bson_append_oid (profile_query, "user", -1, user_oid);

        return mongo_find_one (black_collection, profile_query);
    }

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

mongoc_collection_t *black_guild_collection = NULL;

BlackGuild *black_guild_new (void) {

    BlackGuild *guild = (BlackGuild *) malloc (sizeof (BlackGuild));
    if (guild) {
        memset (guild, 0, sizeof (BlackGuild));
        guild->name = NULL;
        guild->description =  NULL;
        guild->location = NULL;
        guild->creation_date =  NULL;

        guild->leader = NULL;
        guild->members = dlist_init (user_destroy, NULL);
    } 

    return guild;

}

void black_guild_destroy (BlackGuild *guild) {

    if (guild) {
        if (guild->name) free (guild->name);
        if (guild->description) free (guild->description);
        if (guild->location) free (guild->location);
        if (guild->creation_date) free (guild->creation_date);

        user_destroy (guild->leader);
        dlist_destroy (guild->members);

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
        unsigned int i = 0;
        bson_append_array_begin (doc, "members", -1, &members_array);
        User *member = NULL;
        for (ListElement *le = LIST_START (guild->members); le; le = le->next) {
            member = (User *) le->data;
            keylen = bson_uint32_to_string (i, &key, buf, sizeof (buf));
            bson_append_oid (&members_array, key, (int) keylen, &member->oid);
            i++;
        }
        bson_append_array_end (doc, &members_array);
    }

}

// FIXME: parse trophies and any other value we need to fill our guild model
// parses a bson doc into a black guild model
static BlackGuild *black_guild_doc_parse (const bson_t *guild_doc, bool populate_users) {

    BlackGuild *guild =  NULL;

    if (guild_doc) {
        guild = black_guild_new ();

        bson_iter_t iter;
        bson_type_t type;

        if (bson_iter_init (&iter, guild_doc)) {
            while (bson_iter_next (&iter)) {
                const char *key = bson_iter_key (&iter);
                const bson_value_t *value = bson_iter_value (&iter);

                if (!strcmp (key, "_id")) {
                    bson_oid_copy (&value->value.v_oid, &guild->oid);
                    // const bson_oid_t *oid = bson_iter_oid (&iter);
                    // memcpy (&user->oid, oid, sizeof (bson_oid_t));
                }

                else if (!strcmp (key, "name") && value->value.v_utf8.str) {
                    guild->name = (char *) calloc (value->value.v_utf8.len + 1, sizeof (char));
                    strcpy (guild->name, value->value.v_utf8.str);
                }

                else if (!strcmp (key, "description") && value->value.v_utf8.str) {
                    guild->description = (char *) calloc (value->value.v_utf8.len + 1, sizeof (char));
                    strcpy (guild->description, value->value.v_utf8.str);
                }

                else if (!strcmp (key, "creationDate")) {
                    time_t secs = (time_t) bson_iter_date_time (&iter) / 1000;
                    memcpy (guild->creation_date, gmtime (&secs), sizeof (struct tm));
                }
                
                else if (!strcmp (key, "location")) {
                    if (value->value.v_utf8.str) {
                        if (!strcmp (value->value.v_utf8.str, " ")) {
                            guild->location = (char *) calloc (16, sizeof (char));
                            strcpy (guild->location, "No location");
                        }
                        
                        else {
                            guild->location = (char *) calloc (value->value.v_utf8.len + 1, sizeof (char));
                            strcpy (guild->location, value->value.v_utf8.str);
                        }
                    }
                }

                else if (!strcmp (key, "leader")) {
                    // const bson_oid_t *oid = bson_iter_oid (&iter);
                    const bson_oid_t *oid = &value->value.v_oid;
                    if (populate_users) guild->leader = user_get_by_oid (oid, false);
                    else {
                        guild->leader = user_new ();
                        bson_oid_copy (&value->value.v_oid, &guild->leader->oid);
                    } 
                }

                // FIXME: iter members array!! and get how many they are
                else if (!strcmp (key, "members")) {

                }

                else logMsg (stdout, WARNING, NO_TYPE, createString ("Got unknown key %s when parsing user doc.", key));
            }
        }
    }

    return guild;

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

            // FIXME: check mongo control panel!!
            // time_t rawtime;
            // struct tm *timeinfo;
            // time (&rawtime);
            // guild->creation_date = gmtime (rawtime);

            guild->type = s_guild->type;
            guild->required_trophies = s_guild->required_trophies;

            // FIXME:
            // guild->n_members = 1;
            // guild->leader = user_new ();
            // guild->leader->oid = s_guild->leader->oid;
            // guild->members = (User **) calloc (guild->n_members, sizeof (User *));
            // guild->members[0] = user_new ();
            // guild->members[0]->oid = guild->leader->oid;
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