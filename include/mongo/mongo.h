#ifndef _MONGO_H_
#define _MONGO_H_

#include <mongoc/mongoc.h>
#include <bson/bson.h>

extern void mongo_set_app_name (const char *name);

extern void mongo_set_uri (const char *uri);

extern void mongo_set_db_name (const char *name);

// ping the db to test for connection
// returns 0 on success, 1 on error
extern int mongo_ping_db (void);

// connect to the mongo db with db name
extern int mongo_connect (void);

// disconnects from the db
extern void mongo_disconnect (void);

extern mongoc_collection_t *mongo_get_collection (const char *collection_name);

#pragma region CRUD

// counts the docs in a collection by a matching query
extern int64_t mongo_count_docs (mongoc_collection_t *collection, bson_t *query);

// inserts a document into a collection
extern int mongo_insert_document (mongoc_collection_t *collection, bson_t *doc);

// use a query to find one doc
extern const bson_t *mongo_find_one (mongoc_collection_t *collection, bson_t *query);

// use a query to find all matching documents
// an empty query will return all the docs
extern bson_t **mongo_find_all (mongoc_collection_t *collection, bson_t *query, 
    uint64_t *n_docs);

// updates a doc by a matching query with the new values
// destroys query and update bson_t
extern int mongo_update_one (mongoc_collection_t *collection, bson_t *query, 
    bson_t *update);

// deletes one matching document by a query
extern int mongo_delete_one (mongoc_collection_t *collection, bson_t *query);

#pragma endregion

#endif