#include "files.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "base.h"
#include "hash.h"
#include "util.h"

#define HTTPPO_FILES_REVALIDATION_TIME (2500 * 1000)

static size_t get_time_nsec(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_REALTIME, &t) == -1) {
        die("clock_gettime");
    }

    return t.tv_nsec;
}

static HttppoFile* httppo_file_read(const char* name) {
    base_file file = base_read_whole_file(name);
    if (!file.contents) {
        return NULL;
    }

    HttppoFile* hfile = malloc(sizeof(HttppoFile));
    hfile->last_read = get_time_nsec();
    hfile->name = name;
    hfile->contents = file.contents;
    hfile->size = file.stat.st_size;
    hfile->last_modified = file.stat.st_mtim.tv_nsec;

    return hfile;
}

static int httppo_file_update(HttppoFile* file, struct stat const* stat) {
    memset(file->contents, 0, file->size);
    file->contents = realloc(file->contents, stat->st_size);
    file->size = stat->st_size;

    if (base_read_whole_file_buf(file->name, file->contents, file->size) != 0) {
        return 1;
    }

    file->last_modified = stat->st_mtim.tv_nsec;
    file->last_read = get_time_nsec();
    return 0;
}

HttppoFile* httppo_files_get(HttppoFiles* files, const char* name) {
    pthread_mutex_lock(&files->mutex);
    HttppoFile* file = ht_find(&files->table, name);

    if (!file) {
        file = httppo_file_read(name);
        if (!file) {
            goto end;
        }
    }

    struct stat s;
    if (stat(file->name, &s) != 0) {
        goto end;
    }

    if ((size_t)s.st_mtim.tv_nsec != file->last_modified) {
        if (httppo_file_update(file, &s) != 0) {
            goto end;
        }
    }

end:
    pthread_mutex_unlock(&files->mutex);
    return file;
}

HttppoFiles httppo_files_new(size_t cap) {
    HttppoFiles files = {
        .table = ht_make(hash_djb2, hash_str_eq, cap),
    };
    pthread_mutex_init(&files.mutex, NULL);
    return files;
}

static void httppo_file_delete(HttppoFile* file) {
    free(file->contents);
}

void httppo_files_revalidate(HttppoFiles* files) {
    pthread_mutex_lock(&files->mutex);

    size_t now = get_time_nsec();

    HT_ITER(files->table, {
        HttppoFile* file = (HttppoFile*)kv.value;
        if (now - file->last_read > HTTPPO_FILES_REVALIDATION_TIME) {
            ht_delete(&files->table, file->name);
            httppo_file_delete(file);
        }
    });

    pthread_mutex_unlock(&files->mutex);
}
