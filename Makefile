CC ?= cc
AR ?= ar
AMIGA_CC ?= m68k-amigaos-gcc
AMIGA_AR ?= m68k-amigaos-ar
ADFTOOL ?= gadf
RM = rm -rf

CFLAGS ?= -std=c89 -Wall -Wextra -Werror -pedantic -Iinclude -Isrc
LDFLAGS ?=
AMIGA_CFLAGS ?= -O2 -std=gnu89 -Wall -Wextra -Werror -Iinclude -Isrc -m68000 -DANA_TARGET_AMIGA
AMIGA_LDFLAGS ?= -mcrt=nix13 -lamiga
BUILD_DIR ?= build
AMIGA_BUILD_DIR ?= $(BUILD_DIR)/amiga
ADF_DIR ?= $(BUILD_DIR)/adf
AMIGA_FAST_BUILD_DIR := $(BUILD_DIR)/amiga-fast

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

TOOL_TEST_HELPERS := \
	$(BUILD_DIR)/tests/ana_convert_image_test

EXAMPLE_BINS := \
	$(BUILD_DIR)/examples/hello/hello \
	$(BUILD_DIR)/examples/invaders/invaders

TOOL_BINS := $(BUILD_DIR)/tools/ana-convert/ana-convert

AMIGA_OBJS := $(ANA_SRCS:%.c=$(AMIGA_BUILD_DIR)/%.o)
AMIGA_LIBANA := $(AMIGA_BUILD_DIR)/libana.a
AMIGA_FAST_OBJS := $(ANA_SRCS:%.c=$(AMIGA_FAST_BUILD_DIR)/%.o)
AMIGA_FAST_LIBANA := $(AMIGA_FAST_BUILD_DIR)/libana.a

AMIGA_EXAMPLE_BINS := \
	$(AMIGA_BUILD_DIR)/examples/hello/hello \
	$(AMIGA_BUILD_DIR)/examples/invaders/invaders

AMIGA_INVADERS_DEBUG_BIN := $(AMIGA_BUILD_DIR)/examples/invaders-debug/invaders
AMIGA_INVADERS_FAST_BIN := $(AMIGA_FAST_BUILD_DIR)/examples/invaders-fast/invaders

ADF_FILES := \
	$(ADF_DIR)/hello.adf \
	$(ADF_DIR)/invaders.adf

INVADERS_DEBUG_ADF := $(ADF_DIR)/invaders-debug.adf
INVADERS_FAST_ADF := $(ADF_DIR)/invaders-fast.adf

.PHONY: all lib examples tools test amiga-lib amiga-examples amiga-invaders-debug amiga-invaders-fast adfs invaders-debug-adf invaders-fast-adf clean

all: lib examples tools

lib: $(LIBANA)

examples: $(EXAMPLE_BINS)

tools: $(TOOL_BINS)

amiga-lib: $(AMIGA_LIBANA)

amiga-examples: $(AMIGA_EXAMPLE_BINS)

amiga-invaders-debug: $(AMIGA_INVADERS_DEBUG_BIN)

amiga-invaders-fast: $(AMIGA_INVADERS_FAST_BIN)

adfs: $(ADF_FILES)

invaders-debug-adf: $(INVADERS_DEBUG_ADF)

invaders-fast-adf: $(INVADERS_FAST_ADF)

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

$(AMIGA_LIBANA): $(AMIGA_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_FAST_LIBANA): $(AMIGA_FAST_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) -c $< -o $@

$(AMIGA_FAST_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) -DANA_AMIGA_DIRECT_PRESENT -c $< -o $@

$(AMIGA_BUILD_DIR)/examples/hello/hello: examples/hello/main.c $(AMIGA_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $< $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_BUILD_DIR)/examples/invaders/invaders: examples/invaders/main.c $(AMIGA_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $< $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INVADERS_DEBUG_BIN): examples/invaders/main.c $(AMIGA_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) -DANA_INVADERS_DEBUG_STATS $< $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INVADERS_FAST_BIN): examples/invaders/main.c $(AMIGA_FAST_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) -DANA_INVADERS_DEBUG_STATS $< $(AMIGA_FAST_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(ADF_DIR)/hello.adf: $(AMIGA_BUILD_DIR)/examples/hello/hello
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAHello

$(ADF_DIR)/invaders.adf: $(AMIGA_BUILD_DIR)/examples/invaders/invaders
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvaders

$(INVADERS_DEBUG_ADF): $(AMIGA_INVADERS_DEBUG_BIN)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvDbg

$(INVADERS_FAST_ADF): $(AMIGA_INVADERS_FAST_BIN)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvFast

$(BUILD_DIR)/tests/%: tests/%.c $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< $(LIBANA) $(LDFLAGS) -o $@

test: $(TEST_BINS) $(TOOL_BINS) $(TOOL_TEST_HELPERS) $(EXAMPLE_BINS)
	set -e; for test_bin in $(TEST_BINS); do $$test_bin; done
	set -e; for example_bin in $(EXAMPLE_BINS); do $$example_bin >/dev/null; done
	ANA_CONVERT=$(BUILD_DIR)/tools/ana-convert/ana-convert ANA_CONVERT_PROBE=$(BUILD_DIR)/tests/ana_convert_image_test sh tests/ana_convert_test.sh

clean:
	$(RM) $(BUILD_DIR)
