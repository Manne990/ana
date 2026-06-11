CC ?= cc
AR ?= ar
HOST_NATIVE_CC := $(shell command -v x86_64-linux-gnu-gcc 2>/dev/null || command -v cc 2>/dev/null || echo cc)
HOST_NATIVE_LDFLAGS := $(shell if command -v x86_64-linux-gnu-gcc >/dev/null 2>&1 && command -v x86_64-linux-gnu-ld.bfd >/dev/null 2>&1; then echo -fuse-ld=bfd; fi)
HOST_CC ?= $(HOST_NATIVE_CC)
AMIGA_DOCKER_IMAGE ?= amigadev/crosstools:m68k-amigaos-gcc10_amd64
AMIGA_DOCKER_RUN ?= docker run --rm --platform linux/amd64 --user $(shell id -u):$(shell id -g) -v "$(CURDIR):/work" -w /work $(AMIGA_DOCKER_IMAGE)
AMIGA_NATIVE_CC := $(shell command -v m68k-amigaos-gcc 2>/dev/null)
AMIGA_NATIVE_AR := $(shell command -v m68k-amigaos-ar 2>/dev/null)
AMIGA_NATIVE_AS := $(shell command -v vasmm68k_mot 2>/dev/null || command -v /opt/m68k-amigaos/bin/vasmm68k_mot 2>/dev/null)
AMIGA_CC ?= $(if $(AMIGA_NATIVE_CC),$(AMIGA_NATIVE_CC),$(AMIGA_DOCKER_RUN) m68k-amigaos-gcc)
AMIGA_AR ?= $(if $(AMIGA_NATIVE_AR),$(AMIGA_NATIVE_AR),$(AMIGA_DOCKER_RUN) m68k-amigaos-ar)
AMIGA_AS ?= $(if $(AMIGA_NATIVE_AS),$(AMIGA_NATIVE_AS),$(AMIGA_DOCKER_RUN) vasmm68k_mot)
ADFTOOL ?= $(shell command -v gadf 2>/dev/null || command -v $(HOME)/go/bin/gadf 2>/dev/null || echo gadf)
FS_UAE ?= /Applications/FS-UAE.app/Contents/MacOS/fs-uae
RM = rm -rf

CFLAGS ?= -std=c89 -Wall -Wextra -Werror -pedantic -Iinclude -Isrc
LDFLAGS ?=
HOST_CFLAGS ?= $(CFLAGS)
HOST_LDFLAGS ?= $(LDFLAGS) $(HOST_NATIVE_LDFLAGS)
TOOL_CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -Iinclude -Isrc
AMIGA_BASE_CFLAGS ?= -O2 -std=gnu89 -Wall -Wextra -Werror -Iinclude -Isrc -m68000 -DANA_TARGET_AMIGA
AMIGA_PRESENT_CFLAGS ?= -DANA_AMIGA_DIRECT_PRESENT
AMIGA_CFLAGS ?= $(AMIGA_BASE_CFLAGS) $(AMIGA_PRESENT_CFLAGS)
AMIGA_DEBUG_CFLAGS ?= $(AMIGA_CFLAGS) -DANA_DEBUG_STATS
AMIGA_INPUT_PROBE_HARNESS_CFLAGS ?= $(AMIGA_CFLAGS) -DPROBE_RUNTIME_TICKS=200
AMIGA_INPUT_PROBE_SYNTHETIC_HARNESS_CFLAGS ?= $(AMIGA_CFLAGS) -DPROBE_RUNTIME_TICKS=60 -DPROBE_SYNTHETIC_INPUT=1
BB_HARNESS_FRAMES ?= 120
BB_HARNESS_SCENARIO_ID ?= 0
AMIGA_BYTE_BROTHERS_HARNESS_CFLAGS ?= $(AMIGA_A1200_DEBUG_CFLAGS) -DBB_EMULATOR_HARNESS -DBB_HARNESS_FRAMES=$(BB_HARNESS_FRAMES) -DBB_HARNESS_SCENARIO=$(BB_HARNESS_SCENARIO_ID) -DBB_INPUT_DEBUG_OVERLAY=0
AMIGA_BUFFERED_DEBUG_CFLAGS ?= $(AMIGA_BASE_CFLAGS) -DANA_DEBUG_STATS
AMIGA_SYNC_CFLAGS ?= $(AMIGA_BASE_CFLAGS) -DANA_AMIGA_DIRECT_PRESENT -DANA_AMIGA_DIRECT_PRESENT_SYNC -DANA_DEBUG_STATS
AMIGA_A1200_BASE_CFLAGS ?= -O2 -std=gnu89 -Wall -Wextra -Werror -Iinclude -Isrc -m68020 -DANA_TARGET_AMIGA -DANA_AMIGA_A1200_BASELINE
# Direct-present is the normal Amiga presentation path. Scrolling tile layers
# can request ANA_SCROLL_BACKEND_HARDWARE; until the planar fine-scroll backend
# exists this uses the conservative tile-layer path. The transitional
# visible-bitmap bridge is opt-in via ANA_SCROLL_BACKEND_NATIVE only.
AMIGA_A1200_PRESENT_CFLAGS ?= $(AMIGA_PRESENT_CFLAGS)
AMIGA_A1200_CFLAGS ?= $(AMIGA_A1200_BASE_CFLAGS) $(AMIGA_A1200_PRESENT_CFLAGS)
AMIGA_A1200_DEBUG_CFLAGS ?= $(AMIGA_A1200_CFLAGS) -DANA_DEBUG_STATS
AMIGA_ASFLAGS ?= -quiet -Fhunk -m68000 -phxass
AMIGA_LDFLAGS ?= -mcrt=nix13 -lamiga
BUILD_DIR ?= build
AMIGA_BUILD_DIR ?= $(BUILD_DIR)/amiga
ADF_DIR ?= $(BUILD_DIR)/adf
AMIGA_DEBUG_BUILD_DIR := $(BUILD_DIR)/amiga-debug
AMIGA_HARNESS_BUILD_DIR := $(BUILD_DIR)/amiga-harness
AMIGA_BUFFERED_DEBUG_BUILD_DIR := $(BUILD_DIR)/amiga-buffered-debug
AMIGA_SYNC_BUILD_DIR := $(BUILD_DIR)/amiga-sync
AMIGA_A1200_BUILD_DIR := $(BUILD_DIR)/amiga-a1200
AMIGA_A1200_DEBUG_BUILD_DIR := $(BUILD_DIR)/amiga-a1200-debug
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
	$(BUILD_DIR)/examples/input_probe/input_probe \
	$(BUILD_DIR)/examples/invaders/invaders \
	$(BUILD_DIR)/examples/amaze/amaze \
	$(BUILD_DIR)/examples/byte_brothers/byte_brothers

INPUT_PROBE_SRCS := \
	examples/input_probe/main.c

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
INVADERS_ASSET_STAMP := $(INVADERS_ASSET_BUILD_DIR)/.stamp

AMAZE_SRCS := \
	examples/amaze/main.c \
	examples/amaze/amaze_game.c \
	examples/amaze/amaze_render.c \
	examples/amaze/amaze_assets.c
AMAZE_HEADERS := \
	examples/amaze/amaze_assets.h \
	examples/amaze/amaze_game.h \
	examples/amaze/amaze_internal.h
AMAZE_ASSET_BUILD_DIR := $(BUILD_DIR)/assets/amaze
AMAZE_ASSET_DIR := $(AMAZE_ASSET_BUILD_DIR)/assets
AMAZE_ASSET_MANIFEST := examples/amaze/assets/assets.ana
AMAZE_ASSET_SOURCES := $(wildcard examples/amaze/assets/*)
AMAZE_ASSET_STAMP := $(AMAZE_ASSET_BUILD_DIR)/.stamp

BYTE_BROTHERS_SRCS := \
	examples/byte_brothers/main.c \
	examples/byte_brothers/byte_brothers_game.c \
	examples/byte_brothers/byte_brothers_levels.c \
	examples/byte_brothers/byte_brothers_render.c \
	examples/byte_brothers/byte_brothers_assets.c
BYTE_BROTHERS_HEADERS := \
	examples/byte_brothers/byte_brothers_assets.h \
	examples/byte_brothers/byte_brothers_game.h \
	examples/byte_brothers/byte_brothers_internal.h \
	examples/byte_brothers/byte_brothers_levels.h \
	examples/byte_brothers/byte_brothers_render.h
BYTE_BROTHERS_ASSET_BUILD_DIR := $(BUILD_DIR)/assets/byte_brothers
BYTE_BROTHERS_ASSET_DIR := $(BYTE_BROTHERS_ASSET_BUILD_DIR)/assets
BYTE_BROTHERS_ASSET_MANIFEST := examples/byte_brothers/assets/assets.ana
BYTE_BROTHERS_ASSET_SOURCES := $(wildcard examples/byte_brothers/assets/*)
BYTE_BROTHERS_ASSET_STAMP := $(BYTE_BROTHERS_ASSET_BUILD_DIR)/.stamp

TOOL_BINS := $(BUILD_DIR)/tools/ana-convert/ana-convert

AMIGA_ASM_OBJS := $(ANA_AMIGA_ASM_SRCS:%.asm=$(AMIGA_BUILD_DIR)/%.o)
AMIGA_OBJS := $(ANA_SRCS:%.c=$(AMIGA_BUILD_DIR)/%.o) $(AMIGA_ASM_OBJS)
AMIGA_LIBANA := $(AMIGA_BUILD_DIR)/libana.a
AMIGA_DEBUG_ASM_OBJS := $(ANA_AMIGA_ASM_SRCS:%.asm=$(AMIGA_DEBUG_BUILD_DIR)/%.o)
AMIGA_DEBUG_OBJS := $(ANA_SRCS:%.c=$(AMIGA_DEBUG_BUILD_DIR)/%.o) $(AMIGA_DEBUG_ASM_OBJS)
AMIGA_DEBUG_LIBANA := $(AMIGA_DEBUG_BUILD_DIR)/libana.a
AMIGA_BUFFERED_DEBUG_ASM_OBJS := $(ANA_AMIGA_ASM_SRCS:%.asm=$(AMIGA_BUFFERED_DEBUG_BUILD_DIR)/%.o)
AMIGA_BUFFERED_DEBUG_OBJS := $(ANA_SRCS:%.c=$(AMIGA_BUFFERED_DEBUG_BUILD_DIR)/%.o) $(AMIGA_BUFFERED_DEBUG_ASM_OBJS)
AMIGA_BUFFERED_DEBUG_LIBANA := $(AMIGA_BUFFERED_DEBUG_BUILD_DIR)/libana.a
AMIGA_SYNC_ASM_OBJS := $(ANA_AMIGA_ASM_SRCS:%.asm=$(AMIGA_SYNC_BUILD_DIR)/%.o)
AMIGA_SYNC_OBJS := $(ANA_SRCS:%.c=$(AMIGA_SYNC_BUILD_DIR)/%.o) $(AMIGA_SYNC_ASM_OBJS)
AMIGA_SYNC_LIBANA := $(AMIGA_SYNC_BUILD_DIR)/libana.a
AMIGA_A1200_ASM_OBJS := $(ANA_AMIGA_ASM_SRCS:%.asm=$(AMIGA_A1200_BUILD_DIR)/%.o)
AMIGA_A1200_OBJS := $(ANA_SRCS:%.c=$(AMIGA_A1200_BUILD_DIR)/%.o) $(AMIGA_A1200_ASM_OBJS)
AMIGA_A1200_LIBANA := $(AMIGA_A1200_BUILD_DIR)/libana.a
AMIGA_A1200_DEBUG_ASM_OBJS := $(ANA_AMIGA_ASM_SRCS:%.asm=$(AMIGA_A1200_DEBUG_BUILD_DIR)/%.o)
AMIGA_A1200_DEBUG_OBJS := $(ANA_SRCS:%.c=$(AMIGA_A1200_DEBUG_BUILD_DIR)/%.o) $(AMIGA_A1200_DEBUG_ASM_OBJS)
AMIGA_A1200_DEBUG_LIBANA := $(AMIGA_A1200_DEBUG_BUILD_DIR)/libana.a

AMIGA_EXAMPLE_BINS := \
	$(AMIGA_BUILD_DIR)/examples/hello/hello \
	$(AMIGA_BUILD_DIR)/examples/input_probe/input_probe \
	$(AMIGA_BUILD_DIR)/examples/invaders/invaders \
	$(AMIGA_BUILD_DIR)/examples/amaze/amaze \
	$(AMIGA_BUILD_DIR)/examples/byte_brothers/byte_brothers
AMIGA_A1200_EXAMPLE_BINS := \
	$(AMIGA_A1200_BUILD_DIR)/examples/hello/hello \
	$(AMIGA_A1200_BUILD_DIR)/examples/input_probe/input_probe \
	$(AMIGA_A1200_BUILD_DIR)/examples/invaders/invaders \
	$(AMIGA_A1200_BUILD_DIR)/examples/amaze/amaze \
	$(AMIGA_A1200_BUILD_DIR)/examples/byte_brothers/byte_brothers

AMIGA_INPUT_PROBE_A1200_DEBUG_BIN := $(AMIGA_A1200_DEBUG_BUILD_DIR)/examples/input-probe-a1200-debug/input_probe
AMIGA_INPUT_PROBE_HARNESS_BIN := $(AMIGA_HARNESS_BUILD_DIR)/examples/input_probe/input_probe
AMIGA_INPUT_PROBE_SYNTHETIC_HARNESS_BIN := $(AMIGA_HARNESS_BUILD_DIR)/examples/input_probe_synthetic/input_probe
AMIGA_BYTE_BROTHERS_HARNESS_BIN := $(AMIGA_HARNESS_BUILD_DIR)/examples/byte_brothers/byte_brothers
AMIGA_INVADERS_DEBUG_BIN := $(AMIGA_DEBUG_BUILD_DIR)/examples/invaders-debug/invaders
AMIGA_INVADERS_BUFFERED_DEBUG_BIN := $(AMIGA_BUFFERED_DEBUG_BUILD_DIR)/examples/invaders-buffered-debug/invaders
AMIGA_INVADERS_SYNC_BIN := $(AMIGA_SYNC_BUILD_DIR)/examples/invaders-sync/invaders
AMIGA_INVADERS_A1200_DEBUG_BIN := $(AMIGA_A1200_DEBUG_BUILD_DIR)/examples/invaders-a1200-debug/invaders
AMIGA_AMAZE_A1200_DEBUG_BIN := $(AMIGA_A1200_DEBUG_BUILD_DIR)/examples/amaze-a1200-debug/amaze
AMIGA_BYTE_BROTHERS_A1200_DEBUG_BIN := $(AMIGA_A1200_DEBUG_BUILD_DIR)/examples/byte-brothers-a1200-debug/byte_brothers

ADF_FILES := \
	$(ADF_DIR)/hello.adf \
	$(ADF_DIR)/invaders.adf \
	$(ADF_DIR)/amaze.adf \
	$(ADF_DIR)/byte-brothers.adf

INVADERS_DEBUG_ADF := $(ADF_DIR)/invaders-debug.adf
INVADERS_BUFFERED_DEBUG_ADF := $(ADF_DIR)/invaders-buffered-debug.adf
INVADERS_SYNC_ADF := $(ADF_DIR)/invaders-sync.adf
INVADERS_A1200_ADF := $(ADF_DIR)/invaders-a1200.adf
INVADERS_A1200_DEBUG_ADF := $(ADF_DIR)/invaders-a1200-debug.adf
INPUT_PROBE_A1200_DEBUG_ADF := $(ADF_DIR)/input-probe-a1200-debug.adf
INPUT_PROBE_HARNESS_ADF := $(ADF_DIR)/input-probe-harness.adf
INPUT_PROBE_SYNTHETIC_HARNESS_ADF := $(ADF_DIR)/input-probe-synthetic-harness.adf
AMAZE_A1200_ADF := $(ADF_DIR)/amaze-a1200.adf
AMAZE_A1200_DEBUG_ADF := $(ADF_DIR)/amaze-a1200-debug.adf
BYTE_BROTHERS_A1200_ADF := $(ADF_DIR)/byte-brothers-a1200.adf
BYTE_BROTHERS_A1200_DEBUG_ADF := $(ADF_DIR)/byte-brothers-a1200-debug.adf
BYTE_BROTHERS_A1200_DEBUG_FS_UAE_CONFIG := $(BUILD_DIR)/fs-uae/byte-brothers-a1200-debug.fs-uae

.PHONY: all lib examples assets examples/invaders-assets invaders-assets examples/amaze-assets amaze-assets examples/byte-brothers-assets byte-brothers-assets tools test amiga-lib amiga-examples amiga-a1200-lib amiga-a1200-examples amiga-input-probe-a1200-debug amiga-input-probe-harness amiga-input-probe-synthetic-harness amiga-byte-brothers-harness amiga-invaders-debug amiga-invaders-buffered-debug amiga-invaders-sync amiga-invaders-a1200-debug amiga-amaze-a1200-debug amiga-byte-brothers-a1200-debug adfs input-probe-a1200-debug-adf input-probe-harness-adf input-probe-synthetic-harness-adf emulator-input-probe emulator-input-probe-synthetic emulator-byte-brothers emulator-byte-brothers-scroll emulator-byte-brothers-input emulator-byte-brothers-all invaders-debug-adf invaders-buffered-debug-adf invaders-sync-adf invaders-a1200-adf invaders-a1200-debug-adf amaze-a1200-adf amaze-a1200-debug-adf byte-brothers-a1200-adf byte-brothers-a1200-debug-adf byte-brothers-a1200-debug-fsuae-config run-byte-brothers-a1200-debug release-package clean-assets clean

all: lib examples tools

lib: $(LIBANA)

examples: $(EXAMPLE_BINS)

assets: invaders-assets amaze-assets byte-brothers-assets

examples/invaders-assets: $(INVADERS_ASSET_STAMP)

invaders-assets: $(INVADERS_ASSET_STAMP)

examples/amaze-assets: $(AMAZE_ASSET_STAMP)

amaze-assets: $(AMAZE_ASSET_STAMP)

examples/byte-brothers-assets: $(BYTE_BROTHERS_ASSET_STAMP)

byte-brothers-assets: $(BYTE_BROTHERS_ASSET_STAMP)

tools: $(TOOL_BINS)

amiga-lib: $(AMIGA_LIBANA)

amiga-examples: $(AMIGA_EXAMPLE_BINS)

amiga-a1200-lib: $(AMIGA_A1200_LIBANA)

amiga-a1200-examples: $(AMIGA_A1200_EXAMPLE_BINS)

amiga-input-probe-a1200-debug: $(AMIGA_INPUT_PROBE_A1200_DEBUG_BIN)

amiga-input-probe-harness: $(AMIGA_INPUT_PROBE_HARNESS_BIN)

amiga-input-probe-synthetic-harness: $(AMIGA_INPUT_PROBE_SYNTHETIC_HARNESS_BIN)

amiga-byte-brothers-harness: $(AMIGA_BYTE_BROTHERS_HARNESS_BIN)

amiga-invaders-debug: $(AMIGA_INVADERS_DEBUG_BIN)

amiga-invaders-buffered-debug: $(AMIGA_INVADERS_BUFFERED_DEBUG_BIN)

amiga-invaders-sync: $(AMIGA_INVADERS_SYNC_BIN)

amiga-invaders-a1200-debug: $(AMIGA_INVADERS_A1200_DEBUG_BIN)

amiga-amaze-a1200-debug: $(AMIGA_AMAZE_A1200_DEBUG_BIN)

amiga-byte-brothers-a1200-debug: $(AMIGA_BYTE_BROTHERS_A1200_DEBUG_BIN)

adfs: $(ADF_FILES)

invaders-debug-adf: $(INVADERS_DEBUG_ADF)

invaders-buffered-debug-adf: $(INVADERS_BUFFERED_DEBUG_ADF)

invaders-sync-adf: $(INVADERS_SYNC_ADF)

invaders-a1200-adf: $(INVADERS_A1200_ADF)

invaders-a1200-debug-adf: $(INVADERS_A1200_DEBUG_ADF)

input-probe-a1200-debug-adf: $(INPUT_PROBE_A1200_DEBUG_ADF)

input-probe-harness-adf: $(INPUT_PROBE_HARNESS_ADF)

input-probe-synthetic-harness-adf: $(INPUT_PROBE_SYNTHETIC_HARNESS_ADF)

emulator-input-probe:
	python3 tools/emulator/run_input_probe.py --no-send-keys

emulator-input-probe-synthetic:
	python3 tools/emulator/run_input_probe.py --synthetic --no-send-keys

emulator-byte-brothers:
	python3 tools/emulator/run_byte_brothers.py --scenario static

emulator-byte-brothers-scroll:
	python3 tools/emulator/run_byte_brothers.py --scenario scroll

emulator-byte-brothers-input:
	python3 tools/emulator/run_byte_brothers.py --scenario input

emulator-byte-brothers-all:
	python3 tools/emulator/run_byte_brothers.py --scenario static
	python3 tools/emulator/run_byte_brothers.py --scenario scroll
	python3 tools/emulator/run_byte_brothers.py --scenario input

amaze-a1200-adf: $(AMAZE_A1200_ADF)

amaze-a1200-debug-adf: $(AMAZE_A1200_DEBUG_ADF)

byte-brothers-a1200-adf: $(BYTE_BROTHERS_A1200_ADF)

byte-brothers-a1200-debug-adf: $(BYTE_BROTHERS_A1200_DEBUG_ADF)

byte-brothers-a1200-debug-fsuae-config: $(BYTE_BROTHERS_A1200_DEBUG_FS_UAE_CONFIG)

run-byte-brothers-a1200-debug: $(BYTE_BROTHERS_A1200_DEBUG_FS_UAE_CONFIG)
	$(FS_UAE) $(BYTE_BROTHERS_A1200_DEBUG_FS_UAE_CONFIG)

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

$(BUILD_DIR)/examples/input_probe/input_probe: $(INPUT_PROBE_SRCS) $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INPUT_PROBE_SRCS) $(LIBANA) $(LDFLAGS) -o $@

$(INVADERS_ASSET_STAMP): $(TOOL_BINS) $(INVADERS_ASSET_SOURCES)
	$(RM) $(INVADERS_ASSET_DIR)
	mkdir -p $(INVADERS_ASSET_DIR)
	$(BUILD_DIR)/tools/ana-convert/ana-convert build $(INVADERS_ASSET_MANIFEST) --out $(INVADERS_ASSET_DIR)
	touch $@

$(BUILD_DIR)/examples/invaders/invaders: $(INVADERS_SRCS) $(INVADERS_HEADERS) $(LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INVADERS_SRCS) $(LIBANA) $(LDFLAGS) -o $@

$(AMAZE_ASSET_STAMP): $(TOOL_BINS) $(AMAZE_ASSET_SOURCES)
	$(RM) $(AMAZE_ASSET_DIR)
	mkdir -p $(AMAZE_ASSET_DIR)
	$(BUILD_DIR)/tools/ana-convert/ana-convert build $(AMAZE_ASSET_MANIFEST) --out $(AMAZE_ASSET_DIR)
	touch $@

$(BUILD_DIR)/examples/amaze/amaze: $(AMAZE_SRCS) $(AMAZE_HEADERS) $(LIBANA) $(AMAZE_ASSET_STAMP)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(AMAZE_SRCS) $(LIBANA) $(LDFLAGS) -o $@

$(BYTE_BROTHERS_ASSET_STAMP): $(TOOL_BINS) $(BYTE_BROTHERS_ASSET_SOURCES)
	$(RM) $(BYTE_BROTHERS_ASSET_DIR)
	mkdir -p $(BYTE_BROTHERS_ASSET_DIR)
	$(BUILD_DIR)/tools/ana-convert/ana-convert build $(BYTE_BROTHERS_ASSET_MANIFEST) --out $(BYTE_BROTHERS_ASSET_DIR)
	touch $@

$(BUILD_DIR)/examples/byte_brothers/byte_brothers: $(BYTE_BROTHERS_SRCS) $(BYTE_BROTHERS_HEADERS) $(LIBANA) $(BYTE_BROTHERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(BYTE_BROTHERS_SRCS) $(LIBANA) $(LDFLAGS) -o $@

$(BUILD_DIR)/tools/ana-convert/ana-convert: tools/ana-convert/main.c tools/ana-convert/vendor/stb_image.h include/ana/ana_version.h
	mkdir -p $(@D)
	$(HOST_CC) $(TOOL_CFLAGS) $< $(HOST_LDFLAGS) -o $@

$(AMIGA_LIBANA): $(AMIGA_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_SYNC_LIBANA): $(AMIGA_SYNC_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_DEBUG_LIBANA): $(AMIGA_DEBUG_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_BUFFERED_DEBUG_LIBANA): $(AMIGA_BUFFERED_DEBUG_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_A1200_LIBANA): $(AMIGA_A1200_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_A1200_DEBUG_LIBANA): $(AMIGA_A1200_DEBUG_OBJS)
	mkdir -p $(@D)
	$(AMIGA_AR) rcs $@ $^

$(AMIGA_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) -c $< -o $@

$(AMIGA_DEBUG_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_DEBUG_CFLAGS) -c $< -o $@

$(AMIGA_BUFFERED_DEBUG_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_BUFFERED_DEBUG_CFLAGS) -c $< -o $@

$(AMIGA_SYNC_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_SYNC_CFLAGS) -c $< -o $@

$(AMIGA_A1200_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_CFLAGS) -c $< -o $@

$(AMIGA_A1200_DEBUG_BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_DEBUG_CFLAGS) -c $< -o $@

$(AMIGA_BUILD_DIR)/src/sound/vendor/ptplayer/ptplayer.o: src/sound/vendor/ptplayer/ptplayer.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -DOSCOMPAT=1 -o $@ $<

$(AMIGA_SYNC_BUILD_DIR)/src/sound/vendor/ptplayer/ptplayer.o: src/sound/vendor/ptplayer/ptplayer.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -DOSCOMPAT=1 -o $@ $<

$(AMIGA_DEBUG_BUILD_DIR)/src/sound/vendor/ptplayer/ptplayer.o: src/sound/vendor/ptplayer/ptplayer.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -DOSCOMPAT=1 -o $@ $<

$(AMIGA_BUFFERED_DEBUG_BUILD_DIR)/src/sound/vendor/ptplayer/ptplayer.o: src/sound/vendor/ptplayer/ptplayer.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -DOSCOMPAT=1 -o $@ $<

$(AMIGA_A1200_BUILD_DIR)/src/sound/vendor/ptplayer/ptplayer.o: src/sound/vendor/ptplayer/ptplayer.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -DOSCOMPAT=1 -o $@ $<

$(AMIGA_A1200_DEBUG_BUILD_DIR)/src/sound/vendor/ptplayer/ptplayer.o: src/sound/vendor/ptplayer/ptplayer.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -DOSCOMPAT=1 -o $@ $<

$(AMIGA_BUILD_DIR)/src/sound/ana_ptplayer_wrap.o: src/sound/ana_ptplayer_wrap.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -o $@ $<

$(AMIGA_SYNC_BUILD_DIR)/src/sound/ana_ptplayer_wrap.o: src/sound/ana_ptplayer_wrap.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -o $@ $<

$(AMIGA_DEBUG_BUILD_DIR)/src/sound/ana_ptplayer_wrap.o: src/sound/ana_ptplayer_wrap.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -o $@ $<

$(AMIGA_BUFFERED_DEBUG_BUILD_DIR)/src/sound/ana_ptplayer_wrap.o: src/sound/ana_ptplayer_wrap.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -o $@ $<

$(AMIGA_A1200_BUILD_DIR)/src/sound/ana_ptplayer_wrap.o: src/sound/ana_ptplayer_wrap.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -o $@ $<

$(AMIGA_A1200_DEBUG_BUILD_DIR)/src/sound/ana_ptplayer_wrap.o: src/sound/ana_ptplayer_wrap.asm
	mkdir -p $(@D)
	$(AMIGA_AS) $(AMIGA_ASFLAGS) -o $@ $<

$(AMIGA_BUILD_DIR)/examples/hello/hello: examples/hello/main.c $(AMIGA_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $< $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_BUILD_DIR)/examples/input_probe/input_probe: $(INPUT_PROBE_SRCS) $(AMIGA_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $(INPUT_PROBE_SRCS) $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INPUT_PROBE_HARNESS_BIN): $(INPUT_PROBE_SRCS) $(AMIGA_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_INPUT_PROBE_HARNESS_CFLAGS) $(INPUT_PROBE_SRCS) $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INPUT_PROBE_SYNTHETIC_HARNESS_BIN): $(INPUT_PROBE_SRCS) $(AMIGA_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_INPUT_PROBE_SYNTHETIC_HARNESS_CFLAGS) $(INPUT_PROBE_SRCS) $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_BUILD_DIR)/examples/invaders/invaders: $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $(INVADERS_SRCS) $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_BUILD_DIR)/examples/amaze/amaze: $(AMAZE_SRCS) $(AMAZE_HEADERS) $(AMIGA_LIBANA) $(AMAZE_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $(AMAZE_SRCS) $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_BUILD_DIR)/examples/byte_brothers/byte_brothers: $(BYTE_BROTHERS_SRCS) $(BYTE_BROTHERS_HEADERS) $(AMIGA_LIBANA) $(BYTE_BROTHERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_CFLAGS) $(BYTE_BROTHERS_SRCS) $(AMIGA_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_A1200_BUILD_DIR)/examples/hello/hello: examples/hello/main.c $(AMIGA_A1200_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_CFLAGS) $< $(AMIGA_A1200_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_A1200_BUILD_DIR)/examples/input_probe/input_probe: $(INPUT_PROBE_SRCS) $(AMIGA_A1200_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_CFLAGS) $(INPUT_PROBE_SRCS) $(AMIGA_A1200_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_A1200_BUILD_DIR)/examples/invaders/invaders: $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_A1200_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_CFLAGS) $(INVADERS_SRCS) $(AMIGA_A1200_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_A1200_BUILD_DIR)/examples/amaze/amaze: $(AMAZE_SRCS) $(AMAZE_HEADERS) $(AMIGA_A1200_LIBANA) $(AMAZE_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_CFLAGS) $(AMAZE_SRCS) $(AMIGA_A1200_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_A1200_BUILD_DIR)/examples/byte_brothers/byte_brothers: $(BYTE_BROTHERS_SRCS) $(BYTE_BROTHERS_HEADERS) $(AMIGA_A1200_LIBANA) $(BYTE_BROTHERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_CFLAGS) $(BYTE_BROTHERS_SRCS) $(AMIGA_A1200_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INVADERS_DEBUG_BIN): $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_DEBUG_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_DEBUG_CFLAGS) $(INVADERS_SRCS) $(AMIGA_DEBUG_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INVADERS_BUFFERED_DEBUG_BIN): $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_BUFFERED_DEBUG_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_BUFFERED_DEBUG_CFLAGS) $(INVADERS_SRCS) $(AMIGA_BUFFERED_DEBUG_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INVADERS_SYNC_BIN): $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_SYNC_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_SYNC_CFLAGS) $(INVADERS_SRCS) $(AMIGA_SYNC_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INVADERS_A1200_DEBUG_BIN): $(INVADERS_SRCS) $(INVADERS_HEADERS) $(AMIGA_A1200_DEBUG_LIBANA) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_DEBUG_CFLAGS) $(INVADERS_SRCS) $(AMIGA_A1200_DEBUG_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_INPUT_PROBE_A1200_DEBUG_BIN): $(INPUT_PROBE_SRCS) $(AMIGA_A1200_DEBUG_LIBANA)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_DEBUG_CFLAGS) $(INPUT_PROBE_SRCS) $(AMIGA_A1200_DEBUG_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_AMAZE_A1200_DEBUG_BIN): $(AMAZE_SRCS) $(AMAZE_HEADERS) $(AMIGA_A1200_DEBUG_LIBANA) $(AMAZE_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_DEBUG_CFLAGS) $(AMAZE_SRCS) $(AMIGA_A1200_DEBUG_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_BYTE_BROTHERS_A1200_DEBUG_BIN): $(BYTE_BROTHERS_SRCS) $(BYTE_BROTHERS_HEADERS) $(AMIGA_A1200_DEBUG_LIBANA) $(BYTE_BROTHERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_DEBUG_CFLAGS) $(BYTE_BROTHERS_SRCS) $(AMIGA_A1200_DEBUG_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(AMIGA_BYTE_BROTHERS_HARNESS_BIN): $(BYTE_BROTHERS_SRCS) $(BYTE_BROTHERS_HEADERS) $(AMIGA_A1200_DEBUG_LIBANA) $(BYTE_BROTHERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_BYTE_BROTHERS_HARNESS_CFLAGS) $(BYTE_BROTHERS_SRCS) $(AMIGA_A1200_DEBUG_LIBANA) $(AMIGA_LDFLAGS) -o $@

$(ADF_DIR)/hello.adf: $(AMIGA_BUILD_DIR)/examples/hello/hello
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAHello

$(ADF_DIR)/input-probe.adf: $(AMIGA_BUILD_DIR)/examples/input_probe/input_probe
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInput

$(INPUT_PROBE_HARNESS_ADF): $(AMIGA_INPUT_PROBE_HARNESS_BIN)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAProbe

$(INPUT_PROBE_SYNTHETIC_HARNESS_ADF): $(AMIGA_INPUT_PROBE_SYNTHETIC_HARNESS_BIN)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANASynth

$(ADF_DIR)/invaders.adf: $(AMIGA_BUILD_DIR)/examples/invaders/invaders $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvaders $(INVADERS_ASSET_DIR)

$(ADF_DIR)/amaze.adf: $(AMIGA_BUILD_DIR)/examples/amaze/amaze $(AMAZE_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAAMAze $(AMAZE_ASSET_DIR)

$(ADF_DIR)/byte-brothers.adf: $(AMIGA_BUILD_DIR)/examples/byte_brothers/byte_brothers $(BYTE_BROTHERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAByte $(BYTE_BROTHERS_ASSET_DIR)

$(INVADERS_A1200_ADF): $(AMIGA_A1200_BUILD_DIR)/examples/invaders/invaders $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInv1200 $(INVADERS_ASSET_DIR)

$(AMAZE_A1200_ADF): $(AMIGA_A1200_BUILD_DIR)/examples/amaze/amaze $(AMAZE_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l AMAZE1200 $(AMAZE_ASSET_DIR)

$(BYTE_BROTHERS_A1200_ADF): $(AMIGA_A1200_BUILD_DIR)/examples/byte_brothers/byte_brothers $(BYTE_BROTHERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l BYTE1200 $(BYTE_BROTHERS_ASSET_DIR)

$(INVADERS_DEBUG_ADF): $(AMIGA_INVADERS_DEBUG_BIN) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvDbg $(INVADERS_ASSET_DIR)

$(INVADERS_BUFFERED_DEBUG_ADF): $(AMIGA_INVADERS_BUFFERED_DEBUG_BIN) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvBuf $(INVADERS_ASSET_DIR)

$(INVADERS_SYNC_ADF): $(AMIGA_INVADERS_SYNC_BIN) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInvSync $(INVADERS_ASSET_DIR)

$(INVADERS_A1200_DEBUG_ADF): $(AMIGA_INVADERS_A1200_DEBUG_BIN) $(INVADERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInv12Dbg $(INVADERS_ASSET_DIR)

$(INPUT_PROBE_A1200_DEBUG_ADF): $(AMIGA_INPUT_PROBE_A1200_DEBUG_BIN)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l ANAInputDbg

$(AMAZE_A1200_DEBUG_ADF): $(AMIGA_AMAZE_A1200_DEBUG_BIN) $(AMAZE_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l AMAZE12Dbg $(AMAZE_ASSET_DIR)

$(BYTE_BROTHERS_A1200_DEBUG_ADF): $(AMIGA_BYTE_BROTHERS_A1200_DEBUG_BIN) $(BYTE_BROTHERS_ASSET_STAMP)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l BYTE12Dbg $(BYTE_BROTHERS_ASSET_DIR)

$(BYTE_BROTHERS_A1200_DEBUG_FS_UAE_CONFIG): $(BYTE_BROTHERS_A1200_DEBUG_ADF) tools/emulator/write_fsuae_launch_config.py
	python3 tools/emulator/write_fsuae_launch_config.py --profile a1200 --adf $(BYTE_BROTHERS_A1200_DEBUG_ADF) --output $@

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
	$(RM) $(BUILD_DIR)/assets $(BUILD_DIR)/examples/invaders-build-assets $(TOOL_BINS)
