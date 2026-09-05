FROM debian:trixie-slim
RUN apt-get update \
    && apt-get install -y --no-install-recommends git ca-certificates gcc-aarch64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*
