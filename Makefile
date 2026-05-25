CC ?= cc
AR ?= ar
HOST_NATIVE_CC := $(shell command -v x86_64-linux-gnu-gcc 2>/dev/null || command -v cc 2>/dev/null || echo cc)
HOST_NATIVE_LDFLAGS := $(shell if command -v x86_64-linux-gnu-gcc >/dev/null 2>&1 && command -v x86_64-linux-gnu-ld.bfd >/dev/null 2>&1; then echo -fuse-ld=bfd; fi)
HOST_CC ?= $(HOST_NATIVE_CC)
AMIGA_CC ?= m68k-amigaos-gcc
AMIGA_AR ?= m68k-amigaos-ar
AMIGA_AS ?= $(shell command -v vasmm68k_mot 2>/dev/null || command -v /opt/m68k-amigaos/bin/vasmm68k_mot 2>/dev/null || echo vasmm68k_mot)
ADFTOOL ?= gadf
RM = rm -rf

CFLAGS ?= -std=c89 -Wall -Wextra -Werror -pedantic -Iinclude -Isrc
LDFLAGS ?=
HOST_CFLAGS ?= $(CFLAGS)
HOST_LDFLAGS ?= $(LDFLAGS) $(HOST_NATIVE_LDFLAGS)
TOOL_CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -Iinclude -Isrc
AMIGA_BASE_CFLAGS ?= -O2 -std=gnu89 -Wall -Wextra -Werror -Iinclude -Isrc -m68000 -DANA_TARGET_AMIGA
AMIGA_PRESENT_CFLAGS ?= -DANA_AMIGA_DIRECT_PRESENT
AMIGA_CFLAGS ?= $(AMIGA_BASE_CFLAGS) $(AMIGA_PRESENT_CFLAGS)
AMIGA_SYNC_CFLAGS ?= $(AMIGA_BASE_CFLAGS) -DANA_AMIGA_DIRECT_PRESENT -DANA_AMIGA_DIRECT_PRESENT_SYNC
AMIGA_ASFLAGS ?= -quiet -Fhunk -m68000 -phxass
AMIGA_LDFLAGS ?= -mcrt=nix13 -lamiga
BUILD_DIR ?= build
AMIGA_BUILD_DIR ?= $(BUILD_DIR)/amiga
ADF_DIR ?= $(BUILD_DIR)/adf
AMIGA_SYNC_BUILD_DIR := $(BUILD_DIR)/amiga-sync
RELEASE_VERSION ?= $(shell grep ANA_VERSION_STRING include/ana/ana_version.h | cut -d '"' -f 2)
RELEASE_NAME ?= ana-$(RELEASE_VERSION)
RELEASE_ROOT := $(BUILD_DIR)/release
RELEASE_DIR := $(RELEASE_ROOT)/$(RELEASE_NAME)
RELEASE_ARCHIVE := $(RELEASE_ROOT)/$(RELEASE_NAME).tar.gz
RELEASE_FILES := README.md LICENSE Makefile include src tools examples tests docs .github

ANA_SRCS := \
	src/core/ana_core.c \
	src/core/ana_platform.c \
	src/core/ana_helpers.c \
	src/core/ana_result.c \
	src/gfx/ana_gfx.c \
	src/input/ana_input.c \
	src/sound/ana_sound.c
ANA_OBJS := $(ANA_SRCS:%.c=$(BUILD_DIR)/%.o)
LIBANA := $(BUILD_DIR)/libana.a
ANA_AMIGA_ASM_SRCS := \
	src/sound/ana_ptplayer_wrap.asm \
	src/sound/vendor/ptplayer/ptplayer.asm

TEST_BINS := \
	$(BUILD_DIR)/tests/platform_profile_test \
	$(BUILD_DIR)/tests/runtime_loop_test \
	$(BUILD_DIR)/tests/gfx_test \
	$(BUILD_DIR)/tests/input_test \
	$(BUILD_DIR)/tests/helpers_test \
	$(BUILD_DIR)/tests/sound_test

TOOL_TEST_HELPERS := \
	$(BUILD_DIR)/tests/ana_convert_image_test

EXAMPLE_BINS := \
	$(BUILD_DIR)/examples/hello/hello \
	$(BUILD_DIR)/examples/invaders/invaders

INVADERS_SRCS := \
	examples/invaders/main.c \
	examples/invaders/invaders_game.c \
	examples/invaders/invaders_render.c \
	examples/invaders/invaders_assets.c
INVADERS_HEADERS := \
	examples/invaders/invaders_assets.h \
	examples/invaders/invaders_game.h \
	examples/invaders/invaders_internal.h
INVADERS_ASSET_BUILD_DIR := $(BUILD_DIR)/assets/invaders
INVADERS_ASSET_DIR := $(INVADERS_ASSET_BUILD_DIR)/assets
INVADERS_ASSET_MANIFEST := examples/invaders/assets/assets.ana
INVADERS_ASSET_SOURCES := $(wildcard examples/invaders/assets/*)
INVADERS_ASSET_BUILDER := \
	$(BUILD_DIR)/examples/invaders-build-assets/build-assets
INVADERS_ASSET_STAMP := $(INVADERS_ASSET_BUILD_DIR)/.stamp

TOOL_BINS := $(BUILD_DIR)/tools/ana-convert/ana-convert

AMIGA_ASM_OBJS := $(ANA_AMIGA_ASM_SRCS:%.asm=$(AMIGA_BUILD_DIR)/%.o)
AMIGA_OBJS := $(ANA_SRCS:%.c=$(AMIGA_BUILD_DIR)/%.o) $(AMIGA_ASM_OBJS)
AMIGA_LIBANA := $(AMIGA_BUILD_DIR)/libana.a
AMIGA_SYNC_ASM_OBJS := $(ANA_AMIGA_ASM_SRCS:%.asm=$(AMIGA_SYNC_BUILD_DIR)/%.o)
AMIGA_SYNC_OBJS := $(ANA_SRCS:%.c=$(AMIGA_SYNC_BUILD_DIR)/%.o) $(AMIGA_SYNC_ASM_OBJS)
AMIGA_SYNC_LIBANA := $(AMIGA_SYNC_BUILD_DIR)/libana.a

AMIGA_EXAMPLE_BINS := \
	$(AMIGA_BUILD_DIR)/examples/hello/hello \
	$(AMIGA_BUILD_DIR)/examples/invaders/invaders

AMIGA_INVADERS_DEBUG_BIN := $(AMIGA_BUILD_DIR)/examples/invaders-debug/invaders
AMIGA_INVADERS_SYNC_BIN := $(AMIGA_SYNC_BUILD_DIR)/examples/invaders-sync/invaders

ADF_FILES := \
	$(ADF_DIR)/hello.adf \
	$(ADF_DIR)/invaders.adf

INVADERS_DEBUG_ADF := $(ADF_DIR)/invaders-debug.adf
INVADERS_SYNC_ADF := $(ADF_DIR)/invaders-sync.adf

.PHONY: all lib examples assets examples/invaders-assets invaders-assets tools test amiga-lib amiga-examples amiga-invaders-debug amiga-invaders-sync adfs invaders-debug-adf invaders-sync-adf release-package clean-assets clean

all: lib examples tools

lib: $(LIBANA)

examples: $(EXAMPLE_BINS)

assets: invaders-assets

examples/invaders-assets: $(INVADERS_ASSET_STAMP)

invaders-assets: $(INVADERS_ASSET_STAMP)

tools: $(TOOL_BINS)

amiga-lib: $(AMIGA_LIBANA)

amiga-examples: $(AMIGA_EXAMPLE_BINS)

amiga-invaders-debug: $(AMIGA_INVADERS_DEBUG_BIN)

amiga-invaders-sync: $(AMIGA_INVADERS_SYNC_BIN)

adfs: $(ADF_FILES)

invaders-debug-adf: $(INVADERS_DEBUG_ADF)

invaders-sync-adf: $(INVADERS_SYNC_ADF)

release-package:
	$(RM) $(RELEASE_ROOT)
	mkdir -p $(RELEASE_DIR)
	cp -R $(RELEASE_FILES) $(RELEASE_DIR)/
	tar -C $(RELEASE_ROOT) -czf $(RELEASE_ARCHIVE) $(RELEASE_NAME)
	@echo "Wrote $(RELEASE_ARCHIVE)"

$(LIBANA): $(ANA_OBJS)
	mkdir -p $(@D)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/examples/hello/hello: examples/hello/main.c $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< $(LIBANA) $(LDFLAGS) -o $@

$(INVADERS_ASSET_BUILDER): examples/invaders/build_assets.c
	mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) $< $(HOST_LDFLAGS) -o $@

$(INVADERS_ASSET_STAMP): $(INVADERS_ASSET_BUILDER) $(TOOL_BINS) $(INVADERS_ASSET_SOURCES)
	mkdir -p $(INVADERS_ASSET_DIR)
	$(INVADERS_ASSET_BUILDER) $(INVADERS_ASSET_DIR)
	$(BUILD_DIR)/tools/ana-convert/ana-convert build $(INVADERS_ASSET_MANIFEST) --out $(INVADERS_ASSET_DIR)
	touch $@

$(BUILD_DIR)/examples/invaders/invaders: $(INVADERS_SRCS) $(INVADERS_HEADERS) $(LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INVADERS_SRCS) $(LIBANA) $(LDFLAGS) -o $@

$(BUILD_DIR)/tools/ana-convert/ana-convert: tools/ana-convert/main.c tools/ana-convert/vendor/stb_image.h
	mkdir -p $(@D)
	$(HOST_CC) $(TOOL_CFLAGS) $< $(HOST_LDFLAGS) -o $@

$(AMIGA_LIBANA): $(AMIGA_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_SYNC_LIBANA): $(AMIGA_SYNC_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) -c $< -o $@

$(AMIGA_SYNC_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_SYNC_CFLAGS) -c $< -o $@

$(AMIGA_BUILD_DIR)/src/sound/vendor/ptplayer/ptplayer.o: src/sound/vendor/ptplayer/ptplayer.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -DOSCOMPAT=1 -o $@ $<

$(AMIGA_SYNC_BUILD_DIR)/src/sound/vendor/ptplayer/ptplayer.o: src/sound/vendor/ptplayer/ptplayer.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -DOSCOMPAT=1 -o $@ $<

$(AMIGA_BUILD_DIR)/src/sound/ana_ptplayer_wrap.o: src/sound/ana_ptplayer_wrap.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -o $@ $<

$(AMIGA_SYNC_BUILD_DIR)/src/sound/ana_ptplayer_wrap.o: src/sound/ana_ptplayer_wrap.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -o $@ $<

$(AMIGA_BUILD_DIR)/examples/hello/hello: examples/hello/main.c $(AMIGA_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $< $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_BUILD_DIR)/examples/invaders/invaders: $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $(INVADERS_SRCS) $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INVADERS_DEBUG_BIN): $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) -DANA_INVADERS_DEBUG_STATS $(INVADERS_SRCS) $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INVADERS_SYNC_BIN): $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_SYNC_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_SYNC_CFLAGS) -DANA_INVADERS_DEBUG_STATS $(INVADERS_SRCS) $(AMIGA_SYNC_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(ADF_DIR)/hello.adf: $(AMIGA_BUILD_DIR)/examples/hello/hello
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAHello

$(ADF_DIR)/invaders.adf: $(AMIGA_BUILD_DIR)/examples/invaders/invaders $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvaders $(INVADERS_ASSET_DIR)

$(INVADERS_DEBUG_ADF): $(AMIGA_INVADERS_DEBUG_BIN) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvDbg $(INVADERS_ASSET_DIR)

$(INVADERS_SYNC_ADF): $(AMIGA_INVADERS_SYNC_BIN) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvSync $(INVADERS_ASSET_DIR)

$(BUILD_DIR)/tests/%: tests/%.c $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< $(LIBANA) $(LDFLAGS) -o $@

test: $(TEST_BINS) $(TOOL_BINS) $(TOOL_TEST_HELPERS) $(EXAMPLE_BINS)
	set -e; for test_bin in $(TEST_BINS); do $$test_bin; done
	set -e; for example_bin in $(EXAMPLE_BINS); do $$example_bin >/dev/null; done
	ANA_CONVERT=$(BUILD_DIR)/tools/ana-convert/ana-convert ANA_CONVERT_PROBE=$(BUILD_DIR)/tests/ana_convert_image_test sh tests/ana_convert_test.sh

clean:
	$(RM) $(BUILD_DIR)

clean-assets:
	$(RM) $(BUILD_DIR)/assets $(INVADERS_ASSET_BUILDER) $(TOOL_BINS)
