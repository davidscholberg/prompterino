BUILD_DIR = build
HEADERS =
SRCS = main.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
CFLAGS = -Wall -Wextra -Werror

$(BUILD_DIR)/%.o: %.c $(HEADERS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/prompt-c: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: all clean pedantic sanitize

all: $(BUILD_DIR)/prompt-c

clean:
	rm -rf $(BUILD_DIR)

pedantic:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) -std=c89 -Wpedantic"
	$(MAKE) clean

sanitize:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) -Wno-error -fsanitize=address,leak,undefined"
