#ifndef ARENA_INCLUDE
#define ARENA_INCLUDE

#include <stdlib.h>

typedef struct {
    char* memory;
    size_t memory_size;
    size_t offset;
} Arena;

void* arena_alloc(Arena* arena, size_t size);
Arena arena_new(size_t cap);
void arena_free(Arena* arena);

#ifdef ARENA_IMPLEMENTATION

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

bool is_power_of_two(uintptr_t ptr) {
    return (ptr & (ptr - 1)) == 0;
}

uintptr_t align_ptr(uintptr_t ptr, uint8_t align) {
    assert(is_power_of_two(align));

    uintptr_t left =
        ptr & (align - 1);  // == ptr % align but faster because align is a power of two
    if (left != 0) {
        ptr += align - left;
    }

    return ptr;
}

void* arena_alloc(Arena* arena, size_t size) {
    char* curr = arena->memory + arena->offset;
    if (arena->memory_size - arena->offset < size) {
        return NULL;
    }

    curr = (char*)align_ptr((uintptr_t)curr, sizeof(uintptr_t));
    uintptr_t diff = (uintptr_t)curr - (uintptr_t)(arena->memory + arena->offset);
    arena->offset += size + diff;
    return curr;
}

Arena arena_new(size_t cap) {
    return (Arena){
        .memory = (char*)malloc(sizeof(char) * cap),
        .memory_size = cap,
        .offset = 0,
    };
}

void arena_free(Arena* arena) {
    arena->offset = 0;
}

#endif  // ARENA_IMPLEMENTATION

#endif  // ARENA_INCLUDE
