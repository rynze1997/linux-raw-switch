CC = gcc
# -Wall: enable all common warnings
# -Wextra: enable additional warnings
# -O2: optimization level 2 (faster code)
CFLAGS = -Wall -Wextra -O2
BUILD_DIR = build
TARGET = $(BUILD_DIR)/hub

all: $(TARGET)

$(TARGET): hub.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) hub.c

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
