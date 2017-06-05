/**
 * Created by renwuxun on 5/29/17.
 * what u need to do, is to maintain your memory
**/

#ifndef WORKER_WORKER_H
#define WORKER_WORKER_H


#include <ev.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "defs.h"
#include "buf.h"



#ifndef WORKER_CLOSE_TIMEOUT_SEC
# define WORKER_CLOSE_TIMEOUT_SEC 15.
#endif



struct worker_conn_s {
    int fd;
    struct worker_buf_s* recvbuf;
    struct worker_buf_s* sendbuf;
    struct ev_timer close_timer;
    struct ev_io rw_watcher;
    struct worker_conn_s* next;
};

struct worker_conn_pool_s {
    struct worker_conn_s* conn;
    size_t total;
    size_t frees;
};

struct worker_s {
    void (*recv_buf_setter)(struct worker_conn_s* worker_conn);
    void (*on_recv_send_timeout)(struct worker_conn_s* worker_conn);
    void (*on_recv_error)(struct worker_conn_s* worker_conn);
    void (*on_recv_close)(struct worker_conn_s* worker_conn);
    void (*on_recv_success)(struct worker_conn_s* worker_conn);
    void (*on_send_error)(struct worker_conn_s* worker_conn);
    void (*on_send_success)(struct worker_conn_s* worker_conn);
    struct ev_loop* loop;
    struct ev_signal signal_sigwinch_watcher;
    struct ev_signal signal_sigquit_watcher;
    struct ev_io accept_watcher;
    int listen_fd;
    struct worker_conn_pool_s* conn_pool;
    struct worker_buf_pool_s* buf_pool;
};


size_t worker_conn_pool_need_size(size_t count);
struct worker_conn_pool_s* worker_conn_pool_init(char* const ptr, size_t ptrsize);


void worker_conn_recv_start(struct worker_conn_s* worker_conn);
void worker_conn_send_start(struct worker_conn_s* worker_conn);


void worker_conn_close(struct worker_conn_s* worker_conn);


void worker_init(
        int listen_fd,
        struct worker_conn_pool_s* conn_pool,
        struct worker_buf_pool_s* buf_pool,
        void (*recv_buf_setter)(struct worker_conn_s* worker_conn),
        void (*on_recv_send_timeout)(struct worker_conn_s* worker_conn),
        void (*on_recv_error)(struct worker_conn_s* worker_conn),
        void (*on_recv_close)(struct worker_conn_s* worker_conn),
        void (*on_recv_success)(struct worker_conn_s* worker_conn),
        void (*on_send_error)(struct worker_conn_s* worker_conn),
        void (*on_send_success)(struct worker_conn_s* worker_conn)
);
int worker_run();



#endif //WORKER_WORKER_H
