CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Isrc

BUILD_DIR := build
TARGET := $(BUILD_DIR)/caz
SOURCES := \
	src/main.c \
	src/caz_cpu.c \
	src/caz_droid.c \
	src/caz_programs.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: src/%.c src/caz_cpu.h src/caz_droid.h src/caz_programs.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	$(TARGET) --program farmyard-mouser --scenario farmyard --steps 48

clean:
	rm -rf $(BUILD_DIR)
