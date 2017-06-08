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



struct worker_conn_s {
    struct worker_conn_s* next;
    struct ev_io r_watcher;
    struct ev_io w_watcher;
    struct worker_buf_s* current_recvbuf;
    struct worker_buf_s* sendbuf;
    struct worker_buf_s* recvbuf;
    int fd;
    struct sockaddr addr;
    socklen_t addr_len;
    void* data;
};

struct worker_s {
    struct ev_loop* loop;
    struct ev_signal signal_sigwinch_watcher;
    struct ev_signal signal_sigquit_watcher;
    struct ev_io accept_watcher;
    int listen_fd;
    struct worker_buf_s* (*on_conn_recv_buf_alloc)(struct worker_conn_s* worker_conn);
    void (*on_conn_recv_success)(struct worker_conn_s* worker_conn, struct worker_buf_s* recvbuf);
    void (*on_conn_recv_error)(struct worker_conn_s* worker_conn);
    void (*on_conn_send_error)(struct worker_conn_s* worker_conn);
    void (*on_conn_recv_close)(struct worker_conn_s* worker_conn);
    void (*on_conn_send_a_buf_finish)(struct worker_conn_s* worker_conn, struct worker_buf_s* a_send_buf);
    struct worker_conn_s* (*on_conn_alloc)();
    void (*on_conn_free)(struct worker_conn_s* worker_conn);
};


void worker_conn_close(struct worker_conn_s* worker_conn);

void worker_conn_send_buf_append(struct worker_conn_s* worker_conn, struct worker_buf_s* sendbuf);

void worker_conn_send_start(struct worker_conn_s* worker_conn);

int worker_accept(struct worker_conn_s* worker_conn);

void worker_init(
        int listen_fd,
        struct worker_buf_s* (*on_conn_recv_buf_alloc)(struct worker_conn_s* worker_conn),
        void (*on_conn_recv_success)(struct worker_conn_s* worker_conn, struct worker_buf_s* recvbuf),
        void (*on_conn_recv_error)(struct worker_conn_s* worker_conn),
        void (*on_conn_send_error)(struct worker_conn_s* worker_conn),
        void (*on_conn_recv_close)(struct worker_conn_s* worker_conn),
        void (*on_conn_send_a_buf_finish)(struct worker_conn_s* worker_conn, struct worker_buf_s* a_send_buf),
        struct worker_conn_s* (*on_conn_alloc)(),
        void (*on_conn_free)(struct worker_conn_s* worker_conn)
);


int worker_run();


#endif //WORKER_WORKER_H
