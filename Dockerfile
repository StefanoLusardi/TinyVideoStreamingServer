# syntax=docker/dockerfile:1

# ---------- 1) build ----------
FROM ubuntu:24.04 AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ cmake make libboost-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt .
COPY src/ src/
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j"$(nproc)" \
 && strip build/mjpeg-proxy

# ---------- 2) proxy (target: proxy) ----------
FROM ubuntu:24.04 AS proxy
COPY --from=build /src/build/mjpeg-proxy /usr/local/bin/mjpeg-proxy
USER ubuntu
EXPOSE 8080
# argomenti: <url-upstream> [porta=8080] [bind=0.0.0.0]
ENTRYPOINT ["mjpeg-proxy"]

# ---------- 3) TLS con Caddy (target: tls) ----------
FROM caddy:2-alpine AS tls
COPY Caddyfile /etc/caddy/Caddyfile
