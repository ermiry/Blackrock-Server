#ifndef _ERMIRY_BLACK_PROFILE_H_
#define _ERMIRY_BLACK_PROFILE_H_

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "mongo/mongo.h"

#define BLACK_COLL_NAME         "blackprofile"

extern mongoc_collection_t *black_profile_collection;

typedef struct BlackPVPStats {

    i32 total_kills;

    i32 wins_death_match;
    i32 wins_free;
    i32 wins_koth;

} BlackPVPStats;

typedef struct GameStats {

    i32 arcade_kills, arcade_bestScore;     // can be used as int64
    i32 arcade_bestTime;                    // stored as secs

    i32 horde_kills, horde_bestScore;       // can be used as int64
    i32 horde_bestTime;                     // stored as secs

} GameStats;

typedef struct BlackPVEStats {

    GameStats solo_stats;
    GameStats multi_stats;
    
} BlackPVEStats;

typedef struct BlackProfile {

    bson_oid_t oid;

    bson_oid_t user_oid;

    struct tm *date_purchased;
    struct tm *last_time;
    u64 time_played;                // stored in secs

    bson_oid_t guild_oid;

    DoubleList *achievements;

    BlackPVEStats *pve_stats;
    BlackPVPStats *pvp_stats;

} BlackProfile;

extern BlackProfile *black_profile_new (void);
extern void black_profile_delete (BlackProfile *profile);

// gets a black profile from the db by its oid
extern BlackProfile *black_profile_get_by_oid (const bson_oid_t *oid, bool populate);

#endif