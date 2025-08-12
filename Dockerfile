# syntax=docker/dockerfile:1.7-labs
###############################
# Kolosal Server Dockerfile
#
# Improvements over previous version:
# - Multi-stage build (builder + slim runtime)
# - Ninja + ccache for faster builds
# - Build args for configurability (CMAKE_VERSION, BUILD_TYPE, ENABLE_NATIVE_OPTIMIZATION, ENABLE_OPENCL, RUN_TESTS)
# - Cache-friendly layering: copy meta files first
# - Optional tests (disabled by default)
# - Strips binaries & copies only required runtime artifacts
# - Non-root runtime with Tini as entrypoint
# - Basic volume mounts for models/data
# - Healthcheck retained (tunable)
###############################

# Default to CUDA-enabled build using NVIDIA CUDA devel image
# To use a pure CPU build instead, override BASE_IMAGE (and optionally set ENABLE_CUDA=OFF)
ARG CUDA_VERSION=12.4.1
ARG BASE_IMAGE=nvidia/cuda:${CUDA_VERSION}-devel-ubuntu22.04
FROM ${BASE_IMAGE} AS build

ARG DEBIAN_FRONTEND=noninteractive
ARG TZ=UTC
ARG CMAKE_VERSION=3.25.3
ARG BUILD_TYPE=Release
ARG ENABLE_NATIVE_OPTIMIZATION=OFF
ARG ENABLE_OPENCL=ON
ARG ENABLE_CUDA=ON
ARG RUN_TESTS=OFF
ARG GGML_CUDA_NO_VMM=ON
ARG USE_PODOFO=ON
ARG TARGETARCH

ENV TZ=${TZ} \
    CC=gcc \
    CXX=g++ \
    BUILD_TYPE=${BUILD_TYPE}

# System dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential git pkg-config ca-certificates \
    curl wget gpg software-properties-common \
    libcurl4-openssl-dev libyaml-cpp-dev libssl-dev libbz2-dev \
    python3 python3-pip ninja-build ccache \
    ocl-icd-opencl-dev opencl-headers \
    # PoDoFo / PDF support dependencies (needed if USE_PODOFO=ON) \
    libfreetype6-dev libjpeg-dev libpng-dev libtiff-dev libxml2-dev libfontconfig1-dev \
  && rm -rf /var/lib/apt/lists/*

# Install pinned CMake
RUN set -eux; \
    if ! command -v cmake >/dev/null || [ "$(cmake --version | awk 'NR==1{print $3}')" != "${CMAKE_VERSION}" ]; then \
      cd /tmp; \
      wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz; \
      tar -xf cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz; \
      cp -r cmake-${CMAKE_VERSION}-linux-x86_64/bin/* /usr/local/bin/; \
      cp -r cmake-${CMAKE_VERSION}-linux-x86_64/share/cmake* /usr/local/share/ || true; \
      rm -rf cmake-${CMAKE_VERSION}-linux-x86_64*; \
    fi; \
    cmake --version

# CCache setup
ENV PATH=/usr/lib/ccache:${PATH} \
    CCACHE_DIR=/root/.ccache \
    CCACHE_MAXSIZE=1G

WORKDIR /src

# Copy entire project (relying on .dockerignore to trim context). This avoids failures when optional files are absent.
COPY . .

# Init submodules if git metadata present
RUN if [ -d .git ]; then git submodule update --init --recursive; else echo "No .git directory – assuming external deps vendored"; fi

# Configure & build
RUN set -eux; \
    cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DENABLE_NATIVE_OPTIMIZATION=${ENABLE_NATIVE_OPTIMIZATION} \
      -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} \
      -DENABLE_OPENCL=${ENABLE_OPENCL} \
  -DUSE_PODOFO=${USE_PODOFO} \
  -DGGML_CUDA=${ENABLE_CUDA} \
  -DGGML_CUDA_NO_VMM=${GGML_CUDA_NO_VMM}; \
    cmake --build build --config ${BUILD_TYPE}

# Sanity check: ensure CUDA library exists when CUDA is enabled
RUN set -eux; \
    if [ "${ENABLE_CUDA}" = "ON" ]; then \
      if ! find build -maxdepth 4 -type f -name 'libllama-cuda.so' | grep -q .; then \
        echo 'ERROR: Expected libllama-cuda.so not found in build output while ENABLE_CUDA=ON'; \
        echo 'Listing candidate llama libs:'; find build -type f -name 'libllama-*' -printf '%p\n' || true; \
        exit 1; \
      else \
        echo 'Found libllama-cuda.so:'; find build -maxdepth 4 -type f -name 'libllama-cuda.so'; \
      fi; \
    else \
      echo 'ENABLE_CUDA=OFF, skipping CUDA library presence check'; \
    fi

# Optional test run
RUN if [ "${RUN_TESTS}" = "ON" ]; then ctest --test-dir build -j"$(nproc)" --output-on-failure; else echo "Tests skipped"; fi

# Collect runtime artifacts
RUN set -eux; \
    strip -s build/kolosal-server || true; \
    mkdir -p /out/bin /out/config /out/libs /out/licenses; \
    cp build/kolosal-server /out/bin/; \
    # Preserve all potential config variants
    [ -f config-rms.yaml ] && cp config-rms.yaml /out/config/config-rms.yaml || true; \
    [ -f config_rms.yaml ] && cp config_rms.yaml /out/config/config_rms.yaml || true; \
    [ -f config.yaml ] && cp config.yaml /out/config/config.yaml.orig || true; \
    # Determine primary config.yaml (preference: config-rms.yaml, config_rms.yaml, config.yaml.orig)
    if [ -f /out/config/config-rms.yaml ]; then cp /out/config/config-rms.yaml /out/config/config.yaml; \
    elif [ -f /out/config/config_rms.yaml ]; then cp /out/config/config_rms.yaml /out/config/config.yaml; \
    elif [ -f /out/config/config.yaml.orig ]; then mv /out/config/config.yaml.orig /out/config/config.yaml; \
    else echo "No config found; provide one at runtime"; fi; \
    # Copy inference engine plugin/shared libraries (not always on main binary's ldd)
    find build -type f -name 'libllama-*.so*' -exec cp -n {} /out/libs/ \; || true; \
    # Attempt to gather non-system shared libs referenced by the binary
    ldd build/kolosal-server | awk '{for(i=1;i<=NF;i++) if ($i ~ /\//) print $i}' | sort -u > /tmp/libs_list.txt || true; \
    while read -r lib; do \
      case "$lib" in \
        /lib/*|/usr/lib/*) ;; \
        *) cp -n "$lib" /out/libs/ 2>/dev/null || true ;; \
      esac; \
    done < /tmp/libs_list.txt; \
    cp LICENSE /out/licenses/ 2>/dev/null || true; \
    echo 'Collected config files:'; ls -1 /out/config || true; \
    echo 'Collected libs:'; ls -1 /out/libs || true

########################################
# Runtime stage
########################################
FROM nvidia/cuda:${CUDA_VERSION}-runtime-ubuntu22.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive
ENV LD_LIBRARY_PATH=/usr/local/lib:/app/libs:/usr/local/cuda/lib64 \
  KOL_CONFIG=/app/config/config.yaml \
  KOL_MODELS_DIR=/app/models \
  NVIDIA_VISIBLE_DEVICES=all \
  NVIDIA_DRIVER_CAPABILITIES=compute,utility

# Runtime dependencies (keep minimal & in sync with build ldd output)
RUN apt-get update && apt-get install -y --no-install-recommends \
      libcurl4 libyaml-cpp0.7 libssl3 libbz2-1.0 libgomp1 zlib1g \
      ca-certificates curl tini \
      # Runtime libs for PoDoFo (if enabled) \
      libfreetype6 libjpeg-turbo8 libpng16-16 libtiff5 libxml2 fontconfig \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /out/bin/kolosal-server /usr/local/bin/kolosal-server
COPY --from=build /out/config /app/config
COPY --from=build /out/libs /app/libs
COPY --from=build /out/licenses /licenses

# Make inference plugins available at expected path (/usr/local/lib)
RUN set -eux; \
    mkdir -p /usr/local/lib; \
    for pattern in 'libllama-*.so*' 'libkolosal_server.so*'; do \
      if ls /app/libs/$pattern 1> /dev/null 2>&1; then \
        cp /app/libs/$pattern /usr/local/lib/ || true; \
      fi; \
    done; \
    ldconfig || true; \
    echo 'Runtime libs in /usr/local/lib:'; ls -1 /usr/local/lib | grep -E 'lib(llama|kolosal)' || true; \
    if [ "${ENABLE_CUDA:-ON}" = "ON" ] && [ ! -f /usr/local/lib/libllama-cuda.so ]; then \
      echo 'WARNING: libllama-cuda.so missing in runtime stage even though ENABLE_CUDA=ON'; \
      echo 'Listing /app/libs contents for diagnostics:'; ls -l /app/libs | sed 's/^/  /'; \
    fi

# Optional debug env (set KOL_DEBUG=1 to list libs & config at container start)
ENV KOL_DEBUG=0

# Lightweight entry wrapper to provide diagnostics when requested
COPY --from=build /bin/sh /bin/sh

RUN printf '%s\n' '#!/bin/sh' \
  'set -e' \
  'if [ "$KOL_DEBUG" = "1" ]; then' \
  '  echo "[DEBUG] Config directory (/app/config):"; ls -l /app/config || true;' \
  '  echo "[DEBUG] Expected CUDA lib presence: "; if [ -f /usr/local/lib/libllama-cuda.so ]; then echo "FOUND"; else echo "MISSING"; fi' \
  '  echo "[DEBUG] Listing /usr/local/lib llama libs:"; ls -1 /usr/local/lib | grep libllama || true;' \
  'fi' \
  'if [ -f /app/config/config-rms.yaml ]; then' \
  '  echo "Using config-rms.yaml";' \
  '  exec kolosal-server --config /app/config/config-rms.yaml' \
  'else' \
  '  echo "Using config.yaml";' \
  '  exec kolosal-server --config /app/config/config.yaml' \
  'fi' > /usr/local/bin/kolosal-entry.sh && chmod +x /usr/local/bin/kolosal-entry.sh

# Non-root user
RUN useradd -r -u 10001 -d /app kolosal && chown -R kolosal:kolosal /app
USER kolosal

VOLUME ["/app/models", "/app/data"]

EXPOSE 8084

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD curl -fsS http://localhost:8084/health || exit 1

ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/kolosal-entry.sh"]

# Example builds:
# (Default CUDA build)
# docker build -t kolosal-server:cuda . \
#   --build-arg BUILD_TYPE=Release \
#   --build-arg ENABLE_CUDA=ON
# docker run --rm --gpus all -p 8084:8084 kolosal-server:cuda
#
# (CPU-only build)
# docker build -t kolosal-server:cpu . \
#   --build-arg ENABLE_CUDA=OFF \
#   --build-arg BASE_IMAGE=ubuntu:22.04 \
#   --build-arg BUILD_TYPE=Release
# docker run --rm -p 8084:8084 kolosal-server:cpu
