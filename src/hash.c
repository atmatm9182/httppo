#include "hash.h"

#include <string.h>

uint64_t hash_djb2(void const* ptr) {
    const char* str = (const char*)ptr;

    uint64_t hash = 5381;
    char c;

    while ((c = *str++)) hash = ((hash << 5) + hash) + c;

    return hash;
}

bool hash_str_eq(void const* lhs, void const* rhs) {
    return strcmp((const char*)lhs, (const char*)rhs) == 0;
}
