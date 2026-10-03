#include "life.h"
#include "test_support.h"

#include <stdint.h>

static void check_arena_unchanged(const Arena *arena, const Arena *before)
{
    CHECK(arena->base == before->base && arena->capacity == before->capacity);
    CHECK(arena->used == before->used && arena->peak == before->peak);
    CHECK(arena->allocations == before->allocations);
    CHECK(arena->lifetime_peak == before->lifetime_peak);
    CHECK(arena->lifetime_allocations == before->lifetime_allocations);
    CHECK(arena->resets == before->resets);
}

static void test_block_and_blinker(void)
{
    _Alignas(max_align_t) unsigned char memory[512];
    Arena arena;
    arena_init(&arena, memory, life_capacity(5, 5));
    Life *life = life_create(&arena, 5, 5);
    CHECK(arena.used == arena.capacity && arena.allocations == 3);
    life->current[1 * 5 + 1] = 1;
    life->current[1 * 5 + 2] = 1;
    life->current[2 * 5 + 1] = 1;
    life->current[2 * 5 + 2] = 1;
    unsigned char block[25];
    memcpy(block, life->current, sizeof(block));
    Arena before = arena;
    unsigned char *first = life->current;
    unsigned char *second = life->next;
    life_step(life);
    CHECK(life->current == second && life->next == first);
    CHECK(memcmp(life->current, block, sizeof(block)) == 0);
    life_step(life);
    CHECK(life->current == first && life->next == second);
    CHECK(memcmp(life->current, block, sizeof(block)) == 0);
    check_arena_unchanged(&arena, &before);

    life = NULL;
    arena_reset(&arena);
    life = life_create(&arena, 5, 5);
    life->current[2 * 5 + 1] = 1;
    life->current[2 * 5 + 2] = 1;
    life->current[2 * 5 + 3] = 1;
    unsigned char horizontal[25];
    memcpy(horizontal, life->current, sizeof(horizontal));
    life_step(life);
    for (size_t y = 0; y < 5; y++) {
        for (size_t x = 0; x < 5; x++) {
            CHECK(life->current[y * 5 + x] == (x == 2 && y >= 1 && y <= 3));
        }
    }
    life_step(life);
    CHECK(memcmp(life->current, horizontal, sizeof(horizontal)) == 0);
}

static void test_glider_wrap(void)
{
    _Alignas(max_align_t) unsigned char memory[512];
    Arena arena;
    arena_init(&arena, memory, life_capacity(5, 5));
    Life *life = life_create(&arena, 5, 5);
    life_seed_glider(life);
    unsigned char initial[25];
    memcpy(initial, life->current, sizeof(initial));
    for (size_t step = 0; step < 20; step++) {
        life_step(life);
        if (life->generation % 4 == 0) {
            size_t shift = (life->generation / 4) % 5;
            for (size_t y = 0; y < 5; y++) {
                for (size_t x = 0; x < 5; x++) {
                    CHECK(life->current[((y + shift) % 5) * 5 + (x + shift) % 5]
                          == initial[y * 5 + x]);
                }
            }
        }
    }
    CHECK(memcmp(life->current, initial, sizeof(initial)) == 0);
}

static void test_small_board(void)
{
    _Alignas(max_align_t) unsigned char memory[128];
    Arena arena;
    arena_init(&arena, memory, life_capacity(1, 1));
    Life *life = life_create(&arena, 1, 1);
    life->current[0] = 1;
    life_step(life); /* Eight wrapped neighbor positions all reference the cell. */
    CHECK(life->current[0] == 0);
}

static void test_lifecycle(void)
{
    size_t capacity = life_capacity(40, 20);
    void *memory = malloc(capacity); /* The one startup backing allocation. */
    CHECK(memory != NULL);
    Arena arena;
    arena_init(&arena, memory, capacity);
    for (size_t simulation = 0; simulation < 24; simulation++) {
        Life *life = life_create(&arena, 40, 20);
        CHECK((void *)life == memory);
        CHECK(arena.used == capacity && arena.peak == capacity);
        CHECK(arena.allocations == 3);
        CHECK(arena.lifetime_allocations == (simulation + 1) * 3);
        CHECK(arena.resets == simulation);
        CHECK(life->generation == 0);
        for (size_t i = 0; i < life->cells; i++) {
            CHECK(life->current[i] == 0 && life->next[i] == 0);
        }
        life_seed_glider(life);
        Arena before = arena;
        for (size_t generation = 0; generation < 200; generation++) {
            life_step(life);
            check_arena_unchanged(&arena, &before);
            CHECK(life->generation == generation + 1);
        }
        life = NULL; /* Discard the simulation pointer BEFORE invalidation. */
        arena_reset(&arena);
        CHECK(arena.used == 0 && arena.allocations == 0 && arena.peak == 0);
        CHECK(arena.lifetime_peak == capacity);
    }
    CHECK(arena.resets == 24 && arena.lifetime_allocations == 72);
    free(memory); /* Shutdown release, never between simulations. */
    puts("lifecycle: 24 simulations, 4800 generations, 72 arena allocations, "
         "one backing block, unchanged generation-time counters");
}

static void zero_dimensions(void)
{
    (void)life_capacity(0, 5);
}

static void multiply_overflow(void)
{
    (void)life_capacity(SIZE_MAX, 2);
}

static void layout_overflow(void)
{
    (void)life_capacity(SIZE_MAX, 1);
}

static void recreate_without_reset(void)
{
    _Alignas(max_align_t) unsigned char memory[512];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    (void)life_create(&arena, 5, 5);
    (void)life_create(&arena, 5, 5);
}

static void generation_overflow(void)
{
    _Alignas(max_align_t) unsigned char memory[512];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    Life *life = life_create(&arena, 5, 5);
    life->generation = SIZE_MAX;
    life_step(life);
}

int main(void)
{
    test_block_and_blinker();
    test_glider_wrap();
    test_small_board();
    test_lifecycle();
    expect_fatal(zero_dimensions, "life: board dimensions must be nonzero");
    expect_fatal(multiply_overflow, "life: board size overflow");
    expect_fatal(layout_overflow, "arena: size overflow");
    expect_fatal(recreate_without_reset, "life: simulation creation requires an empty arena");
    expect_fatal(generation_overflow, "life: generation counter overflow");
    printf("life tests passed (ARENA_DEBUG=%d)\n", ARENA_DEBUG);
    return EXIT_SUCCESS;
}