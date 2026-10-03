#include "life.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message)
{
    fprintf(stderr, "life: %s\n", message);
    exit(EXIT_FAILURE);
}

static size_t cell_count(size_t width, size_t height)
{
    if (width == 0 || height == 0) {
        fail("board dimensions must be nonzero");
    }
    if (width > SIZE_MAX / height) {
        fail("board size overflow");
    }
    return width * height;
}

size_t life_capacity(size_t width, size_t height)
{
    size_t cells = cell_count(width, height);
    size_t offset = arena_layout_push(0, sizeof(Life), _Alignof(Life));
    offset = arena_layout_push(offset, cells, _Alignof(unsigned char));
    return arena_layout_push(offset, cells, _Alignof(unsigned char));
}

Life *life_create(Arena *arena, size_t width, size_t height)
{
    if (arena == NULL || arena->used != 0) {
        fail("simulation creation requires an empty arena");
    }
    /* Check the complete layout before making any allocations. */
    (void)life_capacity(width, height);
    size_t cells = cell_count(width, height);
    Life *life = arena_alloc(arena, sizeof(*life), _Alignof(Life));
    unsigned char *current = arena_alloc(arena, cells, _Alignof(unsigned char));
    unsigned char *next = arena_alloc(arena, cells, _Alignof(unsigned char));
    *life = (Life){
        .width = width, .height = height, .cells = cells,
        .generation = 0, .current = current, .next = next
    };
    memset(current, 0, cells);
    memset(next, 0, cells);
    return life;
}

void life_seed_glider(Life *life)
{
    if (life->width < 3 || life->height < 3) {
        fail("glider seed requires at least a 3x3 board");
    }
    memset(life->current, 0, life->cells);
    memset(life->next, 0, life->cells);
    life->generation = 0;
    life->current[1] = 1;
    life->current[life->width + 2] = 1;
    life->current[2 * life->width] = 1;
    life->current[2 * life->width + 1] = 1;
    life->current[2 * life->width + 2] = 1;
}

void life_step(Life *life)
{
    if (life->generation == SIZE_MAX) {
        fail("generation counter overflow");
    }
    for (size_t y = 0; y < life->height; y++) {
        size_t above = y == 0 ? life->height - 1 : y - 1;
        size_t below = y == life->height - 1 ? 0 : y + 1;
        for (size_t x = 0; x < life->width; x++) {
            size_t left = x == 0 ? life->width - 1 : x - 1;
            size_t right = x == life->width - 1 ? 0 : x + 1;
            unsigned neighbors =
                (unsigned)life->current[above * life->width + left] +
                life->current[above * life->width + x] +
                life->current[above * life->width + right] +
                life->current[y * life->width + left] +
                life->current[y * life->width + right] +
                life->current[below * life->width + left] +
                life->current[below * life->width + x] +
                life->current[below * life->width + right];
            size_t index = y * life->width + x;
            life->next[index] = (unsigned char)(neighbors == 3 ||
                (neighbors == 2 && life->current[index] != 0));
        }
    }
    unsigned char *previous = life->current;
    life->current = life->next;
    life->next = previous;
    life->generation++;
}