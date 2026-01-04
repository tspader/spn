FROM debian:trixie-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends git ca-certificates gcc libc6-dev \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /root/.config/spn /work \
    && echo 'spn = "/work/spn"' > /root/.config/spn/spn.toml

ENV PATH="/spn/bin:${PATH}"
