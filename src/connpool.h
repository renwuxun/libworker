//
// Created by renwuxun on 6/7/17.
//

#ifndef WORKER_CONNPOOL_H
#define WORKER_CONNPOOL_H


#include <stddef.h>


struct worker_conn_pool_s {
    struct worker_conn_s* conn;
    size_t total;
    size_t frees;
};


size_t worker_conn_pool_need_size(size_t count);

struct worker_conn_pool_s* worker_conn_pool_init(char* const ptr, size_t ptrsize);

struct worker_conn_s* worker_conn_pool_get(struct worker_conn_pool_s* conn_pool);

void worker_conn_pool_put(struct worker_conn_pool_s* conn_pool, struct worker_conn_s* conn);


#endif //WORKER_CONNPOOL_H
