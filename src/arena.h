#ifndef ARENA_H_
#define ARENA_H_

#include <stdint.h>

typedef struct ArenaRegion {
    struct ArenaRegion* next;
    void* data;
    size_t size;
    size_t off;
} ArenaRegion;

typedef struct {
    ArenaRegion* head;
    ArenaRegion* region_pool;
} Arena;

void* arena_alloc(Arena*, size_t);
void arena_free(Arena*);

#ifdef ARENA_H_IMPLEMENTATION

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)

#include <sys/mman.h>
#include <unistd.h>

#define ARENA_ALLOC_PAGE(sz) (mmap(NULL, (sz), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0))
#define ARENA_ALLOC_SMOL(sz) (sbrk((sz)))

#else

#error "Only UNIX-like systems are supported"

#endif // unix check

#define ARENA_PAGE_SIZE 4096

static inline uintptr_t arena_align_ptr(uintptr_t size, uintptr_t align) {
    return size + ((align - (size & (align - 1))) & (align - 1));
}

ArenaRegion* arena_alloc_region(Arena* arena, size_t size) {
    size = arena_align_ptr(size, ARENA_PAGE_SIZE);

    ArenaRegion* head = arena->region_pool;

    while (head) {
        if (head->size - head->off <= sizeof(ArenaRegion)) {
            ArenaRegion* region = (void*)((uint8_t*)head->data + head->off);
            region->data = ARENA_ALLOC_PAGE(size);
            region->size = size;
            region->off = 0;
            region->next = NULL;
            return region;
        }

        head = head->next;
    }

    head = ARENA_ALLOC_SMOL(sizeof(ArenaRegion));
    head->next = arena->region_pool;
    arena->region_pool = head;

    head->data = ARENA_ALLOC_PAGE(size);
    head->size = size;
    head->off = 0;
    head->next = NULL;
    return head;
}

void* arena_alloc(Arena* arena, size_t sz) {
    ArenaRegion* head = arena->head;

    sz = arena_align_ptr(sz, sizeof(void*));

    while (head) {
        if (head->size - head->off <= sz) {
            void* ptr = (void*)((uint8_t*)head->data + head->off);
            head->off += sz;
            return ptr;
        }

        head = head->next;
    }

    head = arena_alloc_region(arena, arena_align_ptr(sz, ARENA_PAGE_SIZE));
    head->next = arena->head;
    arena->head = head;

    void* ptr = (void*)((uint8_t*)head->data + head->off);
    head->off += sz;
    return ptr;
}

void arena_free(Arena* arena) {
    ArenaRegion* head = arena->head;
    while (head) {
        head->off = 0;
        head = head->next;
    }
}

#endif // ARENA_H_IMPLEMENTATION

#endif // ARENA_H_
