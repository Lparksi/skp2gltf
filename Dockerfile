FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    DISPLAY=:99 \
    WINEARCH=win64 \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES=mscoree,mshtml= \
    WINEPREFIX=/root/.wine

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        wine64 \
        xvfb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# The CI pipeline downloads Windows build artifacts into docker-bin/.
COPY docker-bin/skp2gltf.exe /app/skp2gltf.exe
COPY docker-bin/SketchUpAPI.dll /app/SketchUpAPI.dll
COPY docker-bin/SketchUpCommonPreferences.dll /app/SketchUpCommonPreferences.dll

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh && chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
