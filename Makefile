MAKEFLAGS += --no-print-directory
CFLAGS = -Wall -Wextra -Werror
BUILD_DIR = build
EXE = $(BUILD_DIR)/prompterino
HEADERS = config.h
SRCS = main.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

.DEFAULT_GOAL := all

$(EXE): $(SRCS) $(HEADERS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(EXE) $(SRCS)

.PHONY: all build clean debug install pedantic release sanitize

build: $(EXE)

debug: CFLAGS += -g -Og
debug: build

all: debug

clean:
	rm -rf $(BUILD_DIR)

pedantic:
	@$(MAKE) clean
	@$(MAKE) build CFLAGS="$(CFLAGS) -std=c89 -Wpedantic"
	@$(MAKE) clean

release:
	@$(MAKE) clean
	@$(MAKE) build CFLAGS="$(CFLAGS) -O3"

sanitize:
	@$(MAKE) clean
	@$(MAKE) build CFLAGS="$(CFLAGS) -Wno-error -fsanitize=address,leak,undefined -g -Og"

install: release
	install -C -m 0755 "$(EXE)" "$(HOME)/.local/bin/"

install-sanitize: sanitize
	install -C -m 0755 "$(EXE)" "$(HOME)/.local/bin/"
