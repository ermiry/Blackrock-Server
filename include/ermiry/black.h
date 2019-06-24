#ifndef _ERMIRY_BLACK_H_
#define _ERMIRY_BLACK_H_

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "mongo/mongo.h"

/*** Black Profiles ***/

#define BLACK_COLL_NAME         "blackrock"

extern mongoc_collection_t *black_collection;

typedef struct BlackPVPStats {

    i32 totalKills;

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
extern void black_profile_destroy (BlackProfile *profile);

extern BlackProfile *black_profile_get_by_oid (const bson_oid_t *oid, bool populate);
extern BlackProfile *black_profile_get_by_user (const bson_oid_t *user_oid, bool populate);

extern int black_profile_update_with_model (const bson_oid_t *profile_oid, const BlackProfile *black_profile);

/*** Black Guilds ***/

#define BLACK_GUILD_COLL_NAME       "blackguild"

extern mongoc_collection_t *black_guild_collection;

typedef enum BlackGuildType {

    GUILD_TYPE_OPEN = 0,
    GUILD_TYPE_INVITE = 1,
    GUILD_TYPE_CLOSED = 2

} BlackGuildType;

typedef struct BlackGuild {

    bson_oid_t oid;

    // guild description
    String *name;
    String *unique_id;
    String *description;
    String *badge;
    String *location;
    struct tm *creation_date;

    // conditions to enter
    BlackGuildType type;                 // inivite only, open, closed
    u32 required_trophies;      

    // members
    User *leader;
    DoubleList *members;

} BlackGuild;

extern BlackGuild *black_guild_new (void);
extern void black_guild_destroy (BlackGuild *guild);

extern BlackGuild *black_guild_get_by_oid (const bson_oid_t *guild_oid, bool populate);
extern BlackGuild *black_guild_get_by_name (const char *guild_name, bool populate);

/*** Black Achievements ***/

// FIXME:
// we should just send the oid of the achievemnt, the game client has the info
typedef struct BlackAchievement {

    bson_oid_t oid;

    char *name;
    char *description;
    struct tm *date;
    char *img;

} BlackAchievement;

/*** Serialization ***/

// serializd black guild
typedef struct S_BlackGuild {

    char guild_oid[SERIALIZE_BUFF_SIZE];

    char name[SERIALIZE_BUFF_SIZE];
    char description[SERIALIZE_BUFF_SIZE];
    u32 trophies;
    // TODO: maybe have an enum for this?
    // Mexico (MX)
    char location[SERIALIZE_BUFF_SIZE];             // FIXME: how can we better select this?
    struct tm creation_date;

    // conditions to enter
    BlackGuildType type;            // inivite only, open, closed
    u32 required_trophies;

    // FIXME: change for a serialized user reference
    // members
    User *leader;   
    u32 n_members;
    User **members;              // FIXME: how do we want to get members?
                                // by ermiry user or by black profile?

} S_BlackGuild;

#endif