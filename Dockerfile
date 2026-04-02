FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    DISPLAY=:99 \
    WINEARCH=win64 \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES=mscoree,mshtml= \
    WINEPREFIX=/root/.wine

# Install dependencies and Wine
# For arm64, we need to use qemu-user-static to emulate x86_64
RUN dpkg --add-architecture amd64 && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        wine:amd64 \
        wine64:amd64 \
        xvfb \
        qemu-user-static \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# The CI pipeline downloads Windows build artifacts into docker-bin/.
# Copy all runtime files so required MSVC/SDK DLLs are always included.
COPY docker-bin/ /app/

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh && chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
