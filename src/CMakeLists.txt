add_library(worker STATIC defs.h worker.c buf.c conf.c connpool.c)
target_link_libraries(worker ev)