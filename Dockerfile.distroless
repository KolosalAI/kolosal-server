# Ultra-minimal distroless runtime image
# Uses Google's distroless base for maximum security and minimal size
FROM gcr.io/distroless/cc-debian12:latest

# Set environment variables for NVIDIA runtime
ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=all

# Copy pre-built binary and configuration
COPY kolosal-server /app/
COPY config.yaml /app/

# Set working directory
WORKDIR /app

# Create volumes (directories will be created by Docker)
VOLUME ["/app/models", "/app/logs"]

# Expose port
EXPOSE 8080

# No health check in distroless (no curl available)
# Health checks should be handled by orchestrator

# Run as non-root (distroless runs as nobody by default)
USER 65532:65532

# Default command
ENTRYPOINT ["./kolosal-server"]
CMD ["--config", "config.yaml"]
