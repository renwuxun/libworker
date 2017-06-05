//
// Created by renwuxun on 5/29/17.
//

#ifndef WORKER_DEFS_H
#define WORKER_DEFS_H


#ifndef container_of
# define container_of(ptr, type, field) ((type*)((char*)(ptr) - ((char*)&((type*)0)->field)))
#endif


/*致命错误*/
#define worker_log_crit(fmt,...) fprintf(stderr, "[pid:%d] : "fmt"\n", getpid(), ##__VA_ARGS__)
/*错误*/
#define worker_log_error(fmt,...) fprintf(stderr, "[pid:%d] : "fmt"\n", getpid(), ##__VA_ARGS__)
/*警告*/
#define worker_log_warning(fmt,...) fprintf(stderr, "[pid:%d] : "fmt"\n", getpid(), ##__VA_ARGS__)
/*注意，统计类日志*/
#define worker_log_notice(fmt,...) fprintf(stderr, "[pid:%d] : "fmt"\n", getpid(), ##__VA_ARGS__)
/*调试信息*/
#define worker_log_debug(fmt,...) fprintf(stdout, "[pid:%d] : "fmt"\n", getpid(), ##__VA_ARGS__)



#ifndef WORKER_ALIGNMENT
#define WORKER_ALIGNMENT sizeof(unsigned long)
#endif

#define worker_align(size, alignment) (((size) + (alignment - 1)) & ~(alignment - 1))
#define worker_align_ptr(ptr, alignment) (char*)(((uintptr_t) (ptr) + ((uintptr_t) alignment - 1)) & ~((uintptr_t) alignment - 1))


#endif //WORKER_DEFS_H
