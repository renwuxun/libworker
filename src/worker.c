//
// Created by renwuxun on 5/29/17.
//

//#include "worker.h"

#include <ev.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "worker.h"
#include "buf.h"



static struct worker_s worker = {0};


inline static ssize_t worker_recv(int fd, char* buf, size_t bufsize) {
    ssize_t n;
    do {
        n = recv(fd, buf, bufsize, 0);
    } while (EINTR == errno);
    return n;
}
inline static ssize_t worker_send(int fd, char* buf, size_t bufsize) {
    ssize_t n;
    do {
        n = send(fd, buf, bufsize, 0);
    } while (EINTR == errno);
    return n;
}

inline static void worker_conn_recv_stop(struct worker_conn_s* worker_conn) {
    if (ev_is_active(&worker_conn->r_watcher)) {
        ev_io_stop(worker.loop, &worker_conn->r_watcher);
    }
}
inline static void worker_conn_send_stop(struct worker_conn_s* worker_conn) {
    if (ev_is_active(&worker_conn->w_watcher)) {
        ev_io_stop(worker.loop, &worker_conn->w_watcher);
    }
}
void worker_conn_close(struct worker_conn_s* worker_conn) {
    if (worker_conn->fd > 0) {
        worker_conn_recv_stop(worker_conn);
        worker_conn_send_stop(worker_conn);
        close(worker_conn->fd);
        worker_conn->fd = -1;
        worker.on_conn_free(worker_conn);
    }
}

inline static void worker_conn_recv(struct worker_conn_s* worker_conn) {
    ssize_t n;
    for (;;) {
        if (!worker_conn->recvbuf) {
            worker_conn->recvbuf = worker.on_conn_recv_buf_alloc(worker_conn);
        }
        if (!worker_conn->recvbuf) {
            worker_log_warning("no more buf for connection to recv");
            return;
        }
        n = worker_recv(worker_conn->fd, worker_conn->recvbuf->data+worker_conn->recvbuf->idx, worker_conn->recvbuf->size-worker_conn->recvbuf->idx);
        switch (n) {
            case 0:
                worker.on_conn_recv_close(worker_conn);
                worker_conn_close(worker_conn); // 避免关闭fd两次
                return;
            case -1:
                if (EAGAIN != errno) {
                    worker.on_conn_recv_error(worker_conn);
                    worker_conn_close(worker_conn); // 避免关闭fd两次
                }
                return;
            default:
                worker_conn->recvbuf->idx += n;
                worker.on_conn_recv_success(worker_conn, worker_conn->recvbuf);
                if (0 < worker_conn->recvbuf->size) { // 接收区空了
                    return;
                }
                /* curren recv buf is full*/
        }
    }
}

inline static void worker_conn_send(struct worker_conn_s* worker_conn) {
    struct worker_buf_s* a_send_buf;
    ssize_t n;
    for (;worker_conn->sendbuf;) {
        n = worker_send(worker_conn->fd, worker_conn->sendbuf->data+worker_conn->sendbuf->idx, worker_conn->sendbuf->size);
        switch (n) {
            case -1:
                if (EAGAIN != errno) {
                    worker.on_conn_send_error(worker_conn);
                    worker_conn_close(worker_conn); // 避免关闭fd两次
                }
                return;
            default:
                worker_conn->sendbuf->size -= n;
                worker_conn->sendbuf->idx += n;
                if (0 < worker_conn->sendbuf->size) { // 发送区满了
                    return;
                }
                a_send_buf = worker_conn->sendbuf;
                worker_conn->sendbuf = worker_conn->sendbuf->next;
                a_send_buf->next = NULL;
                worker.on_conn_send_a_buf_finish(worker_conn, a_send_buf);
        }
    }
}

void worker_conn_send_buf_append(struct worker_conn_s* worker_conn, struct worker_buf_s* sendbuf) {
    if (!worker_conn->sendbuf) {
        worker_conn->sendbuf = sendbuf;
        return;
    }
    struct worker_buf_s* buf = worker_conn->sendbuf;
    for (;buf->next;buf=buf->next) {}
    buf->next = sendbuf;
}


inline static void worker_accept_stop_cb(struct ev_loop* loop, struct ev_signal* winch_watcher, int revents) {
    if (ev_is_active(&worker.accept_watcher)) {
        ev_io_stop(loop, &worker.accept_watcher);
    }
}
inline static void worker_loop_break_cb(struct ev_loop* loop, struct ev_signal* quit_watcher, int revents) {
    if (ev_is_active(&worker.signal_sigquit_watcher)) {
        ev_io_stop(loop, &worker.accept_watcher);
        ev_break(loop, EVBREAK_ONE);
    }
}

inline static void worker_conn_recv_cb(struct ev_loop* loop, struct ev_io* r_watcher, int revents) {
    struct worker_conn_s* worker_conn = container_of(r_watcher, struct worker_conn_s, r_watcher);
    worker_conn_recv(worker_conn);
}
inline static void worker_conn_send_cb(struct ev_loop* loop, struct ev_io* w_watcher, int revents) {
    struct worker_conn_s* worker_conn = container_of(w_watcher, struct worker_conn_s, w_watcher);
    worker_conn_send(worker_conn);
}

inline static void worker_conn_recv_start(struct worker_conn_s* worker_conn) {
    if (!ev_is_active(&worker_conn->r_watcher)) {
        ev_io_init(&worker_conn->r_watcher, worker_conn_recv_cb, worker_conn->fd, EV_READ);
        ev_io_start(worker.loop, &worker_conn->r_watcher);
    }
}

void worker_conn_send_start(struct worker_conn_s* worker_conn) {
    if (!ev_is_active(&worker_conn->w_watcher)) {
        ev_io_init(&worker_conn->w_watcher, worker_conn_send_cb, worker_conn->fd, EV_WRITE);
        ev_io_start(worker.loop, &worker_conn->w_watcher);
    }
}

int worker_accept(struct worker_conn_s* worker_conn) {
    int client_fd;

    do {
        client_fd = accept(worker.listen_fd, &worker_conn->addr, &worker_conn->addr_len);
        if (-1 == client_fd) {
            switch (errno) {
                case EINTR:
                    continue;
                case EAGAIN:
                    return client_fd;
                case EMFILE:
                    worker_log_warning("process open files limit");
                    return client_fd;
                case ENFILE:
                    worker_log_warning("system open files limit");
                    return client_fd;
                default://error
                    worker_log_warning("worker_accept");
                    return client_fd;
            }
        }
        break;
    } while (1);

    int p = fcntl(client_fd, F_GETFL);
    if (-1 == p || -1 == fcntl(client_fd, F_SETFL, p|O_NONBLOCK)) {
        worker_log_warning("error on set nonblocking");
        return client_fd;
    }

    return client_fd;
}

inline static void worker_accept_cb(struct ev_loop* loop, struct ev_io* accept_watcher, int revents) {
    struct worker_conn_s* worker_conn = worker.on_conn_alloc();
    if (!worker_conn) {
        worker_log_warning("alloc a null connection");
        return;
    }
    if (-1 == worker_accept(worker_conn)) {
        worker.on_conn_free(worker_conn);
        return;
    }

    worker_conn_recv_start(worker_conn);
}


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
) {
    worker.listen_fd = listen_fd;
    worker.on_conn_recv_buf_alloc = on_conn_recv_buf_alloc;
    worker.on_conn_recv_success = on_conn_recv_success;
    worker.on_conn_recv_error = on_conn_recv_error;
    worker.on_conn_send_error = on_conn_send_error;
    worker.on_conn_recv_close = on_conn_recv_close;
    worker.on_conn_send_a_buf_finish = on_conn_send_a_buf_finish;
    worker.on_conn_alloc = on_conn_alloc;
    worker.on_conn_free = on_conn_free;
}


int worker_run() {
    worker.loop = ev_loop_new(EVBACKEND_EPOLL);

    ev_signal_init(&worker.signal_sigwinch_watcher, worker_accept_stop_cb, SIGWINCH);
    ev_signal_start(worker.loop, &worker.signal_sigwinch_watcher);
    ev_unref(worker.loop);
    ev_signal_init(&worker.signal_sigquit_watcher, worker_loop_break_cb, SIGQUIT);
    ev_signal_start(worker.loop, &worker.signal_sigquit_watcher);
    ev_unref(worker.loop);

    ev_io_init(&worker.accept_watcher, worker_accept_cb, worker.listen_fd, EV_READ);
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