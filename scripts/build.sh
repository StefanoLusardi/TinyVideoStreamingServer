#!/usr/bin/env bash
# Compila (richiede scripts/configure.sh).   uso: scripts/build.sh [Debug|Release]
source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"

cmake --build --preset "$PRESET"
