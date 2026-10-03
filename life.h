#ifndef LIFE_H
#define LIFE_H

#include "arena.h"

typedef struct Life {
    size_t width;
    size_t height;
    size_t cells;
    size_t generation;
    unsigned char *current;
    unsigned char *next;
} Life;

/* Exact capacity for a fresh arena: struct, current board, next board.
 * Zero dimensions and arithmetic overflow are fatal.
 */
size_t life_capacity(size_t width, size_t height);

/* Requires an empty arena; allocates and explicitly initializes all state.
 * The caller must discard old Life pointers before resetting its arena.
 */
Life *life_create(Arena *arena, size_t width, size_t height);

/* Fixed glider seed; requires at least 3x3. No allocation. */
void life_seed_glider(Life *life);

/* Synchronous Conway rules on a torus. Swap existing buffers, no allocation. */
void life_step(Life *life);

#endif