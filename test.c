#include <stdlib.h>
#include "src/worker.h"
#include "src/conf.h"


struct worker_buf_pool_s* buf_pool = NULL;
size_t bufsize = 0;



void recv_buf_setter(struct worker_conn_s* worker_conn) {
    if (!worker_conn->recvbuf) {
        worker_conn->recvbuf = worker_buf_pool_get(buf_pool);
        worker_conn->recvbuf->size = bufsize;
        worker_conn->recvbuf->base = worker_conn->recvbuf->data;
    }
}

void send_buf_setter(struct worker_conn_s* worker_conn) {
    // nothing
}

void on_recv_send_timeout(struct worker_conn_s* worker_conn) {
    struct worker_buf_s* buf;
    while (worker_conn->sendbuf) { // 一个buf请不要同时挂在多处
        buf = worker_conn->sendbuf->next;
        worker_buf_pool_put(buf_pool, worker_conn->sendbuf);
        worker_conn->sendbuf = buf;
    }
    while (worker_conn->recvbuf) {
        buf = worker_conn->recvbuf->next;
        worker_buf_pool_put(buf_pool, worker_conn->recvbuf);
        worker_conn->recvbuf = buf;
    }
    worker_conn_close(worker_conn);
}
void on_recv_error(struct worker_conn_s* worker_conn) {
    worker_log_debug("on_recv_error");
    on_recv_send_timeout(worker_conn);
}
void on_recv_close(struct worker_conn_s* worker_conn) {
    worker_log_debug("on_recv_close");
    on_recv_send_timeout(worker_conn);
}
void on_send_error(struct worker_conn_s* worker_conn) {
    worker_log_debug("on_send_error");
    on_recv_send_timeout(worker_conn);
}

void on_recv_success(struct worker_conn_s* worker_conn) {
    char* resp = "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: 6\r\n"
            "\r\n"
            "hello\n";

    struct worker_buf_s buf = {0};
    buf.size = bufsize - worker_conn->recvbuf->size;
    buf.base = worker_conn->recvbuf->data;
    char* ptr;
    while (buf.size>3) {
        ptr = strstr(buf.base, "\r\n\r\n");
        if (!ptr) {
            if (buf.base != worker_conn->recvbuf->data) {
                worker_conn->recvbuf->base = worker_conn->recvbuf->data;
                worker_conn->recvbuf->size = bufsize-buf.size;
                size_t i;
                for(i=0; i<buf.size; i++) {
                    worker_conn->recvbuf->base[i] = buf.base[i];
                }
            }
            break;
        }

        worker_conn->sendbuf = worker_buf_pool_get(buf_pool);
        worker_conn->sendbuf->size = strlen(resp)+1;
        memcpy(worker_conn->sendbuf->base, resp, strlen(resp)+1);
        worker_conn_send_start(worker_conn);

        ptr+=4;
        buf.size = buf.size-(ptr-buf.base);
        buf.base = ptr;
    }
}

void on_send_success(struct worker_conn_s* worker_conn) {
    if (worker_conn->sendbuf->size == 0) {
        worker_buf_pool_put(buf_pool, worker_conn->sendbuf);
        worker_conn->sendbuf = NULL;
    }
}


int main(int argc, char** argv) {
    int listen_fd = worker_env_get_listen_fd();
    if (listen_fd < 0) {
        worker_log_crit("listen_fd < 0, %d", listen_fd);
        return EXIT_FAILURE;
    }

    size_t conn_pool_need_size = worker_conn_pool_need_size(1024);
    char* conn_pool_ptr = malloc(conn_pool_need_size);
    struct worker_conn_pool_s* conn_pool = worker_conn_pool_init(
            conn_pool_ptr,
            conn_pool_need_size
    );

    bufsize = worker_align(2048, WORKER_ALIGNMENT);
    size_t buf_pool_need_size = worker_buf_pool_need_size(1024, bufsize);
    char* buf_pool_ptr = malloc(conn_pool_need_size);
    buf_pool = worker_buf_pool_init(
            buf_pool_ptr,
            buf_pool_need_size,
            bufsize
    );

    worker_init(
            listen_fd,
            conn_pool,
            buf_pool,
            recv_buf_setter,
            on_recv_send_timeout,
            on_recv_error,
            on_recv_close,
            on_recv_success,
            on_send_error,
            on_send_success
    );

    worker_log_debug("worker start");

    int r = worker_run();

    free(conn_pool_ptr);
    free(buf_pool_ptr);

    worker_log_debug("worker stop");

    return r;
}