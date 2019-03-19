#ifndef __MONGO_H__
#define __MONGO_H__

#include <mongoc/mongoc.h>
#include <bson/bson.h>

// FIXME: we need to have more flexibility here!!
// extern const char *uri_string;
// extern const char *db_name;

#pragma region MONGO

extern int mongo_connect (void);
extern void mongo_disconnect (void);

extern mongoc_uri_t *mongo_uri;
extern mongoc_client_t *mongo_client;
extern mongoc_database_t *mongo_database;

#pragma endregion

#pragma region CRUD OPERATIONS

// counts the docs in a collection by a matching query
extern uint64_t mongo_count_docs (mongoc_collection_t *collection, bson_t *query);

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