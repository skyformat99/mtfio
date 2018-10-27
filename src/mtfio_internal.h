#ifndef MTFIO_INTERNAL
#define MTFIO_INTERNAL

#include <limits.h>
#include <linux/limits.h>
#include <pthread.h>

#include "mtfio.h"

struct mtfio_handle {
    char path[PATH_MAX+1];
    pthread_mutex_t mutex;
    int num_threads;
    int locked;
};

#endif
