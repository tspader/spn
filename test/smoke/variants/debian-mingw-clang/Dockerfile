FROM debian:trixie-slim
RUN apt-get update \
    && apt-get install -y --no-install-recommends git ca-certificates clang lld gcc-mingw-w64-x86-64 \
    && rm -rf /var/lib/apt/lists/*
