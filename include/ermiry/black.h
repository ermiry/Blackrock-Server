#ifndef _BLACK_H_
#define _BLACK_H_

#include "ermiry/models.h"

#include "mongo/mongo.h"

/*** Black Profiles ***/

#define BLACK_COLL_NAME         "blackrock"

extern mongoc_collection_t *black_collection;

/*** Black Guilds ***/

#define BLACK_GUILD_COLL_NAME       "blackguild"

extern mongoc_collection_t *black_guild_collection;

extern BlackGuild *black_guild_new (void);
extern void black_guild_destroy (BlackGuild *guild);

#endif