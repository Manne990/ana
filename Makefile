CC ?= cc
AR ?= ar
RM = rm -rf

CFLAGS ?= -std=c89 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS ?=
BUILD_DIR ?= build

ANA_SRCS := src/ana_platform.c src/ana_result.c
ANA_OBJS := $(ANA_SRCS:%.c=$(BUILD_DIR)/%.o)
LIBANA := $(BUILD_DIR)/libana.a

TEST_BINS := $(BUILD_DIR)/tests/platform_profile_test

.PHONY: all lib test clean

all: lib

lib: $(LIBANA)

$(LIBANA): $(ANA_OBJS)
	mkdir -p $(@D)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/platform_profile_test: tests/platform_profile_test.c $(LIBANA)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< $(LIBANA) $(LDFLAGS) -o $@

test: $(TEST_BINS)
	$(TEST_BINS)

clean:
	$(RM) $(BUILD_DIR)
