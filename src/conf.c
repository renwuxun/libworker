//
// Created by renwuxun on 6/3/17.
//

#include <stdlib.h>
#include "conf.h"


int worker_env_get_listen_fd() {
    int listen_fd = -1;
    char* evnptr = getenv("LISTEN_FD");
    if (evnptr) {
        listen_fd = atoi(evnptr);
    }
    return listen_fd;
}

int worker_env_get_worker_id() {
    int wkr_id = -1;
    char* evnptr = getenv("WKR_ID");
    if (evnptr) {
        wkr_id = atoi(evnptr);
    }
    return wkr_id;
}

int worker_env_get_worker_count() {
    int wkr_count = -1;
    char* evnptr = getenv("WKR_COUNT");
    if (evnptr) {
        wkr_count = atoi(evnptr);
    }
    return wkr_count;
}

int worker_env_get_shm_id() {
    int shm_id = -1;
    char* evnptr = getenv("SHM_ID");
    if (evnptr) {
        shm_id = atoi(evnptr);
    }
    return shm_id;
}

int worker_env_get_shm_size() {
    int shm_size = -1;
    char* evnptr = getenv("SHM_SIZE");
    if (evnptr) {
        shm_size = atoi(evnptr);
    }
    return shm_size;
}