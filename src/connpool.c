//
// Created by renwuxun on 6/7/17.
//

#include <stdio.h>
#include <unistd.h>
#include "connpool.h"
#include "defs.h"
#include "worker.h"


size_t worker_conn_pool_need_size(size_t count) {
    return sizeof(struct worker_conn_pool_s) + sizeof(struct worker_conn_s) * count;
}
struct worker_conn_pool_s* worker_conn_pool_init(char* const ptr, size_t ptrsize) {
    if (ptrsize < sizeof(struct worker_conn_pool_s)+sizeof(struct worker_conn_s)) {
        worker_log_crit("connection pool need at least %d, %d given", (int)(sizeof(struct worker_conn_pool_s)+sizeof(struct worker_conn_s)), (int)ptrsize);
        return NULL;
    }

    char* _ptr = ptr;

    struct worker_conn_pool_s* conn_pool = (struct worker_conn_pool_s*)_ptr;
    _ptr += sizeof(struct worker_conn_pool_s);
    ptrsize -= sizeof(struct worker_conn_pool_s);

    conn_pool->conn = (struct worker_conn_s*)_ptr;
    conn_pool->frees = conn_pool->total = ptrsize/sizeof(struct worker_conn_s);
    int i;
    for (i = 0; i < conn_pool->total-1; i++) {
        conn_pool->conn[i].next = &conn_pool->conn[i+1];
    }
    conn_pool->conn[conn_pool->total-1].next = NULL;

    return conn_pool;
}
struct worker_conn_s* worker_conn_pool_get(struct worker_conn_pool_s* conn_pool) {
    struct worker_conn_s* conn = conn_pool->conn;
    if (conn) {
        conn_pool->conn = conn->next;
        conn->next = NULL;
        conn_pool->frees--;
    }
    return conn;
}
void worker_conn_pool_put(struct worker_conn_pool_s* conn_pool, struct worker_conn_s* conn) {
    if (NULL == conn_pool->conn) {
        conn_pool->conn = conn;
        return;
    }
    conn->next = conn_pool->conn;
    conn_pool->conn = conn;
    conn_pool->frees++;
}