#!/usr/bin/env bash
# Da includere con `source` negli altri script (non si lancia da solo).
#   $1 = Debug | Release  (default: Debug)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Il profilo Conan sta nel repo (.conan2/profiles): lo usiamo come CONAN_HOME.
export CONAN_HOME="${CONAN_HOME:-$ROOT/.conan2}"

BUILD_TYPE="${1:-Debug}"
case "$BUILD_TYPE" in
    Debug|Release) ;;
    *) echo "uso: $0 [Debug|Release]" >&2; exit 2 ;;
esac
PRESET="conan-$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
