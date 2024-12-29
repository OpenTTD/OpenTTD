# --------------------------------------------------------
# 1) Base Image
# --------------------------------------------------------
FROM ubuntu:22.04

LABEL maintainer="Santhosh Janardhanan <santhoshj@gmail.com>" \
    description="Dockerfile to build and run the latest OpenTTD from source."
ARG APPVER=14.1
# --------------------------------------------------------
# 2) Environment Variables for Server Configuration
# --------------------------------------------------------
# These can be overridden at 'docker run' or in a docker-compose.yml
ENV SERVER_NAME="Docker OpenTTD" \
    SERVER_PASSWORD="" \
    COMPANY_PASSWORD="" \
    START_YEAR="1950" \
    PORT="3979"

# --------------------------------------------------------
# 3.1) Install Dependencies + Build Tools
# --------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Tools for building breakpad, zlib, and openttd
    git \
    curl \
    autoconf \
    automake \
    libtool \
    pkg-config \
    build-essential \
    cmake \
    wget\
    xz-utils\
    unzip\
    ca-certificates\
    # OpenTTD optional/encouraged libraries (dev packages, except zlib):
    liblzma-dev \
    libpng-dev \
    liblzo2-dev \
    libfreetype6-dev \
    libfontconfig1-dev \
    libicu-dev \
    libharfbuzz-dev \
    libcurl4-openssl-dev \
    libsdl2-dev \
    # Clean up apt lists
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------
# 3.2) Build zlib from source
# ------------------------------------------------------
ENV ZLIB_VERSION=1.3.1
WORKDIR /tmp
RUN curl -LO "https://www.zlib.net/zlib-${ZLIB_VERSION}.tar.gz" \
    && tar zxvf "zlib-${ZLIB_VERSION}.tar.gz" \
    && cd "zlib-${ZLIB_VERSION}" \
    && ./configure --prefix=/usr/local \
    && make -j"$(nproc)" \
    && make install \
    && cd / && rm -rf /tmp/zlib*

WORKDIR /tmp
RUN git clone https://chromium.googlesource.com/breakpad/breakpad
WORKDIR /tmp/breakpad/
RUN git clone https://chromium.googlesource.com/linux-syscall-support
RUN mkdir -p /tmp/breakpad/src/third_party/lss/ && mv linux-syscall-support/linux_syscall_support.h src/third_party/lss/
WORKDIR /tmp/breakpad
# Pull linux-syscall-support submodule
RUN ./configure && make && \
    make install
# 'make install' is not provided by breakpad, so we do it manually:
# mkdir -p /usr/local/include/breakpad && \
# mkdir -p /usr/local/lib && \
# cp -r src/* /usr/local/include/breakpad/ && \
# find . -name '*.a' -exec cp {} /usr/local/lib/ \; && \
# Cleanup
# cd / && rm -rf /tmp/breakpad

# --------------------------------------------------------
# 4) Pull the Latest OpenTTD Source
# --------------------------------------------------------
WORKDIR /build
RUN git clone --depth=1 --branch ${APPVER} https://github.com/OpenTTD/OpenTTD.git

# --------------------------------------------------------
# 5) Build OpenTTD (Release Mode)
# --------------------------------------------------------
WORKDIR /build/OpenTTD
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release &&\
    make -j"$(nproc)" && \
    make install

# --------------------------------------------------------
# 6) Expose Ports (TCP & UDP)
# --------------------------------------------------------
EXPOSE 3979/tcp
EXPOSE 3979/udp


# ------------------------------------------------------
# 1) Builder Stage: Compile OpenGFX
# ------------------------------------------------------
# FROM ubuntu:22.04 AS builder

# Install system dependencies and tools for building OpenGFX
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    python3 \
    python3-pip \
    make \
    imagemagick \
    optipng grfcodec\
    && rm -rf /var/lib/apt/lists/*

# Install NML (required for compiling OpenGFX)
RUN pip3 install nml

# Clone the OpenGFX repository
WORKDIR /build
RUN git clone https://github.com/OpenTTD/OpenGFX.git

# Compile OpenGFX via 'make'
WORKDIR /build/OpenGFX
RUN make



# --------------------------------------------------------
# 7) Default Command (Dedicated Server)
# --------------------------------------------------------
# We use 'sh -c' so that environment variables are expanded at runtime.
# -D = Dedicated (no GUI)
# -x <year> = Start year
# -n 0.0.0.0:<port> = Listen on all interfaces at <port>
#   (We combine that with the server name using a config approach or '-N' if supported.)
#
# In modern OpenTTD builds, there's no direct param for server_name; it typically reads openttd.cfg
# or is set by environment variables in official Docker images. However, you could:
#   - rely on openttd.cfg, or
#   - pass '-N "MyServer"' if your build supports it.
#
# For demonstration, we show a somewhat "hybrid" approach using '-n' for net config
# and a sample '-x' for start year. Adjust to your needs or configure openttd.cfg.

# COPY --from=builder /build/OpenGFX/build/ /build/OpenTTD/build/

# RUN cp -rvf /build/OpenGFX/* /build/OpenTTD/build/
RUN tar xf /build/OpenGFX/opengfx-*-master-*.tar -C /build/OpenTTD/build/baseset/
WORKDIR /build/OpenTTD
RUN mkdir /config
VOLUME [ "/config" ]
CMD sh -c "\
    echo 'Starting OpenTTD with server: ${SERVER_NAME}' && \
    ./build/openttd -D -c /config/openttd.cfg"