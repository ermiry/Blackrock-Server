// Implementation of a simple object pool system using a stack

#include <stdlib.h>
#include <stdio.h>

#include "utils/objectPool.h"

Pool *pool_init (void (*destroy)(void *data)) {

    Pool *pool = (Pool *) malloc (sizeof (Pool));

    if (pool != NULL) {
        pool->size = 0;
        pool->top = NULL;
        pool->destroy = destroy;
    }

    return pool;

}

void pool_push (Pool *pool, void *data) {

    if (data == NULL) return;

    PoolMember *new = (PoolMember *) malloc (sizeof (PoolMember));
    new->data = data;

    if (POOL_SIZE (pool) == 0) new->next = NULL;
    else new->next = pool->top;

    pool->top = new;
    pool->size++;

}

void *pool_pop (Pool *pool) {

    void *data;

    data = pool->top->data;

    pool->top = pool->top->next;
    pool->size--;

    return data;

}

void pool_clear (Pool *pool) {

    if (pool) {
        if (POOL_SIZE (pool) > 0) {
            void *data = NULL;
            while (pool->size > 0) {
                data = pool_pop (pool);
                if (data) {
                    if (pool->destroy) pool->destroy (data);
                    else free (data);
                }
            }
        }

        free (pool);
    }

}