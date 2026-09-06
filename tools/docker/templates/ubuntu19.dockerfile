FROM ubuntu:19.04
ENV DEBIAN_FRONTEND=noninteractive
RUN sed -i 's/\(archive\|security\)\.ubuntu\.com/old-releases.ubuntu.com/g' /etc/apt/sources.list && apt-get update && apt-get install -y --no-install-recommends {{.packages}} && rm -rf /var/lib/apt/lists/*
