# syntax=docker/dockerfile:1.7-labs

########################################
# Kolosal Server – CUDA Docker image
# - Multi-stage: build (devel) -> runtime (slim)
# - Defaults to GPU (CUDA) build
# - Uses config_rms.yaml as default config
# - Copies only required runtime bits
########################################

ARG CUDA_VERSION=12.4.1
ARG BASE_IMAGE=nvidia/cuda:${CUDA_VERSION}-devel-ubuntu22.04
FROM ${BASE_IMAGE} AS build

ARG DEBIAN_FRONTEND=noninteractive
ARG TZ=UTC
ARG BUILD_TYPE=Release
ARG ENABLE_CUDA=ON
ARG ENABLE_NATIVE_OPTIMIZATION=OFF
ARG USE_PODOFO=ON

ENV TZ=${TZ} \
    CC=gcc \
    CXX=g++ \
    BUILD_TYPE=${BUILD_TYPE}

# Build dependencies (system CURL required by inference/CMakeLists on Linux)
RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential git pkg-config ca-certificates curl \
  cmake ninja-build ccache \
  libcurl4-openssl-dev libssl-dev libbz2-dev \
  libomp-dev libblas-dev liblapack-dev \
      # PDF (PoDoFo) optional deps – safe to install even if disabled
      libfreetype6-dev libjpeg-dev libpng-dev libtiff-dev libxml2-dev libfontconfig1-dev \
    && rm -rf /var/lib/apt/lists/*

# Speed up rebuilds
ENV PATH=/usr/lib/ccache:${PATH} \
    CCACHE_DIR=/root/.ccache \
    CCACHE_MAXSIZE=1G

WORKDIR /src

# Copy repository (rely on .dockerignore to keep context small)
COPY . .

# Initialize submodules when available (no-op if not a git context)
RUN if [ -d .git ]; then git submodule update --init --recursive; else echo "No .git directory – skipping submodules"; fi

# Ensure llama.cpp source is present (fallback when submodules are not in context)
RUN set -eux; \
    if [ ! -f inference/external/llama.cpp/CMakeLists.txt ] && [ ! -f external/llama.cpp/CMakeLists.txt ]; then \
      echo "[Docker build] llama.cpp not found in repo – cloning shallow copy..."; \
      mkdir -p inference/external; \
      git clone --depth=1 https://github.com/ggerganov/llama.cpp.git inference/external/llama.cpp; \
    else \
      echo "[Docker build] Found llama.cpp sources in repo"; \
    fi

# Configure & build (CUDA by default)
RUN set -eux; \
    cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} \
      -DENABLE_NATIVE_OPTIMIZATION=${ENABLE_NATIVE_OPTIMIZATION} \
      -DUSE_CUDA=${ENABLE_CUDA} \
      -DUSE_PODOFO=${USE_PODOFO}; \
    cmake --build build --config ${BUILD_TYPE}

# Determine single-config output dir (project sets output to build/<TYPE>)
RUN set -eux; \
    OUTDIR="build/${BUILD_TYPE}"; \
    test -x "${OUTDIR}/kolosal-server" || { echo "Build output not found at ${OUTDIR}"; ls -la build || true; exit 1; };

# Collect runtime payload
RUN set -eux; \
    OUTDIR="build/${BUILD_TYPE}"; \
    strip -s "${OUTDIR}/kolosal-server" || true; \
    mkdir -p /out/bin /out/config /out/libs /out/licenses; \
    cp "${OUTDIR}/kolosal-server" /out/bin/; \
    # Prefer configs/config_rms.yaml by default
    if [ -f configs/config_rms.yaml ]; then \
      cp configs/config_rms.yaml /out/config/config_rms.yaml; \
      cp configs/config_rms.yaml /out/config/config.yaml; \
    elif [ -f config_rms.yaml ]; then \
      cp config_rms.yaml /out/config/config_rms.yaml; \
      cp config_rms.yaml /out/config/config.yaml; \
    elif [ -f config.yaml ]; then \
      cp config.yaml /out/config/config.yaml; \
    else \
      echo "No config found; you can mount one at runtime"; \
    fi; \
    # Shared libs placed by post-build step alongside the exe
    for p in libllama-*.so* libkolosal_server.so*; do \
      if ls "${OUTDIR}/$p" 1>/dev/null 2>&1; then \
        cp -n "${OUTDIR}/"$p /out/libs/ || true; \
      fi; \
    done; \
    # Non-system dependencies referenced by the binary
    ldd "${OUTDIR}/kolosal-server" | awk '{for(i=1;i<=NF;i++) if ($i ~ /\//) print $i}' | sort -u > /tmp/libs.txt || true; \
    while read -r lib; do case "$lib" in /lib/*|/usr/lib/*) ;; *) cp -n "$lib" /out/libs/ 2>/dev/null || true ;; esac; done < /tmp/libs.txt; \
    cp LICENSE /out/licenses/ 2>/dev/null || true; \
    echo "Collected libs:"; ls -1 /out/libs || true

########################################
# Runtime image
########################################
FROM nvidia/cuda:${CUDA_VERSION}-runtime-ubuntu22.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive
ENV LD_LIBRARY_PATH=/usr/local/lib:/app/libs:/usr/local/cuda/lib64 \
    KOL_MODELS_DIR=/app/models

# Minimal runtime deps (keep in sync with ldd if needed)
RUN apt-get update && apt-get install -y --no-install-recommends \
      libcurl4 libssl3 libbz2-1.0 zlib1g libgomp1 ca-certificates curl tini \
      # PoDoFo runtime libs (safe if unused)
      libfreetype6 libjpeg-turbo8 libpng16-16 libtiff5 libxml2 fontconfig \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /out/bin/kolosal-server /usr/local/bin/kolosal-server
COPY --from=build /out/config /app/config
COPY --from=build /out/libs /app/libs
COPY --from=build /out/licenses /licenses

# Make inference libs discoverable and refresh linker cache
RUN set -eux; \
    mkdir -p /usr/local/lib; \
    if ls /app/libs/libllama-*.so* 1>/dev/null 2>&1; then cp /app/libs/libllama-*.so* /usr/local/lib/ || true; fi; \
    if ls /app/libs/libkolosal_server.so* 1>/dev/null 2>&1; then cp /app/libs/libkolosal_server.so* /usr/local/lib/ || true; fi; \
    ldconfig || true

# Simple entrypoint wrapper – use config_rms.yaml by default when present
RUN printf '%s\n' '#!/bin/sh' \
  'set -e' \
  'CONFIG="/app/config/config_rms.yaml"' \
  'if [ ! -f "$CONFIG" ]; then CONFIG="/app/config/config.yaml"; fi' \
  'echo "Starting kolosal-server with: $CONFIG"' \
  'exec kolosal-server --config "$CONFIG"' \
  > /usr/local/bin/kolosal-entry.sh && chmod +x /usr/local/bin/kolosal-entry.sh

# Non-root runtime
RUN useradd -r -u 10001 -d /app kolosal && chown -R kolosal:kolosal /app
USER kolosal

VOLUME ["/app/models", "/app/data"]

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
  CMD curl -fsS http://localhost:8080/v1/health || exit 1

ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/kolosal-entry.sh"]
