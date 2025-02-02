#ifndef HTTPPO_UTIL_H_
#define HTTPPO_UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static inline void die(const char* msg) {
    fprintf(stderr, "ERROR: %s: %s\n", msg, strerror(errno));
    exit(1);
}

#endif // HTTPPO_UTIL_H_
