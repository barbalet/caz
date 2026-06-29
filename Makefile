CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Isrc

BUILD_DIR := build
TARGET := $(BUILD_DIR)/caz
CAZENV_SURVIVAL := $(BUILD_DIR)/cazenv-survival
CAZENV_CATALYST_PROJECT := cazenv/CazEnvCatalyst.xcodeproj
CAZENV_RELEASE_DERIVED := .build/cazenv-release
SOURCES := \
	src/main.c \
	src/caz_core.c \
	src/caz_cpu.c \
	src/caz_body.c \
	src/caz_droid.c \
	src/caz_opencat.c \
	src/caz_loader.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)
CAZENV_SURVIVAL_SOURCES := \
	cazenv/Tools/RunSurvivalTest.c \
	cazenv/c_core/caz_env.c \
	src/caz_core.c \
	src/caz_cpu.c \
	src/caz_body.c \
	src/caz_droid.c \
	src/caz_loader.c

.PHONY: all run version cazenv-version-config release-src release-packages cazenv-maccatalyst-package cazenv-ios-package cazenv-probes cazenv-regression cazenv-stress cazenv-movement cazenv-survival cazenv-survival-compat cazenv-survival-strict cazenv-survival-matrix cazenv-survival-long clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: src/%.c src/caz_core.h src/caz_cpu.h src/caz_body.h src/caz_droid.h src/caz_opencat.h src/caz_loader.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	$(TARGET) --program curious-patrol --scenario kitchen --steps 48

version: $(TARGET)
	@$(TARGET) --core-version

cazenv-version-config:
	@scripts/caz_version_config.sh >/dev/null

release-src: $(TARGET)
	mkdir -p dist
	set -e; \
	VERSION="$$( $(TARGET) --core-version )"; \
	SRC_ROOT="caz-$$VERSION"; \
	SRC_STAGE="$$(mktemp -d)/$$SRC_ROOT"; \
	rsync -a ./ "$$SRC_STAGE"/ \
	  --exclude .git \
	  --exclude build \
	  --exclude .build \
	  --exclude dist \
	  --exclude "cazenv/build" \
	  --exclude "*.xcuserstate" \
	  --exclude "xcuserdata" \
	  --exclude ".DS_Store"; \
	ditto -c -k --keepParent "$$SRC_STAGE" "dist/caz-src-$$VERSION.zip"; \
	ls -lh "dist/caz-src-$$VERSION.zip"; \
	shasum -a 256 "dist/caz-src-$$VERSION.zip"

release-packages: release-src cazenv-maccatalyst-package cazenv-ios-package

cazenv-maccatalyst-package: $(TARGET) cazenv-version-config
	mkdir -p dist
	set -e; \
	VERSION="$$( $(TARGET) --core-version )"; \
	DERIVED="$(CAZENV_RELEASE_DERIVED)-maccatalyst"; \
	xcodebuild \
	  -project $(CAZENV_CATALYST_PROJECT) \
	  -scheme CazEnvCatalyst \
	  -configuration Release \
	  -destination "generic/platform=macOS,variant=Mac Catalyst" \
	  -derivedDataPath "$$DERIVED" \
	  CODE_SIGNING_ALLOWED=NO \
	  CODE_SIGNING_REQUIRED=NO \
	  CODE_SIGN_IDENTITY="" \
	  build; \
	APP_PATH="$$DERIVED/Build/Products/Release-maccatalyst/CazEnv.app"; \
	ditto -c -k --keepParent "$$APP_PATH" "dist/cazenv-maccatalyst-$$VERSION.zip"; \
	ls -lh "dist/cazenv-maccatalyst-$$VERSION.zip"; \
	shasum -a 256 "dist/cazenv-maccatalyst-$$VERSION.zip"

cazenv-ios-package: $(TARGET) cazenv-version-config
	mkdir -p dist
	set -e; \
	VERSION="$$( $(TARGET) --core-version )"; \
	DERIVED="$(CAZENV_RELEASE_DERIVED)-ios"; \
	xcodebuild \
	  -project $(CAZENV_CATALYST_PROJECT) \
	  -scheme CazEnvCatalyst \
	  -configuration Release \
	  -destination "generic/platform=iOS" \
	  -derivedDataPath "$$DERIVED" \
	  CODE_SIGNING_ALLOWED=NO \
	  CODE_SIGNING_REQUIRED=NO \
	  CODE_SIGN_IDENTITY="" \
	  build; \
	APP_PATH="$$DERIVED/Build/Products/Release-iphoneos/CazEnv.app"; \
	ditto -c -k --keepParent "$$APP_PATH" "dist/cazenv-ios-$$VERSION.zip"; \
	ls -lh "dist/cazenv-ios-$$VERSION.zip"; \
	shasum -a 256 "dist/cazenv-ios-$$VERSION.zip"

cazenv-probes: $(CAZENV_SURVIVAL)
	$(CAZENV_SURVIVAL) --mode probes

cazenv-regression: $(CAZENV_SURVIVAL)
	$(CAZENV_SURVIVAL) --mode regression

cazenv-stress: $(CAZENV_SURVIVAL)
	$(CAZENV_SURVIVAL) --mode stress

cazenv-movement: $(CAZENV_SURVIVAL)
	$(CAZENV_SURVIVAL) --mode movement

cazenv-survival: cazenv-survival-strict

cazenv-survival-compat: cazenv-regression cazenv-stress
	$(CAZENV_SURVIVAL) --mode short-compat 14 3

cazenv-survival-strict: cazenv-regression cazenv-stress
	$(CAZENV_SURVIVAL) --mode short-strict 14 3

cazenv-survival-matrix: cazenv-regression cazenv-stress
	$(CAZENV_SURVIVAL) --mode matrix 30 5

cazenv-survival-long: cazenv-survival-matrix

$(CAZENV_SURVIVAL): $(CAZENV_SURVIVAL_SOURCES) cazenv/c_core/caz_env.h src/caz_core.h
	mkdir -p $(BUILD_DIR)
	$(CC) -Icazenv/c_core $(CFLAGS) $(CAZENV_SURVIVAL_SOURCES) -o $@ -lm

clean:
	rm -rf $(BUILD_DIR)
