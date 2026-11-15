#!/usr/bin/env python3
"""Finto ESP32-CAM che imita lo stream di CameraWebServer (esp32-camera).

Riproduce in loop i JPEG di una cartella. Solo libreria standard.

  /stream   multipart/x-mixed-replace, Transfer-Encoding: chunked, come l'ESP32
  /capture  singolo JPEG

Come la scheda vera, per default serve UN solo stream alla volta: un secondo
client resta appeso finche' il primo non chiude (usa --multi per disattivarlo).
Cosi' si verifica che il proxy apra una sola connessione.

Frame di prova (vedi comando ffmpeg nel README / nella risposta):
  mkdir -p frames && ffmpeg -y -f lavfi -i "testsrc2=size=640x480:rate=15" \
      -t 10 -q:v 5 frames/%04d.jpg
"""
import argparse
import glob
import itertools
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BOUNDARY = "123456789000000000000987654321"
STREAM_LOCK = threading.Lock()  # l'esp_http_server della scheda serve uno stream per volta
STATE = {"active": 0}


class Cam(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    frames: list[bytes] = []
    args: argparse.Namespace

    def log_message(self, fmt, *a):  # log compatto sul nostro formato
        pass

    def log(self, msg):
        print(f"[{time.strftime('%H:%M:%S')}] {self.client_address[0]}: {msg}", flush=True)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/stream":
            self.stream()
        elif path == "/capture":
            jpg = self.frames[0]
            self.send_response(200)
            self.send_header("Content-Type", "image/jpeg")
            self.send_header("Content-Length", str(len(jpg)))
            self.end_headers()
            self.wfile.write(jpg)
        else:
            self.send_error(404)

    def chunk(self, data: bytes):
        self.wfile.write(b"%x\r\n" % len(data) + data + b"\r\n")

    def stream(self):
        self.close_connection = True
        multi = self.args.multi
        if not multi:
            self.log("stream richiesto, in attesa (la scheda serve un solo stream)" if STREAM_LOCK.locked() else "stream richiesto")
            STREAM_LOCK.acquire()
        STATE["active"] += 1
        self.log(f"stream avviato (connessioni attive: {STATE['active']})")
        sent, t0 = 0, time.monotonic()
        try:
            self.send_response(200)
            self.send_header("Content-Type", f"multipart/x-mixed-replace;boundary={BOUNDARY}")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("X-Framerate", "60")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            period = 1.0 / self.args.fps
            nxt = time.monotonic()
            for jpg in itertools.cycle(self.frames):
                now = time.monotonic()
                if self.args.drop_after and now - t0 > self.args.drop_after:
                    self.log(f"simulo guasto: chiudo dopo {self.args.drop_after:.0f} s")
                    break
                ts = time.time()
                # stessa sequenza di esp32-camera: header parte, JPEG, boundary
                self.chunk(
                    b"Content-Type: image/jpeg\r\nContent-Length: %d\r\nX-Timestamp: %d.%06d\r\n\r\n"
                    % (len(jpg), int(ts), int((ts % 1) * 1e6))
                )
                self.chunk(jpg)
                self.chunk(f"\r\n--{BOUNDARY}\r\n".encode())
                sent += 1
                nxt += period
                time.sleep(max(0.0, nxt - time.monotonic()))
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, TimeoutError):
            pass
        finally:
            STATE["active"] -= 1
            self.log(f"stream chiuso dopo {sent} frame ({sent / max(time.monotonic() - t0, 1e-6):.1f} fps)")
            if not multi:
                STREAM_LOCK.release()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", default="frames", help="cartella con i .jpg (default: frames)")
    ap.add_argument("--port", type=int, default=81)
    ap.add_argument("--fps", type=float, default=15)
    ap.add_argument("--multi", action="store_true", help="consenti piu' stream contemporanei")
    ap.add_argument("--drop-after", type=float, default=0, metavar="SEC",
                    help="chiudi lo stream dopo SEC secondi (per provare la riconnessione)")
    args = ap.parse_args()

    files = sorted(glob.glob(f"{args.frames}/*.jpg"))
    if not files:
        sys.exit(f"nessun .jpg in '{args.frames}/': genera i frame con ffmpeg (vedi docstring)")
    Cam.frames = [open(f, "rb").read() for f in files]
    Cam.args = args
    avg = sum(map(len, Cam.frames)) // len(Cam.frames) // 1024
    print(f"{len(files)} frame (~{avg} KiB ciascuno), {args.fps:g} fps, porta {args.port}, "
          f"{'multi-stream' if args.multi else 'un solo stream alla volta'}", flush=True)

    srv = ThreadingHTTPServer(("0.0.0.0", args.port), Cam)
    srv.daemon_threads = True
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
