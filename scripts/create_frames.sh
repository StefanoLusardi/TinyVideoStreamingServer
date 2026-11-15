#!/usr/bin/env bash
# Genera i frame JPEG di prova per due camere finte (frames1/ e frames2/).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

mkdir -p frames1 frames2
ffmpeg -loglevel error -y -f lavfi -i "testsrc=size=640x480:rate=15"  -t 10 -q:v 5 frames1/%04d.jpg
ffmpeg -loglevel error -y -f lavfi -i "testsrc2=size=640x480:rate=15" -t 10 -q:v 5 frames2/%04d.jpg
echo "frame generati in frames1/ e frames2/"
