FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends {{.packages}} && rm -rf /var/lib/apt/lists/*
