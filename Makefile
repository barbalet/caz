CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Isrc

BUILD_DIR := build
TARGET := $(BUILD_DIR)/caz
CAZENV_SURVIVAL := $(BUILD_DIR)/cazenv-survival
SOURCES := \
	src/main.c \
	src/caz_cpu.c \
	src/caz_body.c \
	src/caz_droid.c \
	src/caz_opencat.c \
	src/caz_loader.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)
CAZENV_SURVIVAL_SOURCES := \
	cazenv/Tools/RunSurvivalTest.c \
	cazenv/c_core/caz_env.c \
	src/caz_cpu.c \
	src/caz_body.c \
	src/caz_droid.c \
	src/caz_loader.c

.PHONY: all run cazenv-probes cazenv-survival cazenv-survival-compat cazenv-survival-strict cazenv-survival-long clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: src/%.c src/caz_cpu.h src/caz_body.h src/caz_droid.h src/caz_opencat.h src/caz_loader.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	$(TARGET) --program farmyard-mouser --scenario farmyard --steps 48

cazenv-probes: $(CAZENV_SURVIVAL)
	$(CAZENV_SURVIVAL) --mode probes

cazenv-survival: cazenv-survival-compat

cazenv-survival-compat: $(CAZENV_SURVIVAL)
	$(CAZENV_SURVIVAL) --mode short-compat 14 3

cazenv-survival-strict: $(CAZENV_SURVIVAL)
	$(CAZENV_SURVIVAL) --mode short-strict 14 3

cazenv-survival-long: $(CAZENV_SURVIVAL)
	$(CAZENV_SURVIVAL) --mode long-strict 30 5

$(CAZENV_SURVIVAL): $(CAZENV_SURVIVAL_SOURCES) cazenv/c_core/caz_env.h
	mkdir -p $(BUILD_DIR)
	$(CC) -Icazenv/c_core $(CFLAGS) $(CAZENV_SURVIVAL_SOURCES) -o $@

clean:
	rm -rf $(BUILD_DIR)
