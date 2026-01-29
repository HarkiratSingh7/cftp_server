# Base image
FROM debian:bookworm

# Metadata
LABEL maintainer="honey.harkirat@outlook.com"
LABEL description="Dockerfile for cftp_server FTP project"

# Install required tools and libraries
RUN apt-get update -y && \
    apt-get install -y \
    cmake \
    gcc \
    clang \
    clang-format \
    gdb \
    htop \
    sudo \
    valgrind \
    make \
    libevent-dev \
    libssl-dev \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    python3-pytest\
    git \
    net-tools \
    checkinstall \
    bsdmainutils \
    zlib1g-dev \
    python3-venv \
    tcpdump \
    iproute2 \
    netcat-openbsd \
    lftp \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

RUN apt update -y && \
    apt install -y \
    ftp

RUN python3 -m pip install pre-commit

CMD ["./build/cftp_server"]

# Expose the port CFTP server will run on
EXPOSE 21
EXPOSE 1024-65535