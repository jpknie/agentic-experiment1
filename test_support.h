#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Unlike assert, checks remain active with -DNDEBUG. */
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static inline void expect_fatal(void (*action)(void), const char *diagnostic)
{
    int descriptors[2];
    CHECK(pipe(descriptors) == 0);
    CHECK(fflush(NULL) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        close(descriptors[0]);
        if (dup2(descriptors[1], STDERR_FILENO) < 0) {
            _Exit(100);
        }
        close(descriptors[1]);
        action();
        _Exit(0); /* A fatal API must never get here. */
    }
    close(descriptors[1]);
    char output[1024] = {0};
    size_t used = 0;
    char chunk[128];
    for (;;) {
        ssize_t count = read(descriptors[0], chunk, sizeof(chunk));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        CHECK(count >= 0);
        if (count == 0) {
            break;
        }
        size_t copy = (size_t)count;
        if (copy > sizeof(output) - 1 - used) {
            copy = sizeof(output) - 1 - used;
        }
        memcpy(output + used, chunk, copy);
        used += copy;
    }
    close(descriptors[0]);
    int status;
    pid_t result;
    do {
        result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);
    CHECK(result == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == EXIT_FAILURE);
    if (strstr(output, diagnostic) == NULL) {
        fprintf(stderr, "expected diagnostic '%s', received: %s\n", diagnostic, output);
        CHECK(0);
    }
}

#endif