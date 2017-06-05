//
// Created by renwuxun on 6/3/17.
//

#ifndef WORKER_CONF_H
#define WORKER_CONF_H


int worker_env_get_listen_fd();

int worker_env_get_worker_id();

int worker_env_get_worker_count();

int worker_env_get_shm_id();

int worker_env_get_shm_size();


#endif //WORKER_CONF_H
