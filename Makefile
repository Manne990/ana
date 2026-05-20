CC ?= cc
AR ?= ar
RM = rm -rf

CFLAGS ?= -std=c89 -Wall -Wextra -Werror -pedantic -Iinclude -Isrc
LDFLAGS ?=
BUILD_DIR ?= build

ANA_SRCS := \
	src/core/ana_core.c \
	src/core/ana_platform.c \
	src/core/ana_result.c \
	src/gfx/ana_gfx.c \
	src/input/ana_input.c
ANA_OBJS := $(ANA_SRCS:%.c=$(BUILD_DIR)/%.o)
LIBANA := $(BUILD_DIR)/libana.a

TEST_BINS := \
	$(BUILD_DIR)/tests/platform_profile_test \
	$(BUILD_DIR)/tests/runtime_loop_test \
	$(BUILD_DIR)/tests/gfx_test \
	$(BUILD_DIR)/tests/input_test

EXAMPLE_BINS := \
	$(BUILD_DIR)/examples/hello/hello \
	$(BUILD_DIR)/examples/invaders/invaders

TOOL_BINS := $(BUILD_DIR)/tools/ana-convert/ana-convert

.PHONY: all lib examples tools test clean

all: lib examples tools

lib: $(LIBANA)

examples: $(EXAMPLE_BINS)

tools: $(TOOL_BINS)

$(LIBANA): $(ANA_OBJS)
	mkdir -p $(@D)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/examples/hello/hello: examples/hello/main.c $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< $(LIBANA) $(LDFLAGS) -o $@

$(BUILD_DIR)/examples/invaders/invaders: examples/invaders/main.c $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< $(LIBANA) $(LDFLAGS) -o $@

$(BUILD_DIR)/tools/ana-convert/ana-convert: tools/ana-convert/main.c $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< $(LIBANA) $(LDFLAGS) -o $@

$(BUILD_DIR)/tests/%: tests/%.c $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< $(LIBANA) $(LDFLAGS) -o $@

test: $(TEST_BINS)
	set -e; for test_bin in $(TEST_BINS); do $$test_bin; done

clean:
	$(RM) $(BUILD_DIR)
