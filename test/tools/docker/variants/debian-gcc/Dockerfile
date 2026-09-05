FROM debian:trixie-slim
RUN apt-get update \
    && apt-get install -y --no-install-recommends git ca-certificates gcc g++ libc6-dev \
    && rm -rf /var/lib/apt/lists/*
