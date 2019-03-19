#ifndef __MODELS_H__
#define __MODELS_H__

#include <mongoc/mongoc.h>
#include <bson/bson.h>

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

    int n_friends;
    struct User **friends;

} User;

#pragma region BLACKROCK

typedef struct BlackGuild {

    bson_oid_t oid;

    // FIXME: how can we create a club tag such as brawl?

    // guild description
    char *name;
    char *description;
    u32 trophies;
    // Mexico (MX)
    char *location;             // FIXME: how can we better select this?

    // conditions to enter
    char *type;                 // inivite only, open, closed
    u32 required_trophies;      

    // members
    bson_oid_t leader;          // TODO: reference to the guild leader
    // TODO: do we want ranks in the guilds?

    // TODO: store the members as an array of oids in the db
    u32 n_members;
    User *members;              // FIXME: how do we want to get members?
                                // by ermiry user or by black profile?

} BlackGuild;

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