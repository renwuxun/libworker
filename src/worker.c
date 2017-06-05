//
// Created by renwuxun on 5/29/17.
//

#include "worker.h"



static struct worker_s worker = {0};

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
inline static struct worker_conn_s* worker_conn_pool_get(struct worker_conn_pool_s* conn_pool) {
    struct worker_conn_s* conn = conn_pool->conn;
    if (conn) {
        conn_pool->conn = conn->next;
        conn->next = NULL;
        conn_pool->frees--;
    }
    return conn;
}
inline static void worker_conn_pool_put(struct worker_conn_pool_s* conn_pool, struct worker_conn_s* conn) {
    if (NULL == conn_pool->conn) {
        conn_pool->conn = conn;
        return;
    }
    conn->next = conn_pool->conn;
    conn_pool->conn = conn;
    conn_pool->frees++;
}


inline static void worker_real_send(struct worker_conn_s* worker_conn) {
    ssize_t n;
    do {
        n = send(worker_conn->fd, worker_conn->sendbuf->base, worker_conn->sendbuf->size, 0);
        worker_log_debug("send:%d, %s, %d",n,strerror(errno), worker_conn->sendbuf->size);
        switch (n) {
            case -1:
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) { // 缓冲区满了,等下次
                    errno = 0;
                    return;
                }
                worker.on_send_error(worker_conn);
                worker_log_error("send");
                return;
            default:
                worker_conn->sendbuf->base += n;
                worker_conn->sendbuf->size -= n;
                worker.on_send_success(worker_conn);
                if (!worker_conn->sendbuf) {
                    return;
                }
        }
    } while (1);
}
inline static void worker_real_recv(struct worker_conn_s* worker_conn) {
    ssize_t n;
    do {
        worker.recv_buf_setter(worker_conn);
        n = recv(worker_conn->fd, worker_conn->recvbuf->base, worker_conn->recvbuf->size, 0);
        worker_log_debug("recv:%d, %s, %d",n,strerror(errno), worker_conn->recvbuf->size);
        switch (n) {
            case 0:
                worker.on_recv_close(worker_conn);
                return;
            case -1:
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    errno = 0;
                    return;
                }
                worker.on_recv_error(worker_conn);
                worker_log_error("recv");
                return;
            default:
                worker_conn->recvbuf->base += n;
                worker_conn->recvbuf->size -= n;
                worker.on_recv_success(worker_conn);
                if (!worker_conn->recvbuf) {
                    return;
                }
        }
    } while (1);
}


inline static void worker_conn_register_recv(struct worker_conn_s* worker_conn) {
    if (ev_is_active(&worker_conn->rw_watcher)) {
        ev_timer_stop(worker.loop, &worker_conn->close_timer);
        ev_io_stop(worker.loop, &worker_conn->rw_watcher);
    }
    ev_io_set(&worker_conn->rw_watcher, worker_conn->fd, EV_READ);
    ev_io_start(worker.loop, &worker_conn->rw_watcher);
    ev_timer_set(&worker_conn->close_timer, WORKER_CLOSE_TIMEOUT_SEC, 0.);
    ev_timer_start(worker.loop, &worker_conn->close_timer);
}
inline static void worker_conn_register_send(struct worker_conn_s* worker_conn) {
    if (ev_is_active(&worker_conn->rw_watcher)) {
        ev_timer_stop(worker.loop, &worker_conn->close_timer);
        ev_io_stop(worker.loop, &worker_conn->rw_watcher);
    }
    ev_io_set(&worker_conn->rw_watcher, worker_conn->fd, EV_WRITE);
    ev_io_start(worker.loop, &worker_conn->rw_watcher);
    ev_timer_set(&worker_conn->close_timer, WORKER_CLOSE_TIMEOUT_SEC, 0);
    ev_timer_start(worker.loop, &worker_conn->close_timer);
}

void worker_conn_recv_start(struct worker_conn_s* worker_conn) {
    worker_real_recv(worker_conn);
    if (worker_conn->recvbuf) {
        worker_conn_register_recv(worker_conn);
    }
}
void worker_conn_send_start(struct worker_conn_s* worker_conn) {
    worker_real_send(worker_conn);
    if (worker_conn->sendbuf) {
        worker_conn_register_send(worker_conn);
    }
}

inline static void worker_recv_send(struct ev_loop* loop, struct ev_io* rw_watcher, int revents) {
    struct worker_conn_s* worker_conn = container_of(rw_watcher, struct worker_conn_s, rw_watcher);
    if (revents & EV_READ) {
        worker_real_recv(worker_conn);
    } else if (revents & EV_WRITE) {
        worker_real_send(worker_conn);
    } else {
        worker_log_crit("worker_recv_send: unkown event");
    }
}

void worker_conn_close(struct worker_conn_s* worker_conn) {
    if (ev_is_active(&worker_conn->close_timer)) {
        ev_timer_stop(worker.loop, &worker_conn->close_timer);
    }
    if (ev_is_active(&worker_conn->rw_watcher)) {
        ev_io_stop(worker.loop, &worker_conn->rw_watcher);
        worker_conn_pool_put(worker.conn_pool, worker_conn);
    }
}

inline static void worker_recv_send_timeout(struct ev_loop* loop, struct ev_timer* close_timer, int revents) {
    struct worker_conn_s* worker_conn = container_of(close_timer, struct worker_conn_s, close_timer);
    worker.on_recv_send_timeout(worker_conn);
}


inline static void worker_accept(struct ev_loop* loop, struct ev_io* accept_watcher, int revents) {
    int client_fd;
    __SOCKADDR_ARG client_addr={0};
    socklen_t* __restrict client_addr_len={0};

    struct worker_conn_s* worker_conn = worker_conn_pool_get(worker.conn_pool);
    if (NULL == worker_conn) {
        worker_log_warning("no more connection, total:%d", (int)worker.conn_pool->total);
        return;
    }

    do {
        client_fd = accept(worker.listen_fd, client_addr, client_addr_len);
        if (-1 == client_fd) {
            switch (errno) {
                case EINTR:
                    continue;
                case EAGAIN:
                    goto end;
                case EMFILE:
                    worker_log_warning("process open files limit");
                    goto end;
                case ENFILE:
                    worker_log_warning("system open files limit");
                    goto end;
                default://error
                    worker_log_error("worker_accept");
                    goto end;
            }
        }
        break;
    } while (1);

    int p = fcntl(client_fd, F_GETFL);
    if (-1 == p || -1 == fcntl(client_fd, F_SETFL, p|O_NONBLOCK)) {
        goto end;
    }

    worker_conn->fd = client_fd;
    ev_init(&worker_conn->rw_watcher, worker_recv_send);
    ev_init(&worker_conn->close_timer, worker_recv_send_timeout);
    worker_conn_recv_start(worker_conn);

end:
    worker_conn_pool_put(worker.conn_pool, worker_conn);
}

inline static void worker_accept_stop(struct ev_loop* loop, struct ev_signal* quit_watcher, int revents) {
    if (ev_is_active(&worker.accept_watcher)) {
        ev_io_stop(loop, &worker.accept_watcher);
    }
}

inline static void worker_loop_break(struct ev_loop* loop, struct ev_signal* quit_watcher, int revents) {
    if (ev_is_active(&worker.signal_sigquit_watcher)) {
        ev_io_stop(loop, &worker.accept_watcher);
        ev_break(loop, EVBREAK_ONE);
    }
}


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
) {
    worker.loop = ev_loop_new(EVBACKEND_EPOLL);
    worker.listen_fd = listen_fd;
    worker.conn_pool = conn_pool;
    worker.buf_pool = buf_pool;
    worker.recv_buf_setter = recv_buf_setter;
    worker.on_recv_send_timeout = on_recv_send_timeout;
    worker.on_recv_error = on_recv_error;
    worker.on_recv_close = on_recv_close;
    worker.on_recv_success = on_recv_success;
    worker.on_send_error = on_send_error;
    worker.on_send_success = on_send_success;
    ev_io_init(&worker.accept_watcher, worker_accept, worker.listen_fd, EV_READ);
};

int worker_run() {
    ev_signal_init(&worker.signal_sigwinch_watcher, worker_accept_stop, SIGWINCH);
    ev_signal_start(worker.loop, &worker.signal_sigwinch_watcher);
    ev_unref(worker.loop);
    ev_signal_init(&worker.signal_sigquit_watcher, worker_loop_break, SIGQUIT);
    ev_signal_start(worker.loop, &worker.signal_sigquit_watcher);
    ev_unref(worker.loop);

    ev_io_start(worker.loop, &worker.accept_watcher);

    int r = ev_run(worker.loop, 0);

    ev_io_stop(worker.loop, &worker.accept_watcher);

    ev_ref(worker.loop);
    ev_signal_stop(worker.loop, &worker.signal_sigwinch_watcher);
    ev_ref(worker.loop);
    ev_signal_stop(worker.loop, &worker.signal_sigquit_watcher);

    ev_loop_destroy(worker.loop);

    return r;
}