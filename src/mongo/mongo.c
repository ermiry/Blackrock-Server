#include <stdlib.h>
#include <stdio.h>

#include <mongoc/mongoc.h>
#include <bson/bson.h>

#include "utils/myUtils.h"
#include "utils/log.h"

// FIXME: we need to have more flexibility here!!
mongoc_uri_t *mongo_uri;
mongoc_client_t *mongo_client;
mongoc_database_t *mongo_database;

#define APP_NAME        "blackrock-server"

const char *uri_string;

#pragma region MONGO

int mongo_connect (void) {

    bson_error_t error;

    mongoc_init ();     // init mongo internals

    // safely create mongo uri object
    mongo_uri = mongoc_uri_new_with_error (uri_string, &error);
    if (!mongo_uri) {
        fprintf (stderr,
                "failed to parse URI: %s\n"
                "error message:       %s\n",
                uri_string,
                error.message);
        return 1;
    }

    // create a new client instance
    mongo_client = mongoc_client_new_from_uri (mongo_uri);
    if (!mongo_client) {
        fprintf (stderr, "Failed to create a new client instance!\n");
        return 1;
    }

    // register the app name -> for logging info
    mongoc_client_set_appname (mongo_client, APP_NAME);

    return 0;       // success

}

void mongo_disconnect (void) {

    mongoc_database_destroy (mongo_database);

    mongoc_uri_destroy (mongo_uri);
    mongoc_client_destroy (mongo_client);
    mongoc_cleanup ();

}

#pragma endregion

#pragma region CRUD

// counts the docs in a collection by a matching query
int64_t mongo_count_docs (mongoc_collection_t *collection, bson_t *query) {

    int64_t retval = 0;

    if (collection && query) {
        bson_error_t error;
        retval = mongoc_collection_count_documents (collection, query, NULL, NULL, NULL, &error);
        if (retval < 0) {
            logMsg (stderr, ERROR, NO_TYPE, createString ("%s", error.message));
            retval = 0;
        }

        bson_destroy (query);
    }

    return retval;

}

// inserts a document into a collection
int mongo_insert_document (mongoc_collection_t *collection, bson_t *doc) {

    int retval = 0;
    bson_error_t error;

    if (!mongoc_collection_insert_one (collection, doc, NULL, NULL, &error)) {
        logMsg (stderr, ERROR, NO_TYPE, createString ("%s", error.message));
        retval = 1;
    }

    bson_destroy (doc);

    return retval;

}

// use a query to find one doc
const bson_t *mongo_find_one (mongoc_collection_t *collection, bson_t *query) {

    const bson_t *doc = NULL;

    if (collection && query) {
        mongoc_cursor_t *cursor = mongoc_collection_find_with_opts (collection, query, NULL, NULL);

        mongoc_cursor_next (cursor, &doc);

        bson_destroy (query);
        mongoc_cursor_destroy (cursor);
    }

    return doc;

}

// use a query to find all matching documents
// an empty query will return all the docs in a collection
bson_t **mongo_find_all (mongoc_collection_t *collection, bson_t *query, uint64_t *n_docs) {

    bson_t **retval = NULL;
    *n_docs = 0;

    if (collection && query) {
        uint64_t count = mongo_count_docs (collection, bson_copy (query));
        if (count > 0) {
            retval = (bson_t **) calloc (count, sizeof (bson_t *));
            for (uint64_t i = 0; i < count; i++) retval[i] = bson_new ();

            const bson_t *doc;
            mongoc_cursor_t *cursor = mongoc_collection_find_with_opts (collection, query, NULL, NULL);

            uint64_t i = 0;
            while (mongoc_cursor_next (cursor, &doc)) {
                // add the matching doc into our retval array
                bson_copy_to (doc, retval[i]);
                i++;
            }

            *n_docs = count;

            bson_destroy (query);
            mongoc_cursor_destroy (cursor);
        }
    }

    return retval;

}

// updates a doc by a matching query with the new values;
// destroys query and update bson_t
int mongo_update_one (mongoc_collection_t *collection, bson_t *query, bson_t *update) {

    int retval = 1;

    if (collection && query && update) {
        bson_error_t error;

        if (mongoc_collection_update_one (collection, query, update, NULL, NULL, &error)) 
            retval = 0;

        else
            logMsg (stderr, ERROR, NO_TYPE, createString ("Failed to update doc: %s", error.message));

        bson_destroy (query);
        bson_destroy (update);
    }

    return retval;

}

// deletes one matching document by a query
int mongo_delete_one (mongoc_collection_t *collection, bson_t *query) {

    int retval = 0;

    if (collection && query) {
        bson_error_t error;

        if (!mongoc_collection_delete_one (collection, query, NULL, NULL, &error)) {
            logMsg (stderr, ERROR, NO_TYPE, createString ("Delete failed: %s", error.message));
            retval = 1;
        }

        bson_destroy (query);
    }

    return retval;

}

#pragma endregion