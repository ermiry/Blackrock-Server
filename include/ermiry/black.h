#ifndef _BLACK_H_
#define _BLACK_H_

#include "ermiry/models.h"

#include "mongo/mongo.h"

#define BLACK_COLL_NAME         "blackrock"

extern mongoc_collection_t *black_collection;

#define BLACK_GUILD_COLL_NAME       "blackguild"

extern mongoc_collection_t *black_guild_collection;

#endif