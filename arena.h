#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

#ifndef ARENA_DEBUG
#define ARENA_DEBUG 0
#endif

#define ARENA_POISON_BYTE 0xDD

/* Metadata is owned by the application, never by the arena's reset region. */
typedef struct Arena {
    unsigned char *base;
    size_t capacity;
    size_t used;
    size_t peak;
    size_t allocations;
    size_t lifetime_peak;
    size_t lifetime_allocations;
    size_t resets;
} Arena;

/* Borrow nonempty storage aligned to _Alignof(max_align_t). Never frees it. */
void arena_init(Arena *arena, void *memory, size_t capacity);

/* Dry-run one allocation, including padding. Shared with capacity sizing.
 * Nonzero requests require power-of-two alignment <= _Alignof(max_align_t).
 * Invalid input/overflow is fatal. A zero-size request returns offset unchanged.
 */
size_t arena_layout_push(size_t offset, size_t size, size_t alignment);

/* Zero size returns NULL without validating alignment or changing counters.
 * Other errors are fatal in ALL builds, before any arena mutation.
 */
void *arena_alloc(Arena *arena, size_t size, size_t alignment);

/* Invalidates every old allocation. Retains backing storage/lifetime counters.
 * ARENA_DEBUG=1 poisons previously used bytes, including alignment padding.
 */
void arena_reset(Arena *arena);

#endif