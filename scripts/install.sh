#!/usr/bin/env bash
# Installa le dipendenze con Conan e genera i preset CMake.   uso: scripts/install.sh [Debug|Release]
source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"

conan install . -s build_type="$BUILD_TYPE" --build=missing
