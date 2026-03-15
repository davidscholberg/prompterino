BUILD_DIR = build
HEADERS =
SRCS = main.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
CFLAGS = -Wall -Wextra -Wpedantic -Werror -std=c89

$(BUILD_DIR)/%.o: %.c $(HEADERS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/prompt-c: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: all clean

all: $(BUILD_DIR)/prompt-c

clean:
	rm -rf $(BUILD_DIR)
