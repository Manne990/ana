ANA_SDK ?= .ana-sdk
ANA_SDK_ABS := $(abspath $(ANA_SDK))
PROJECT_ABS := $(abspath .)
CC ?= cc
HOST_CFLAGS ?= -std=c89 -Wall -Wextra -I$(ANA_SDK_ABS)/include -I$(ANA_SDK_ABS)/src
HOST_LIB ?= $(ANA_SDK_ABS)/build/libana.a
AMIGA_DOCKER_IMAGE ?= amigadev/crosstools:m68k-amigaos-gcc10_amd64
AMIGA_NATIVE_CC := $(shell command -v m68k-amigaos-gcc 2>/dev/null)
AMIGA_DOCKER_RUN ?= docker run --rm --platform linux/amd64 --user $(shell id -u):$(shell id -g) -v "$(PROJECT_ABS):/project" -v "$(ANA_SDK_ABS):/ana" -w /project $(AMIGA_DOCKER_IMAGE)
AMIGA_SDK_DOCKER_RUN ?= docker run --rm --platform linux/amd64 --user $(shell id -u):$(shell id -g) -v "$(ANA_SDK_ABS):/ana" -w /ana $(AMIGA_DOCKER_IMAGE)
AMIGA_CC ?= $(if $(AMIGA_NATIVE_CC),$(AMIGA_NATIVE_CC),$(AMIGA_DOCKER_RUN) m68k-amigaos-gcc)
AMIGA_SDK_MAKE ?= $(if $(AMIGA_NATIVE_CC),$(MAKE) -C "$(ANA_SDK_ABS)",$(AMIGA_SDK_DOCKER_RUN) make)
AMIGA_INCLUDE ?= $(if $(AMIGA_NATIVE_CC),-I$(ANA_SDK_ABS)/include -I$(ANA_SDK_ABS)/src,-I/ana/include -I/ana/src)
AMIGA_A1200_LIB ?= $(if $(AMIGA_NATIVE_CC),$(ANA_SDK_ABS)/build/amiga-a1200/libana.a,/ana/build/amiga-a1200/libana.a)
AMIGA_A1200_LIB_HOST := $(ANA_SDK_ABS)/build/amiga-a1200/libana.a
AMIGA_A1200_CFLAGS ?= -O2 -std=gnu89 -Wall -Wextra -Werror $(AMIGA_INCLUDE) -m68020 -DANA_TARGET_AMIGA -DANA_AMIGA_DIRECT_PRESENT -DANA_AMIGA_A1200_BASELINE
AMIGA_LDFLAGS ?= -mcrt=nix13 -lamiga
ADFTOOL ?= $(shell command -v gadf 2>/dev/null || command -v $(HOME)/go/bin/gadf 2>/dev/null || echo gadf)
BUILD_DIR ?= build
EXE_NAME := {{executableName}}
HOST_TARGET := $(BUILD_DIR)/$(EXE_NAME)
AMIGA_A1200_TARGET := $(BUILD_DIR)/amiga-a1200/$(EXE_NAME)
ADF_FILE := $(BUILD_DIR)/adf/$(EXE_NAME)-a1200.adf
ADF_LABEL ?= ANAGame
SRCS := src/main.c src/game.c

.PHONY: all host amiga-examples amiga-a1200-examples amiga-a1200 adfs a1200-adf invaders-a1200-adf amaze-a1200-adf byte-brothers-a1200-adf clean

all: host

host: $(HOST_TARGET)

amiga-examples: amiga-a1200

amiga-a1200-examples: amiga-a1200

amiga-a1200: $(AMIGA_A1200_TARGET)

adfs: a1200-adf

a1200-adf: $(ADF_FILE)

invaders-a1200-adf amaze-a1200-adf byte-brothers-a1200-adf: a1200-adf

$(HOST_LIB):
	$(MAKE) -C "$(ANA_SDK_ABS)" lib

$(AMIGA_A1200_LIB_HOST):
	$(AMIGA_SDK_MAKE) amiga-a1200-lib

$(HOST_TARGET): $(SRCS) $(HOST_LIB)
	mkdir -p $(BUILD_DIR)
	$(CC) $(HOST_CFLAGS) $(SRCS) $(HOST_LIB) -o $@

$(AMIGA_A1200_TARGET): $(SRCS) $(AMIGA_A1200_LIB_HOST)
	mkdir -p $(@D)
	$(AMIGA_CC) $(AMIGA_A1200_CFLAGS) $(SRCS) $(AMIGA_A1200_LIB) $(AMIGA_LDFLAGS) -o $@

$(ADF_FILE): $(AMIGA_A1200_TARGET)
	mkdir -p $(@D)
	$(ADFTOOL) -i $< -a $@ -l $(ADF_LABEL)

clean:
	rm -rf $(BUILD_DIR)
