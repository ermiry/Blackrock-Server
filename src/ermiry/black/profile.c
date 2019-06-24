#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ermiry/ermiry.h"
#include "ermiry/users.h"
#include "ermiry/black/profile.h"
#include "ermiry/black/guild.h"

#include "mongo/mongo.h"

#include "cerver/collections/dllist.h"

#include "cerver/utils/utils.h"
#include "cerver/utils/log.h"

mongoc_collection_t *black_collection = NULL;

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

BlackProfile *black_profile_new (void) {

    BlackProfile *profile = (BlackProfile *) malloc (sizeof (BlackProfile));
    if (profile) {
        memset (profile, 0, sizeof (BlackProfile));

        profile->datePurchased = NULL;
        profile->lastTime = NULL;

        profile->achievements = dlist_init (black_achievement_destroy, NULL);

        profile->pveStats = black_pve_stats_new ();
        profile->pvpStats = black_pvp_stats_new ();
    } 

    return profile;

}

void black_profile_destroy (BlackProfile *profile) {

    if (profile) {
        if (profile->datePurchased) free (profile->datePurchased);
        if (profile->lastTime) free (profile->lastTime);

        dlist_destroy (profile->achievements);

        black_pve_stats_destroy (profile->pveStats);
        black_pvp_stats_destroy (profile->pvpStats);

        free (profile);
    }

}

static void black_profile_bson_append_pve_stats (bson_t *doc, const BlackPVEStats *pve_stats) {

    if (doc && pve_stats) {
        bson_t *pve_stats_doc = bson_new ();  

        bson_t *solo_stats = bson_new ();
        bson_append_int64 (solo_stats, "arcadeKills", -1, pve_stats->solo_stats.arcade_kills);
        bson_append_int64 (solo_stats, "arcadeBestScore", -1, pve_stats->solo_stats.arcade_bestScore);
        bson_append_int64 (solo_stats, "arcadeBestTime", -1, pve_stats->solo_stats.arcade_bestTime);

        bson_append_int64 (solo_stats, "hordeKills", -1, pve_stats->solo_stats.horde_kills);
        bson_append_int64 (solo_stats, "hordeBestScore", -1, pve_stats->solo_stats.horde_bestScore);
        bson_append_int64 (solo_stats, "hordeBestTime", -1, pve_stats->solo_stats.horde_bestTime);
        bson_append_document (pve_stats_doc, "soloStats", -1, solo_stats);

        bson_t *multi_stats = bson_new ();
        bson_append_int64 (multi_stats, "arcadeKills", -1, pve_stats->multi_stats.arcade_kills);
        bson_append_int64 (multi_stats, "arcadeBestScore", -1, pve_stats->multi_stats.arcade_bestScore);
        bson_append_int64 (multi_stats, "arcadeBestTime", -1, pve_stats->multi_stats.arcade_bestTime);

        bson_append_int64 (multi_stats, "hordeKills", -1, pve_stats->multi_stats.horde_kills);
        bson_append_int64 (multi_stats, "hordeBestScore", -1, pve_stats->multi_stats.horde_bestScore);
        bson_append_int64 (multi_stats, "hordeBestTime", -1, pve_stats->multi_stats.horde_bestTime);
        bson_append_document (pve_stats_doc, "multiStats", -1, multi_stats);

        bson_append_document (doc, "pveStats", -1, pve_stats_doc);
    }

}

static void black_profile_bson_append_pvp_stats (bson_t *doc, const BlackPVPStats *pvp_stats) {

    if (doc && pvp_stats) {
        bson_t *pvp_stats_doc = bson_new ();

        bson_append_int32 (pvp_stats_doc, "totalKills", -1, pvp_stats->totalKills);

        bson_append_int32 (pvp_stats_doc, "winsDeathMatch", -1, pvp_stats->wins_death_match);
        bson_append_int32 (pvp_stats_doc, "winsFree", -1, pvp_stats->wins_free);
        bson_append_int32 (pvp_stats_doc, "winsKoth", -1, pvp_stats->wins_koth);

        bson_append_document (doc, "pveStats", -1, pvp_stats_doc);
    }

}

static bson_t *black_profile_bson_create (const BlackProfile *profile) {

    bson_t *doc = NULL;

    if (profile) {
        doc = bson_new ();

        bson_oid_init ((bson_oid_t *) &profile->oid, NULL);
        bson_append_oid (doc, "_id", -1, &profile->oid);

        bson_append_oid (doc, "user", -1, &profile->user_oid);

        bson_append_date_time (doc, "datePurchased", -1, mktime (profile->datePurchased) * 1000);
        bson_append_date_time (doc, "lastTime", -1, mktime (profile->lastTime) * 1000);
        bson_append_int64 (doc, "timePlayed", -1, profile->timePlayed);

        bson_append_int32 (doc, "trophies", -1, profile->trophies);

        bson_append_oid (doc, "guild", -1, &profile->guild_oid);

        char buf[16];
        const char *key = NULL;
        size_t keylen = 0;
        unsigned int i = 0;

        // append array of achievements oids
        bson_t achievements_array;
        bson_append_array_begin (doc, "achievements", -1, &achievements_array);
        Achievement *a = NULL;
        for (ListElement *le = dlist_start (profile->achievements); le; le = le->next) {
            a = (Achievement *) le->data;
            keylen = bson_uint32_to_string (i, &key, buf, sizeof (buf));
            bson_append_oid (&achievements_array, key, (int) keylen, &a->oid);
            i++;
        }
        bson_append_array_end (doc, &achievements_array);

        black_profile_bson_append_pve_stats (doc, profile->pveStats);
        black_profile_bson_append_pvp_stats (doc, profile->pvpStats);
    }

    return doc;

}

static int black_profile_parse_pve_stats (BlackProfile *profile, bson_t *pve_stats_doc) {

    int retval = 1;

    if (profile && pve_stats_doc) {
        bson_iter_t iter;
        bson_type_t type;

        if (bson_iter_init (&iter, pve_stats_doc)) {
            while (bson_iter_next (&iter)) {
                const char *key = bson_iter_key (&iter);
                const bson_value_t *value = bson_iter_value (&iter);

                if (!strcmp (key, "soloStats")) {
                    // TODO:
                }

                else if (!strcmp (key, "multiStats")) {
                    // TODO:
                }

                else cerver_log_msg (stdout, LOG_WARNING, LOG_NO_TYPE, c_string_create ("Got unknown key %s when parsing pvp stats doc.", key));
            }
        }

        bson_destroy (pve_stats_doc);

        return 0;
    }

    return retval;

}

static int black_profile_parse_pvp_stats (BlackProfile *profile, bson_t *pvp_stats_doc) {

    int retval = 1;

    if (profile && pvp_stats_doc) {
        bson_iter_t iter;
        bson_type_t type;

        if (bson_iter_init (&iter, pvp_stats_doc)) {
            while (bson_iter_next (&iter)) {
                const char *key = bson_iter_key (&iter);
                const bson_value_t *value = bson_iter_value (&iter);

                if (!strcmp (key, "totalKills")) 
                    profile->pvpStats->totalKills = value->value.v_int32;

                else if (!strcmp (key, "winsDeathMatch"))
                    profile->pvpStats->wins_death_match = value->value.v_int32;

                else if (!strcmp (key, "winsFree"))
                    profile->pvpStats->wins_free = value->value.v_int32;

                else if (!strcmp (key, "winsKoth"))
                    profile->pvpStats->wins_koth = value->value.v_int32;

                else cerver_log_msg (stdout, LOG_WARNING, LOG_NO_TYPE, c_string_create ("Got unknown key %s when parsing pvp stats doc.", key));
            }
        }

        bson_destroy (pvp_stats_doc);

        return 0;
    }

    return retval;

}

// parses a bson doc into a black profile model
static BlackProfile *black_profile_doc_parse (const bson_t *profile_doc, bool populate) {

    BlackProfile *profile = NULL;

    if (profile_doc) {
        profile = black_profile_new ();

        bson_iter_t iter;
        bson_type_t type;

        if (bson_iter_init (&iter, profile_doc)) {
            while (bson_iter_next (&iter)) {
                const char *key = bson_iter_key (&iter);
                const bson_value_t *value = bson_iter_value (&iter);

                if (!strcmp (key, "_id")) {
                    bson_oid_copy (&value->value.v_oid, &profile->oid);
                    // const bson_oid_t *oid = bson_iter_oid (&iter);
                    // memcpy (&user->oid, oid, sizeof (bson_oid_t));
                }

                else if (!strcmp (key, "user")) 
                    bson_oid_copy (&value->value.v_oid, &profile->user_oid);

                else if (!strcmp (key, "datePurchased")) {
                    time_t secs = (time_t) bson_iter_date_time (&iter) / 1000;
                    memcpy (profile->datePurchased, gmtime (&secs), sizeof (struct tm));
                }

                else if (!strcmp (key, "lastTime")) {
                    time_t secs = (time_t) bson_iter_date_time (&iter) / 1000;
                    memcpy (profile->lastTime, gmtime (&secs), sizeof (struct tm));
                }

                else if (!strcmp (key, "timePlayed")) 
                    profile->timePlayed = value->value.v_int64;

                else if (!strcmp (key, "trophies"))
                    profile->trophies = value->value.v_int32;

                else if (!strcmp (key, "guild")) 
                    bson_oid_copy (&value->value.v_oid, &profile->guild_oid);

                else if (!strcmp (key, "achievements")) {
                    // TODO:
                }

                else if (!strcmp (key, "pveStats")) {
                    bson_t *pve_stats_doc = bson_new_from_data (value->value.v_doc.data, value->value.v_doc.data_len);
                    black_profile_parse_pve_stats (profile, pve_stats_doc);
                }

                else if (!strcmp (key, "pvpStats")) {
                    bson_t *pvp_stats_doc = bson_new_from_data (value->value.v_doc.data, value->value.v_doc.data_len);
                    black_profile_parse_pvp_stats (profile, pvp_stats_doc);
                }

                else cerver_log_msg (stdout, LOG_WARNING, LOG_NO_TYPE, c_string_create ("Got unknown key %s when parsing black profile doc.", key));
            }
        }
    }

    return profile;

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

BlackProfile *black_profile_get_by_oid (const bson_oid_t *oid, bool populate) {

    BlackProfile *profile = NULL;

    if (oid) {
        const bson_t *profile_doc = black_profile_find_by_oid (oid);
        if (profile_doc) {
            profile = black_profile_doc_parse (profile_doc, populate);
            bson_destroy ((bson_t *) profile_doc);
        }
    }

    return profile;

}

BlackProfile *black_profile_get_by_user (const bson_oid_t *user_oid, bool populate) {

    BlackProfile *profile = NULL;

    if (user_oid) {
        const bson_t *profile_doc = black_profile_find_by_user (user_oid);
        if (profile_doc) {
            profile = black_profile_doc_parse (profile_doc, populate);
            bson_destroy ((bson_t *) profile_doc);
        }
    }

    return profile;

}

// TODO:
static bson_t *black_profile_bson_create_update (const BlackProfile *black_profile) {

    bson_t *doc = NULL;

    if (black_profile) {
        doc = bson_new ();

        bson_t set_doc;
        BSON_APPEND_DOCUMENT_BEGIN (doc, "$set", &set_doc);

        
        bson_append_document_end (doc, &set_doc); 
    }

    return doc;

}

// updates a black profile in the db with new values
int black_profile_update_with_model (const bson_oid_t *profile_oid, const BlackProfile *black_profile) {

    int retval = 1;

    if (profile_oid && black_profile) {
        bson_t *profile_query = bson_new ();
        bson_append_oid (profile_query, "_id", -1, profile_oid);

        // create a bson with the update info
        bson_t *update = black_profile_bson_create_update (black_profile);

        retval = mongo_update_one (black_collection, profile_query, update);
    }

    return retval;

}