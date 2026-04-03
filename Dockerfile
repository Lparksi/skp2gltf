FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    DISPLAY=:99 \
    WINEARCH=win64 \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES=mscoree,mshtml= \
    WINEPREFIX=/root/.wine

# Install base dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        wget \
        gnupg2 \
        xvfb \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Setup Box64 and Wine based on architecture
RUN if [ "$(uname -m)" = "aarch64" ]; then \
        # 1. Install Box64 via ryanfortner's repo (The standard for ARM64)
        wget -qO- https://ryanfortner.github.io/box64-debs/KEY.gpg | gpg --dearmor -o /etc/apt/trusted.gpg.d/box64-debs-archive-keyring.gpg && \
        echo "deb [arch=arm64] https://ryanfortner.github.io/box64-debs/ ./" > /etc/apt/sources.list.d/box64-debs.list && \
        apt-get update && apt-get install -y box64 && \
        # 2. Extract Kron4ek's Portable Wine (amd64) to /opt/wine-stable
        # This version is standalone and avoided apt dependency hell on ARM64
        mkdir -p /opt/wine-stable && \
        curl -SL https://github.com/Kron4ek/Wine-Builds/releases/download/9.0/wine-9.0-amd64.tar.xz | tar -xJ -C /opt/wine-stable --strip-components=1 && \
        ln -s /opt/wine-stable/bin/wine64 /usr/local/bin/wine64 && \
        ln -s /opt/wine-stable/bin/wine /usr/local/bin/wine; \
    else \
    # Standard x86_64 path (Requires i386 for Wine)
        dpkg --add-architecture i386 && \
        mkdir -pm755 /etc/apt/keyrings && \
        curl -fsSL https://dl.winehq.org/wine-builds/winehq.key | gpg --dearmor -o /etc/apt/keyrings/winehq-archive.key && \
        curl -fsSL https://dl.winehq.org/wine-builds/debian/dists/bookworm/winehq-bookworm.sources -o /etc/apt/sources.list.d/winehq-bookworm.sources && \
        apt-get update && \
        apt-get install -y --no-install-recommends winehq-stable; \
    fi && \
    rm -rf /var/lib/apt/lists/*

# Pre-initialize Win64 prefix
RUN Xvfb :99 -screen 0 1024x768x24 & \
    PID=$! && \
    sleep 5 && \
    # Ensure we use wine64 if it's available
    export WINEDEBUG=-all && \
    if command -v wine64 >/dev/null 2>&1; then \
        DISPLAY=:99 wine64 wineboot --init; \
    else \
        DISPLAY=:99 wine wineboot --init; \
    fi && \
    wineserver -w && \
    kill $PID || true

WORKDIR /app

# The CI pipeline downloads Windows build artifacts into docker-bin/.
# Copy all runtime files so required MSVC/SDK DLLs are always included.
COPY docker-bin/ /app/

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh && chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
