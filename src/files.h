#pragma once

#include <pthread.h>
#include <time.h>
#include "base.h"

typedef struct {
    const char* name;
    char* contents;
    size_t size;

    size_t last_modified;
    size_t last_read;
} HttppoFile;

/// A type representing a concurrent in-memory file cache
typedef struct {
    hash_table table;
    pthread_mutex_t mutex;
} HttppoFiles;

HttppoFiles httppo_files_new(size_t cap);
HttppoFile* httppo_files_get(HttppoFiles* files, const char* filename);
void httppo_files_revalidate(HttppoFiles* files);
