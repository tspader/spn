FROM frolvlad/alpine-glibc:latest

RUN apk add --no-cache git gcc musl-dev \
    && mkdir -p /root/.config/spn /work \
    && echo 'spn = "/work/spn"' > /root/.config/spn/spn.toml

ENV PATH="/spn/bin:${PATH}"
