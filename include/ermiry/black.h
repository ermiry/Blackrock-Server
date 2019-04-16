#ifndef _BLACK_H_
#define _BLACK_H_

#include "ermiry/models.h"

#include "mongo/mongo.h"

/*** Black Profiles ***/

#define BLACK_COLL_NAME         "blackrock"

extern mongoc_collection_t *black_collection;

extern BlackProfile *black_profile_new (void);
extern void black_profile_destroy (BlackProfile *profile);

extern BlackProfile *black_profile_get_by_oid (const bson_t *oid, bool populate);
extern BlackProfile *black_profile_get_by_user (const bson_t *user_oid, bool populate);

extern int black_profile_update_with_model (const bson_t *profile_oid, const BlackProfile *black_profile);

/*** Black Guilds ***/

#define BLACK_GUILD_COLL_NAME       "blackguild"

extern mongoc_collection_t *black_guild_collection;

extern BlackGuild *black_guild_new (void);
extern void black_guild_destroy (BlackGuild *guild);

#endif