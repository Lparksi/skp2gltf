FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    DISPLAY=:99 \
    WINEARCH=win64 \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES=mscoree,mshtml= \
    WINEPREFIX=/root/.wine

# Install dependencies and Wine
# For x86_64 hosts, we need both wine64 and wine32 (i386) for full functionality.
# For arm64 hosts, we use qemu-user-static to emulate x86_64.
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        wine \
        wine64 \
        wine32:i386 \
        xvfb \
        qemu-user-static \
    && rm -rf /var/lib/apt/lists/*

# Pre-initialize the Wine prefix to avoid slow first-run initialization during tests
RUN Xvfb :99 -screen 0 1024x768x24 & \
    PID=$! && \
    sleep 2 && \
    DISPLAY=:99 WINEDEBUG=-all wineboot --init && \
    wineserver -w && \
    kill $PID || true

WORKDIR /app

# The CI pipeline downloads Windows build artifacts into docker-bin/.
# Copy all runtime files so required MSVC/SDK DLLs are always included.
COPY docker-bin/ /app/

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh && chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
