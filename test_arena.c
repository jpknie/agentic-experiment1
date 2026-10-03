#include "arena.h"
#include "test_support.h"

#include <stdint.h>

static void test_alignment_and_layout(void)
{
    _Alignas(max_align_t) unsigned char memory[512];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    size_t planned = 0;
    size_t requests = 0;
    for (size_t alignment = 1; alignment <= _Alignof(max_align_t); alignment *= 2) {
        for (size_t size = 1; size <= 3; size++) {
            planned = arena_layout_push(planned, size, alignment);
            unsigned char *allocation = arena_alloc(&arena, size, alignment);
            CHECK((uintptr_t)allocation % alignment == 0);
            CHECK(allocation >= memory);
            CHECK(allocation + size <= memory + sizeof(memory));
            CHECK(arena.used == planned);
            memset(allocation, 0xAB, size);
            requests++;
        }
    }
    CHECK(arena.allocations == requests);
    CHECK(arena.lifetime_allocations == requests);
    CHECK(arena.peak == planned);
    CHECK(arena.lifetime_peak == planned);
}

static void test_reset_and_zero(void)
{
    _Alignas(max_align_t) unsigned char memory[128];
    memset(memory, 0x5A, sizeof(memory));
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    unsigned char *first = arena_alloc(&arena, 3, 1);
    (void)arena_alloc(&arena, 2, _Alignof(max_align_t));
    size_t old_used = arena.used;
    CHECK(arena_alloc(&arena, 0, 0) == NULL);
    CHECK(arena_alloc(&arena, 0, SIZE_MAX) == NULL);
    CHECK(arena_layout_push(SIZE_MAX, 0, 0) == SIZE_MAX);
    CHECK(arena.used == old_used);
    CHECK(arena.allocations == 2 && arena.lifetime_allocations == 2);
    CHECK(arena.resets == 0);
    arena_reset(&arena);
    CHECK(arena.used == 0 && arena.peak == 0 && arena.allocations == 0);
    CHECK(arena.capacity == sizeof(memory) && arena.base == memory);
    CHECK(arena.lifetime_peak == old_used && arena.lifetime_allocations == 2);
    CHECK(arena.resets == 1);
    for (size_t i = 0; i < sizeof(memory); i++) {
#if ARENA_DEBUG
        CHECK(memory[i] == (i < old_used ? ARENA_POISON_BYTE : 0x5A));
#else
        CHECK(memory[i] == 0x5A);
#endif
    }
    CHECK(arena_alloc(&arena, 3, 1) == first);
    CHECK(arena.allocations == 1 && arena.lifetime_allocations == 3);
    CHECK(arena.peak == 3 && arena.lifetime_peak == old_used);
    arena_reset(&arena);
    arena_reset(&arena); /* An empty reset still counts. */
    CHECK(arena.resets == 3);
}

static void test_exact_fit(void)
{
    _Alignas(max_align_t) unsigned char memory[32];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    CHECK(arena_alloc(&arena, sizeof(memory), 1) == memory);
    CHECK(arena.used == arena.capacity);
    CHECK(arena_alloc(&arena, 0, 0) == NULL);
}

static void exhaust(void)
{
    _Alignas(max_align_t) unsigned char memory[32];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    (void)arena_alloc(&arena, sizeof(memory), 1);
    (void)arena_alloc(&arena, 1, 1);
}

static void exhaust_with_padding(void)
{
    _Alignas(max_align_t) unsigned char memory[2];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    (void)arena_alloc(&arena, 1, 1);
    (void)arena_alloc(&arena, 1, 2);
}

static void invalid_alignment(void)
{
    (void)arena_layout_push(0, 1, 3);
}

static void zero_alignment(void)
{
    (void)arena_layout_push(0, 1, 0);
}

static void excessive_alignment(void)
{
    (void)arena_layout_push(0, 1, _Alignof(max_align_t) * 2);
}

static void overflow_padding(void)
{
    (void)arena_layout_push(SIZE_MAX, 1, 2);
}

static void overflow_size(void)
{
    (void)arena_layout_push(1, SIZE_MAX, 1);
}

static void invalid_memory(void)
{
    Arena arena;
    arena_init(&arena, NULL, 1);
}

static void invalid_metadata(void)
{
    _Alignas(max_align_t) unsigned char memory[32];
    arena_init(NULL, memory, sizeof(memory));
}

static void misaligned_memory(void)
{
    _Alignas(max_align_t) unsigned char memory[32];
    Arena arena;
    arena_init(&arena, memory + 1, sizeof(memory) - 1);
}

static void empty_capacity(void)
{
    _Alignas(max_align_t) unsigned char memory[32];
    Arena arena;
    arena_init(&arena, memory, 0);
}

static void counter_overflow(void)
{
    _Alignas(max_align_t) unsigned char memory[32];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    arena.lifetime_allocations = SIZE_MAX;
    (void)arena_alloc(&arena, 1, 1);
}

static void reset_overflow(void)
{
    _Alignas(max_align_t) unsigned char memory[32];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));
    arena.resets = SIZE_MAX;
    arena_reset(&arena);
}

int main(void)
{
    test_alignment_and_layout();
    test_reset_and_zero();
    test_exact_fit();
    expect_fatal(exhaust, "arena: exhausted");
    expect_fatal(exhaust_with_padding, "arena: exhausted");
    expect_fatal(invalid_alignment, "arena: invalid alignment");
    expect_fatal(zero_alignment, "arena: invalid alignment");
    expect_fatal(excessive_alignment, "arena: invalid alignment");
    expect_fatal(overflow_padding, "arena: size overflow");
    expect_fatal(overflow_size, "arena: size overflow");
    expect_fatal(invalid_memory, "arena: invalid initialization");
    expect_fatal(invalid_metadata, "arena: invalid initialization");
    expect_fatal(misaligned_memory, "arena: invalid initialization");
    expect_fatal(empty_capacity, "arena: invalid initialization");
    expect_fatal(counter_overflow, "arena: allocation counter overflow");
    expect_fatal(reset_overflow, "arena: reset counter overflow");
    printf("arena tests passed (ARENA_DEBUG=%d)\n", ARENA_DEBUG);
    return EXIT_SUCCESS;
}