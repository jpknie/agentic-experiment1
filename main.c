#include "life.h"

#include <curses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

enum { BOARD_WIDTH = 40, BOARD_HEIGHT = 20, FRAME_MS = 100 };

static void report(FILE *log, const char *event, const Arena *arena, size_t generation)
{
    fprintf(log,
            "%s: generation=%zu capacity=%zu used=%zu peak=%zu allocations=%zu "
            "lifetime_peak=%zu lifetime_allocations=%zu resets=%zu\n",
            event, generation, arena->capacity, arena->used, arena->peak,
            arena->allocations, arena->lifetime_peak, arena->lifetime_allocations,
            arena->resets);
    /* Reports are visible while the program runs, not just at fclose. */
    (void)fflush(log);
}

static void draw(const Life *life, bool paused)
{
    int rows;
    int columns;
    getmaxyx(stdscr, rows, columns);
    erase();
    if (rows < BOARD_HEIGHT + 3 || columns < BOARD_WIDTH) {
        if (rows > 0 && columns > 1) {
            mvaddnstr(0, 0, "Need a terminal at least 40x23; q quits.", columns - 1);
        }
        refresh();
        return;
    }
    mvprintw(0, 0, "Life  generation=%zu  %s", life->generation,
             paused ? "paused" : "running");
    mvaddstr(1, 0, "space: pause  n: step  r: restart  q: quit");
    for (size_t y = 0; y < life->height; y++) {
        for (size_t x = 0; x < life->width; x++) {
            mvaddch((int)y + 2, (int)x,
                    (chtype)(life->current[y * life->width + x] ? '#' : '.'));
        }
    }
    refresh();
}

int main(void)
{
    size_t capacity = life_capacity(BOARD_WIDTH, BOARD_HEIGHT);
    /* The only application-owned heap allocation, during startup. */
    void *memory = malloc(capacity);
    if (memory == NULL) {
        fprintf(stderr, "startup: could not reserve %zu bytes\n", capacity);
        return EXIT_FAILURE;
    }
    /* stdio/ncurses internal allocations are outside the game's allocation rule. */
    FILE *log = fopen("arena-lifecycle.log", "w");
    if (log == NULL) {
        perror("startup: opening arena-lifecycle.log");
        free(memory);
        return EXIT_FAILURE;
    }
    fprintf(log, "startup: backing_allocations=1 backing_frees=0 reserved_bytes=%zu "
                 "ARENA_DEBUG=%d\n", capacity, ARENA_DEBUG);
    Arena arena;
    arena_init(&arena, memory, capacity);
    report(log, "arena-initialized", &arena, 0);
    Life *life = life_create(&arena, BOARD_WIDTH, BOARD_HEIGHT);
    life_seed_glider(life);
    report(log, "simulation-created", &arena, life->generation);

    int result = EXIT_SUCCESS;
    bool input_error = false;
    SCREEN *screen = newterm(NULL, stdout, stdin);
    if (screen == NULL) {
        fprintf(stderr, "UI: could not initialize ncurses (check TERM and terminal)\n");
        result = EXIT_FAILURE;
    } else if (cbreak() == ERR || noecho() == ERR || keypad(stdscr, TRUE) == ERR) {
        result = EXIT_FAILURE;
        input_error = true;
    } else {
        (void)curs_set(0); /* Not every terminal supports hiding the cursor. */
        bool paused = true;
        bool quit = false;
        while (!quit) {
            draw(life, paused);
            timeout(paused ? -1 : FRAME_MS);
            int key = getch();
            switch (key) {
            case 'q':
                quit = true;
                break;
            case ' ':
                paused = !paused;
                break;
            case 'n':
                paused = true;
                life_step(life);
                break;
            case 'r':
                report(log, "simulation-ended", &arena, life->generation);
                life = NULL; /* Drop the old pointer before its lifetime ends. */
                arena_reset(&arena);
                report(log, "arena-reset", &arena, 0);
                life = life_create(&arena, BOARD_WIDTH, BOARD_HEIGHT);
                life_seed_glider(life);
                report(log, "simulation-created", &arena, life->generation);
                paused = true;
                break;
            case ERR:
                if (!paused) {
                    life_step(life);
                }
                break;
            default:
                break; /* Includes terminal resize: redraw, never reallocate. */
            }
        }
    }

    if (screen != NULL) {
        endwin();
        delscreen(screen);
    }
    if (input_error) {
        fprintf(stderr, "UI: could not configure terminal input\n");
    }
    report(log, "simulation-ended", &arena, life->generation);
    life = NULL;
    free(memory); /* The backing block is released only at shutdown. */
    fprintf(log, "shutdown: backing_allocations=1 backing_frees=1 reserved_bytes=0\n");
    int log_error = ferror(log);
    if (fclose(log) != 0) {
        log_error = 1;
    }
    if (log_error) {
        fprintf(stderr, "diagnostics: could not write lifecycle report\n");
        result = EXIT_FAILURE;
    }
    puts("Lifecycle report: arena-lifecycle.log");
    return result;
}