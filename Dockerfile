FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    make \
    git \
    libelf-dev \
    libssl-dev \
    libpsl-dev \
    cmake \
    build-essential


WORKDIR /
RUN git clone https://github.com/tspader/spn.git

WORKDIR /spn
RUN make
# RUN make examples
