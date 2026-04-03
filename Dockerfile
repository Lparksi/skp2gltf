FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    DISPLAY=:99 \
    WINEARCH=win64 \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES=mscoree,mshtml= \
    WINEPREFIX=/root/.wine

# Install dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
        cmake \
        build-essential \
        python3 \
        xvfb \
        gnupg2 \
    && rm -rf /var/lib/apt/lists/*

# Install Box64 for arm64 (Build from source for optimal performance)
RUN if [ "$(uname -m)" = "aarch64" ]; then \
        git clone https://github.com/ptitSeb/box64.git --depth 1 && \
        cd box64 && mkdir build && cd build && \
        cmake .. -DARM64=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo && \
        make -j$(nproc) && make install && \
        cd ../.. && rm -rf box64; \
    fi

# Setup Wine (Using official WineHQ repo structure or architecture-aware install)
# Note: On aarch64, we need the amd64 version of wine to run x64 exes via Box64.
RUN dpkg --add-architecture amd64 && \
    mkdir -pm755 /etc/apt/keyrings && \
    curl -fsSL https://dl.winehq.org/wine-builds/winehq.key | gpg --dearmor -o /etc/apt/keyrings/winehq-archive.key && \
    curl -fsSL https://dl.winehq.org/wine-builds/debian/dists/bookworm/winehq-bookworm.sources -o /etc/apt/sources.list.d/winehq-bookworm.sources && \
    apt-get update && \
    if [ "$(uname -m)" = "aarch64" ]; then \
        # On ARM64, we manually pull the amd64 packages for box64 to use
        apt-get install -y --no-install-recommends winehq-stable:amd64 || \
        (apt-get download wine-stable-amd64 wine-stable:amd64 && dpkg -i --force-depends *.deb && rm *.deb); \
    else \
        apt-get install -y --no-install-recommends winehq-stable; \
    fi && \
    rm -rf /var/lib/apt/lists/*

# Pre-initialize Win64 prefix
RUN Xvfb :99 -screen 0 1024x768x24 & \
    PID=$! && \
    sleep 2 && \
    DISPLAY=:99 WINEDEBUG=-all wine64 wineboot --init && \
    wineserver -w && \
    kill $PID || true

WORKDIR /app

# The CI pipeline downloads Windows build artifacts into docker-bin/.
# Copy all runtime files so required MSVC/SDK DLLs are always included.
COPY docker-bin/ /app/

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh && chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
