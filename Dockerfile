# Kolosal Server Dockerfile with Vulkan support on NVIDIA Container Toolkit
FROM ubuntu:22.04

# Set environment variables for NVIDIA runtime
ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=all

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update base system
RUN apt-get update && apt-get upgrade -y \
    && rm -rf /var/lib/apt/lists/*

# Install Vulkan runtime and OpenGL libraries
RUN apt-get update && apt-get install -y \
    libgl1 vulkan-tools \
    && rm -rf /var/lib/apt/lists/*

# Configure NVIDIA Vulkan ICD
RUN cat > /etc/vulkan/icd.d/nvidia_icd.json <<EOF
{
    "file_format_version" : "1.0.0",
    "ICD": {
        "library_path": "libGLX_nvidia.so.0",
        "api_version" : "1.3.194"
    }
}
EOF

# Configure NVIDIA EGL vendor
RUN mkdir -p /usr/share/glvnd/egl_vendor.d && \
    cat > /usr/share/glvnd/egl_vendor.d/10_nvidia.json <<EOF
{
    "file_format_version" : "1.0.0",
    "ICD" : {
        "library_path" : "libEGL_nvidia.so.0"
    }
}
EOF

# Configure NVIDIA Vulkan layers
RUN cat > /etc/vulkan/implicit_layer.d/nvidia_layers.json <<EOF
{
    "file_format_version" : "1.0.0",
    "layer": {
        "name": "VK_LAYER_NV_optimus",
        "type": "INSTANCE",
        "library_path": "libGLX_nvidia.so.0",
        "api_version" : "1.3.194",
        "implementation_version" : "1",
        "description" : "NVIDIA Optimus layer",
        "functions": {
            "vkGetInstanceProcAddr": "vk_optimusGetInstanceProcAddr",
            "vkGetDeviceProcAddr": "vk_optimusGetDeviceProcAddr"
        },
        "enable_environment": {
            "__NV_PRIME_RENDER_OFFLOAD": "1"
        },
        "disable_environment": {
            "DISABLE_LAYER_NV_OPTIMUS_1": ""
        }
    }
}
EOF

# Install build dependencies for Kolosal Server
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libcurl4-openssl-dev \
    libssl-dev \
    zlib1g-dev \
    wget \
    curl \
    ca-certificates \
    vulkan-validationlayers-dev \
    libvulkan-dev \
    && rm -rf /var/lib/apt/lists/*

# Create application directory
WORKDIR /app

# Copy source code
COPY . .

# Make scripts executable
RUN chmod +x install-linux-deps.sh build-linux.sh

# Build Kolosal Server with Vulkan support
RUN USE_VULKAN=ON BUILD_TYPE=Release ./build-linux.sh

# Create a non-root user for security
RUN useradd -m -u 1000 kolosal && \
    mkdir -p /app/models /app/logs && \
    chown -R kolosal:kolosal /app

# Copy and set up configuration
RUN cp config.example.yaml config.yaml && \
    chown kolosal:kolosal config.yaml

# Create models directory for downloaded models
VOLUME ["/app/models", "/app/logs"]

# Switch to non-root user
USER kolosal

# Expose the default port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# Set working directory to build output
WORKDIR /app/build

# Default command
CMD ["./kolosal-server", "--config", "../config.yaml"]
