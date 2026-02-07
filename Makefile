CC = gcc
# -Wall: enable all common warnings
# -Wextra: enable additional warnings
# -O2: optimization level 2 (faster code)
CFLAGS = -Wall -Wextra -O2
BUILD_DIR = build
SRC_DIR = src
TARGET = $(BUILD_DIR)/sw_switch

SRCS = $(SRC_DIR)/switch.c $(SRC_DIR)/net/socket.c $(SRC_DIR)/switch/mac_table.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
