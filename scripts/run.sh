#!/usr/bin/env bash
# Avvia due camere finte e il proxy. Ctrl-C ferma tutto.   uso: scripts/run.sh [Debug|Release]
# Il binario si puo' scegliere con MJPEG_PROXY_BIN=/percorso/mjpeg-proxy
source "$(dirname "${BASH_SOURCE[0]}")/_common.sh"

BIN="${MJPEG_PROXY_BIN:-$ROOT/build/$BUILD_TYPE/mjpeg-proxy}"
[[ -x "$BIN" ]] || { echo "binario non trovato: $BIN (esegui scripts/build.sh $BUILD_TYPE)" >&2; exit 1; }
[[ -d frames1 && -d frames2 ]] || "$ROOT/scripts/create_frames.sh"

# All'uscita (anche con Ctrl-C) chiude le camere finte avviate in background.
trap 'kill $(jobs -p) 2>/dev/null || true' EXIT

python3 tools/fake_esp32.py --port 9005 --frames frames1 &
python3 tools/fake_esp32.py --port 9006 --frames frames2 &

echo "apri http://127.0.0.1:8080"
"$BIN" --port 8080 \
    salotto=http://127.0.0.1:9005/stream \
    garage=http://127.0.0.1:9006/stream
