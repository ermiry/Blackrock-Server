#ifndef __MODELS_H__
#define __MODELS_H__

#include <mongoc/mongoc.h>
#include <bson/bson.h>

#include "types/myTypes.h"

#include "collections/dllist.h"

// FIXME: we need to add a reference to our blackrock account
// TODO: what happens when we add more games or projects?
typedef struct User {

    bson_oid_t oid;

    char *name;
    char *email;
    char *username;
    char *password;
    struct tm *memberSince;
    struct tm *lastTime;
    char *location;

    DoubleList *friends;

} User;

#pragma region BLACKROCK

// FIXME: move this from here!
#define SERIALIZE_BUFF_SIZE             64
#define EXT_SERIALIZE_BUFF_SIZE         128

typedef enum BlackGuildType {

    GUILD_TYPE_OPEN = 0,
    GUILD_TYPE_INVITE = 1,
    GUILD_TYPE_CLOSED = 2

} BlackGuildType;

typedef struct BlackGuild {

    bson_oid_t oid;

    // guild description
    // FIXME: add a badge
    char *name;
    char *description;
    u32 trophies;
    // Mexico (MX)
    char *location;             // FIXME: how can we better select this?
    struct tm *creation_date;

    // conditions to enter
    BlackGuildType type;                 // inivite only, open, closed
    u32 required_trophies;      

    // members
    User *leader;
    DoubleList *members;

} BlackGuild;

// Serialized Black Guild data
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

// we should just send the oid of the achievemnt, the game client has the info
typedef struct Achievement {

    bson_oid_t oid;

    char *name;
    char *description;
    struct tm date;
    char *img;

} Achievement;

typedef struct BlackPVPStats {

    int totalKills;

    int wins_death_match;
    int wins_free;
    int wins_koth;

} BlackPVPStats;

typedef struct GameStats {

    int arcade_kills, arcade_bestScore, arcade_bestTime;
    int horde_kills, horde_bestScore, horde_bestTime;

} GameStats;

typedef struct BlackPVEStats {

    GameStats multi_stats;
    GameStats solo_stats;
    
} BlackPVEStats;

typedef struct BlackProfile {

    bson_oid_t oid;

    User *user;     // the user this profile is related to

    struct tm datePurchased;
    struct tm lastTime;
    int timePlayed;

    int trophies;

    char *guild;

    // TODO: what about an array of oids?
    Achievement **achievements;

    BlackPVPStats pvpStats;
    BlackPVEStats pveStats;

} BlackProfile;

#pragma endregion

#endif