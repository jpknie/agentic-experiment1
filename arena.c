#include "arena.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message)
{
    fprintf(stderr, "arena: %s\n", message);
    exit(EXIT_FAILURE);
}

static void validate(const Arena *arena)
{
    if (arena == NULL || arena->base == NULL || arena->capacity == 0 ||
        arena->used > arena->capacity ||
        (uintptr_t)arena->base % _Alignof(max_align_t) != 0) {
        fail("invalid arena state");
    }
}

void arena_init(Arena *arena, void *memory, size_t capacity)
{
    if (arena == NULL || memory == NULL || capacity == 0 ||
        (uintptr_t)memory % _Alignof(max_align_t) != 0) {
        fail("invalid initialization (need nonempty, fundamentally aligned storage)");
    }
    *arena = (Arena){.base = memory, .capacity = capacity};
}

size_t arena_layout_push(size_t offset, size_t size, size_t alignment)
{
    if (size == 0) {
        return offset;
    }
    if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
        alignment > _Alignof(max_align_t)) {
        fail("invalid alignment");
    }
    size_t padding = (alignment - offset % alignment) % alignment;
    if (padding > SIZE_MAX - offset) {
        fail("size overflow while aligning");
    }
    size_t aligned = offset + padding;
    if (size > SIZE_MAX - aligned) {
        fail("size overflow while allocating");
    }
    return aligned + size;
}

void *arena_alloc(Arena *arena, size_t size, size_t alignment)
{
    if (size == 0) {
        return NULL;
    }
    validate(arena);
    size_t end = arena_layout_push(arena->used, size, alignment);
    if (end > arena->capacity) {
        fprintf(stderr,
                "arena: exhausted (request=%zu alignment=%zu used=%zu capacity=%zu)\n",
                size, alignment, arena->used, arena->capacity);
        exit(EXIT_FAILURE);
    }
    if (arena->allocations == SIZE_MAX || arena->lifetime_allocations == SIZE_MAX) {
        fail("allocation counter overflow");
    }

    /* Everything that can fail has been checked before these mutations. */
    void *result = arena->base + (end - size);
    arena->used = end;
    arena->allocations++;
    arena->lifetime_allocations++;
    if (end > arena->peak) {
        arena->peak = end;
    }
    if (end > arena->lifetime_peak) {
        arena->lifetime_peak = end;
    }
    return result;
}

void arena_reset(Arena *arena)
{
    validate(arena);
    if (arena->resets == SIZE_MAX) {
        fail("reset counter overflow");
    }
#if ARENA_DEBUG
    memset(arena->base, ARENA_POISON_BYTE, arena->used);
#endif
    arena->used = 0;
    arena->peak = 0;
    arena->allocations = 0;
    arena->resets++;
}