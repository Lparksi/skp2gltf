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
    && rm -rf /var/lib/apt/lists/*

# Add Box86/Box64 repository (Maintained by the community for ARM64 Debian/Raspbian)
# This repo provides architecture-aware Wine builds that work perfectly with Box64.
RUN if [ "$(uname -m)" = "aarch64" ]; then \
        wget https://itai-nelken.github.io/weekly-box86-repro/debian/box86.list -O /etc/apt/sources.list.d/box86.list && \
        wget -qO- https://itai-nelken.github.io/weekly-box86-repro/debian/pub.gpg | gpg --dearmor -o /etc/apt/trusted.gpg.d/box86-repo-gpg.gpg && \
        apt-get update && \
        apt-get install -y box64-android wine-64bit-amd64 && \
        # Create a symlink for consistency
        ln -s /usr/local/bin/box64 /usr/bin/box64 || true; \
    else \
        # Standard x86_64 path
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
