#ifndef INCLUDE_BASE_H
#define INCLUDE_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef BASEDEF
#ifdef BASE_STATIC
#define BASEDEF static
#else
#define BASEDEF extern
#endif // BASE_STATIC
#endif // BASEDEF

#ifndef BASE_ALLOC
#define BASE_ALLOC malloc
#endif // BASE_ALLOC

// DYNAMIC ARRAYS
#define DA_INIT(arr, l, c)                                                                         \
    do {                                                                                           \
        __typeof__(c) _cap = c ? c : 1;                                                           \
        (arr)->items = (__typeof__((arr)->items[0])*)malloc(sizeof((arr)->items[0]) * _cap);      \
        (arr)->cap = _cap;                                                                        \
        (arr)->len = l;                                                                            \
        memset((arr)->items, 0, l * sizeof((arr)->items[0]));                                      \
    } while (0)

#define DA_INIT_ZEROED(arr, c)                                                                     \
    do {                                                                                           \
        DA_INIT((arr), c, c);                                                                      \
        (arr)->len = 0;                                                                            \
    } while (0)

#define DA_RESIZE(arr, c)                                                                          \
    do {                                                                                           \
        (arr)->cap = (c);                                                                          \
        (arr)->items = (__typeof__((arr)->items))realloc((arr)->items, (c) * sizeof((arr)->items[0]));                       \
    } while (0)

#define DA_GROW(arr)                                                                               \
    do {                                                                                           \
        (arr)->cap *= 2;                                                                           \
        DA_RESIZE((arr), (arr)->cap);                                                              \
    } while (0)

#define DA_ADD(arr, elem)                                                                          \
    do {                                                                                           \
        if ((arr)->len == (arr)->cap) {                                                            \
            DA_GROW(arr);                                                                          \
        }                                                                                          \
        (arr)->items[(arr)->len++] = (elem);                                                       \
    } while (0)

#define DA_FREE(arr)                                                                               \
    do {                                                                                           \
        free((arr)->items);                                                                        \
    } while (0)

// HASH TABLE
#define HT_ITER(ht, body) \
  do { \
    for (size_t i = 0; i < (ht)->cap; i++) { \
         ht_bucket* bucket = (ht)->buckets[i]; \
         if (!bucket) { \
             continue; \
         } \
         for (size_t j = 0; j < bucket->len; j++) { \
             ht_kv_pair kv = bucket->items[j]; \
             body \
         } \
  } \
  } \
         while(0)

typedef struct {
    void* key;
    void* value;
} ht_kv_pair;

typedef struct {
    ht_kv_pair* items;
    size_t len;
    size_t cap;
} ht_bucket;

typedef uint64_t (*ht_hash_func)(const void*);
typedef bool (*ht_eq_func)(const void*, const void*);

typedef struct {
    ht_bucket** buckets;
    size_t len;
    size_t cap;

    /// the hash function
    ht_hash_func hash_function;
    /// the key comparison function
    ht_eq_func equality_function;
} hash_table;

BASEDEF hash_table ht_make(ht_hash_func hash_func, ht_eq_func eq_func, size_t cap);
BASEDEF void ht_add(hash_table* ht, void* key, void* value);
BASEDEF void* ht_find(hash_table* ht, const void* key);
BASEDEF void* ht_delete(hash_table* ht, const void* key);
BASEDEF void ht_destroy(hash_table* ht);

// STRING VIEW

#define SV_FMT "%.*s"

typedef struct {
    const char* ptr;
    size_t size;
} string_view;

BASEDEF string_view sv_make(const char* str, size_t size);
BASEDEF void sv_print(string_view sv);
BASEDEF string_view sv_slice(string_view sv, size_t start, size_t len);
BASEDEF string_view sv_slice_end(string_view sv, size_t start);
BASEDEF bool sv_starts_with(string_view sv, const char* prefix);
BASEDEF bool sv_starts_with_sv(string_view sv, string_view prefix);
BASEDEF bool sv_eq(string_view a, string_view b);
BASEDEF bool sv_eq_cstr(string_view sv, const char* str);
BASEDEF int64_t sv_atoi(string_view sv);
BASEDEF double sv_atof(string_view sv);
BASEDEF const char* sv_dup(string_view sv);
BASEDEF ssize_t sv_find_sub(string_view sv, string_view substr);
BASEDEF ssize_t sv_find_sub_cstr(string_view sv, const char* substr);
BASEDEF ssize_t sv_find(string_view sv, char c);

typedef struct {
    char* items;
    size_t len;
    size_t cap;
} string_builder;

BASEDEF string_builder sb_new(size_t cap);
BASEDEF void sb_destroy(string_builder* sb);
BASEDEF void sb_push_cstr(string_builder* sb, const char* str);
BASEDEF char* sb_to_cstr(string_builder* sb);

#ifdef BASE_IMPLEMENTATION

#include <stdio.h>

// STRING VIEW
BASEDEF string_view sv_make(const char* str, size_t size)
{
    return (string_view) { .ptr = str, .size = size };
}

BASEDEF void sv_print(string_view sv)
{
    printf(SV_FMT, (int)sv.size, sv.ptr);
}

BASEDEF string_view sv_slice(string_view sv, size_t start, size_t len)
{
    return (string_view) { .ptr = sv.ptr + start, .size = len };
}

BASEDEF string_view sv_slice_end(string_view sv, size_t start)
{
    return sv_slice(sv, start, sv.size - start);
}

BASEDEF bool sv_starts_with(string_view sv, const char* prefix)
{
    size_t len = strlen(prefix);
    for (size_t i = 0; i < sv.size && i < len; i++) {
        if (sv.ptr[i] != prefix[i]) {
            return false;
        }
    }

    return true;
}

BASEDEF bool sv_starts_with_sv(string_view sv, string_view prefix)
{
    for (size_t i = 0; i < sv.size && i < prefix.size; i++) {
        if (sv.ptr[i] != prefix.ptr[i]) {
            return false;
        }
    }

    return true;
}

BASEDEF bool sv_eq(string_view a, string_view b)
{
    if (a.size != b.size)
        return false;
    return strncmp(a.ptr, b.ptr, a.size) == 0;
}

BASEDEF bool sv_eq_cstr(string_view sv, const char* str)
{
    if (sv.size != strlen(str))
        return false;
    return strncmp(sv.ptr, str, sv.size) == 0;
}

BASEDEF int64_t sv_atoi(string_view sv)
{
    int64_t num = 0;
    for (size_t i = 0; i < sv.size; i++) {
        num = num * 10 + (sv.ptr[i] - '0');
    }
    return num;
}

#define DOUBLE_LIT_MAX_LEN 316

BASEDEF double sv_atof(string_view sv)
{
    char tmp[DOUBLE_LIT_MAX_LEN + 1];
    memcpy(tmp, sv.ptr, sv.size);
    tmp[sv.size] = '\0';
    return atof(tmp);
}

#undef DOUBLE_LIT_MAX_LEN

BASEDEF const char* sv_dup(string_view sv)
{
    char* ptr = (char*)BASE_ALLOC(sizeof(char) * (sv.size + 1));
    memcpy(ptr, sv.ptr, sizeof(char) * sv.size);
    ptr[sv.size] = '\0';
    return ptr;
}

BASEDEF ssize_t sv_find(string_view sv, char c)
{
    for (size_t i = 0; i < sv.size; i++) {
        if (c == sv.ptr[i]) {
            return i;
        }
    }

    return -1;
}

BASEDEF ssize_t sv_find_sub(string_view sv, string_view substr)
{
    for (size_t i = 0; i < sv.size - substr.size + 1; i++) {
        size_t saved_i = i;

        for (size_t j = 0; j < substr.size; j++) {
            if (substr.ptr[j] != sv.ptr[i++]) {
                goto loopend;
            }
        }

        return saved_i;
loopend:
        i = saved_i;
    }

    return -1;
}

BASEDEF ssize_t sv_find_sub_cstr(string_view sv, const char* substr)
{
    size_t substr_len = strlen(substr);

    for (ssize_t i = 0; i < sv.size - substr_len + 1; i++) {
        size_t saved_i = i;

        for (size_t j = 0; j < substr_len; j++) {
            if (substr[j] != sv.ptr[i++]) {
                goto loopend;
            }
        }

        return saved_i;
loopend:
        i = saved_i;
    }

    return -1;
}

// STRING BUILDER
BASEDEF string_builder sb_new(size_t cap)
{
    string_builder sb;
    DA_INIT(&sb, 0, cap);
    return sb;
}

BASEDEF void sb_push_cstr(string_builder* sb, const char* str)
{
    while (*str) {
        DA_ADD(sb, *str++);
    }
}

BASEDEF char* sb_to_cstr(string_builder* sb)
{
    DA_ADD(sb, '\0');
    return sb->items;
}

BASEDEF void sb_destroy(string_builder* sb)
{
    DA_FREE(sb);
}

// HASH TABLE

BASEDEF int ht_load(const hash_table* ht)
{
    return (ht->len / ht->cap) * 100;
}

BASEDEF hash_table ht_make(ht_hash_func hash_func, ht_eq_func eq_func, size_t cap)
{
    ht_bucket** buckets = (ht_bucket**)calloc(cap, sizeof(ht_bucket*));

    return (hash_table) {
        .buckets = buckets,
        .len = 0,
        .cap = cap,
        .hash_function = hash_func,
        .equality_function = eq_func,
    };
}

static inline uint64_t __ht_idx_for(const hash_table* ht, const void* key)
{
    uint64_t hash = ht->hash_function(key);
    return hash % ht->cap;
}

BASEDEF void ht_add(hash_table* ht, void* key, void* value)
{
    if (ht_load(ht) >= 70) {
        ht->cap *= 2;
        ht->buckets = (ht_bucket**)realloc(ht->buckets, ht->cap);
    }

    size_t idx = __ht_idx_for(ht, key);
    ht_bucket* bucket = ht->buckets[idx];
    ht_kv_pair pair = { .key = key, .value = value };

    if (!bucket) {
        bucket = (ht_bucket*)BASE_ALLOC(sizeof(ht_bucket));
        DA_INIT(bucket, 0, 2);
        DA_ADD(bucket, pair);
        ht->buckets[idx] = bucket;
        ht->len++;
        return;
    }

    for (size_t i = 0; i < bucket->len; i++) {
        if (ht->equality_function(key, bucket->items[i].key)) {
            bucket->items[i].value = value;
            return;
        }
    }

    DA_ADD(bucket, pair);
}

BASEDEF void* ht_find(hash_table* ht, const void* key)
{
    size_t idx = __ht_idx_for(ht, key);
    ht_bucket* bucket = ht->buckets[idx];
    if (!bucket)
        return NULL;

    for (size_t i = 0; i < bucket->len; i++) {
        if (ht->equality_function(bucket->items[i].key, key))
            return bucket->items[i].value;
    }

    return NULL;
}

BASEDEF void ht_destroy(hash_table* ht)
{
    for (size_t i = 0; i < ht->cap; i++) {
        if (ht->buckets[i]) {
            DA_FREE(ht->buckets[i]);
            free(ht->buckets[i]);
        }
    }

    free(ht->buckets);
}

#endif
#endif