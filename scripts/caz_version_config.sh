#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HEADER="${ROOT_DIR}/src/caz_core.h"
OUTPUT="${ROOT_DIR}/cazenv/Config/CazVersion.xcconfig"

VERSION=$(sed -n 's/^#define CAZ_CORE_VERSION "\([^"]*\)".*/\1/p' "$HEADER")
VERSION_NUMBER=$(sed -n 's/^#define CAZ_CORE_VERSION_NUMBER \([0-9][0-9]*\)u.*/\1/p' "$HEADER")

if [ -z "$VERSION" ] || [ -z "$VERSION_NUMBER" ]; then
    echo "Unable to read CAZ_CORE_VERSION values from $HEADER" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT")"
TMP_OUTPUT="${OUTPUT}.tmp"
{
    echo "// Generated from src/caz_core.h. Commit this file with Caz Core version changes."
    echo "CAZ_CORE_VERSION = ${VERSION}"
    echo "CAZ_CORE_VERSION_NUMBER = ${VERSION_NUMBER}"
    echo 'MARKETING_VERSION = $(CAZ_CORE_VERSION)'
    echo 'CURRENT_PROJECT_VERSION = $(CAZ_CORE_VERSION_NUMBER)'
} > "$TMP_OUTPUT"
mv "$TMP_OUTPUT" "$OUTPUT"

printf '%s\n' "$VERSION"
