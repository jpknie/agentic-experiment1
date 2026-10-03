CC ?= cc
PKG_CONFIG ?= pkg-config
NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncurses)
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncurses)

WARNINGS := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
COMMON := -std=c11 $(WARNINGS)
FLAGS_debug := -O0 -g3 -DARENA_DEBUG=1
FLAGS_release := -O2 -DNDEBUG -DARENA_DEBUG=0
FLAGS_sanitize := -O1 -g3 -DARENA_DEBUG=1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

.PHONY: all debug release sanitize test test-debug test-release test-sanitize check run clean
all: debug
debug: build/debug/life
release: build/release/life
sanitize: build/sanitize/life

build/debug build/release build/sanitize:
	mkdir -p $@

# Separate output directories prevent mixing flags between configurations.
define CONFIGURATION
build/$(1)/%.o: %.c arena.h life.h | build/$(1)
	$$(CC) $$(CPPFLAGS) $$(NCURSES_CFLAGS) $$(COMMON) $$(FLAGS_$(1)) $$(CFLAGS) -MMD -MP -c $$< -o $$@

build/$(1)/life: build/$(1)/main.o build/$(1)/arena.o build/$(1)/life.o
	$$(CC) $$(FLAGS_$(1)) $$(LDFLAGS) $$^ $$(NCURSES_LIBS) $$(LDLIBS) -o $$@

build/$(1)/test_arena: build/$(1)/test_arena.o build/$(1)/arena.o
	$$(CC) $$(FLAGS_$(1)) $$(LDFLAGS) $$^ $$(LDLIBS) -o $$@

build/$(1)/test_life: build/$(1)/test_life.o build/$(1)/arena.o build/$(1)/life.o
	$$(CC) $$(FLAGS_$(1)) $$(LDFLAGS) $$^ $$(LDLIBS) -o $$@

test-$(1): build/$(1)/test_arena build/$(1)/test_life
	./build/$(1)/test_arena
	./build/$(1)/test_life
endef

$(foreach mode,debug release sanitize,$(eval $(call CONFIGURATION,$(mode))))

test: test-debug test-release
check: test test-sanitize
run: debug
	./build/debug/life
clean:
	rm -rf build

-include $(wildcard build/*/*.d)