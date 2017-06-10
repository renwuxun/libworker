//
// Created by renwuxun on 5/30/17.
//

#include <stdio.h>
#include <unistd.h>
#include "buf.h"



size_t worker_buf_pool_need_size(size_t count, size_t bufsize) {
    return sizeof(struct worker_buf_pool_s) + (sizeof(struct worker_buf_s)+bufsize) * count;
}
struct worker_buf_pool_s* worker_buf_pool_init(char* const ptr, size_t ptrsize, size_t bufsize) {
    if (ptrsize < sizeof(struct worker_buf_pool_s)+sizeof(struct worker_buf_s)+bufsize) {
        worker_log_crit("connection pool need at least %d, %d given", (int)((sizeof(struct worker_buf_pool_s)+sizeof(struct worker_buf_s))+bufsize), (int)ptrsize);
        return NULL;
    }

    char* _ptr = ptr;

    struct worker_buf_pool_s* buf_pool = (struct worker_buf_pool_s*)_ptr;
    _ptr += sizeof(struct worker_buf_pool_s);
    ptrsize -= sizeof(struct worker_buf_pool_s);

    buf_pool->size = bufsize;
    buf_pool->buf = (struct worker_buf_s*)_ptr;
    buf_pool->frees = buf_pool->total = ptrsize/(sizeof(struct worker_buf_s)+bufsize);

    int i;
    for (i = 0; i < buf_pool->total-1; i++) {
        buf_pool->buf[i].next = &buf_pool->buf[i+1];
        buf_pool->buf[i].buf_pool = buf_pool;
    }
    buf_pool->buf[buf_pool->total-1].next = NULL;
    buf_pool->buf[i].buf_pool = buf_pool;

    return buf_pool;
}
struct worker_buf_s* worker_buf_pool_get(struct worker_buf_pool_s* buf_pool) {
    struct worker_buf_s* buf = buf_pool->buf;
    if (buf) {
        buf_pool->buf = buf->next;
        buf->next = NULL;
        buf_pool->frees--;
        buf->idx = 0;
        buf->size = buf_pool->size;
    }

    return buf;
}
void worker_buf_pool_put(struct worker_buf_s* buf) {
    if (NULL == buf->buf_pool->buf) {
        buf->buf_pool->buf = buf;
        return;
    }
    buf->next = buf->buf_pool->buf;
    buf->buf_pool->buf = buf;
    buf->buf_pool->frees++;
}
