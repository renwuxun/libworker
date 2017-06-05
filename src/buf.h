//
// Created by renwuxun on 5/30/17.
//

#ifndef WORKER_BUF_H
#define WORKER_BUF_H


#include <stddef.h>
#include "defs.h"

struct worker_buf_s {
    struct worker_buf_s* next;
    size_t size;
    char* base;
    char data[0];
};

struct worker_buf_pool_s {
    struct worker_buf_s* buf;
    size_t total;
    size_t frees;
};


size_t worker_buf_pool_need_size(size_t count, size_t bufsize);

struct worker_buf_pool_s* worker_buf_pool_init(char* const ptr, size_t ptrsize, size_t bufsize);
// 请不要有多个指针t同时指向同一个buf，否则可能会出现同一个buf被put多次的情况
struct worker_buf_s* worker_buf_pool_get(struct worker_buf_pool_s* buf_pool);

void worker_buf_pool_put(struct worker_buf_pool_s* buf_pool, struct worker_buf_s* buf);


#endif //WORKER_BUF_H
