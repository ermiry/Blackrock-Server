#ifndef _ERMIRY_BLACK_GUILD_H_
#define _ERMIRY_BLACK_GUILD_H_

#include "cerver/types/types.h"
#include "cerver/types/string.h"

#include "mongo/mongo.h"

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

// serializd black guild
typedef struct S_BlackGuild {

    // char guild_oid[SERIALIZE_BUFF_SIZE];

    // char name[SERIALIZE_BUFF_SIZE];
    // char description[SERIALIZE_BUFF_SIZE];
    // u32 trophies;
    // // TODO: maybe have an enum for this?
    // // Mexico (MX)
    // char location[SERIALIZE_BUFF_SIZE];             // FIXME: how can we better select this?
    // struct tm creation_date;

    // // conditions to enter
    // BlackGuildType type;            // inivite only, open, closed
    // u32 required_trophies;

    // // FIXME: change for a serialized user reference
    // // members
    // User *leader;   
    // u32 n_members;
    // User **members;              // FIXME: how do we want to get members?
    //                             // by ermiry user or by black profile?

} S_BlackGuild;

#endif