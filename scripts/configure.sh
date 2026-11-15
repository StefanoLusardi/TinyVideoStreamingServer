#!/usr/bin/env bash
# Configura CMake (richiede scripts/install.sh).   uso: scripts/configure.sh [Debug|Release]
source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"

cmake --preset "$PRESET"
