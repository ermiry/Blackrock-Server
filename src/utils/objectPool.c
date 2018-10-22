// Implementation of a simple object pool system using a stack

// 08/08/2018 --> for now we only need one global pool for the GameObjects like monsters and various items
// the map is handled differntly using an array.

#include <stdlib.h>
#include <stdio.h>

#include "objectPool.h"

// TODO: add the avility to init a pool with some members
Pool *initPool (void) {

    Pool *pool = (Pool *) malloc (sizeof (Pool));

    if (pool != NULL) {
        pool->size = 0;
        pool->top = NULL;
    }

    return pool;

}

void push (Pool *pool, void *data) {

    if (data == NULL) return;

    PoolMember *new = (PoolMember *) malloc (sizeof (PoolMember));
    new->data = data;

    if (POOL_SIZE (pool) == 0) new->next = NULL;
    else new->next = pool->top;

    pool->top = new;
    pool->size++;

}

void *pop (Pool *pool) {

    void *data;

    data = pool->top->data;

    pool->top = pool->top->next;
    pool->size--;

    return data;

}

void clearPool (Pool *pool) {

    if (POOL_SIZE (pool) > 0) {
        PoolMember *ptr, *temp;
        void *data;
        ptr = POOL_TOP (pool);
        while (ptr != NULL) {
            temp = pool->top;
            data = pool->top->data;
            free (data);
            pool->top = pool->top->next;
            free (temp);
            ptr = pool->top;
        }
    }
    
    free (pool);

}
