# Public Network Access Configuration

This guide explains how to configure Kolosal Server to be accessible from other devices on your local network.

## Overview

By default, Kolosal Server is configured for security and only accepts connections from the local machine (localhost). To allow access from other devices on your network, you need to enable public access.

## Quick Start

### Option 1: Command Line Flag
```bash
# Enable public access
./kolosal-server --public

# Or use the longer form
./kolosal-server --allow-public-access
```

### Option 2: Configuration File
Add to your config.yaml:
```yaml
server:
  host: "0.0.0.0"                    # Bind to all network interfaces
  port: "8080"                       # Your desired port
  allow_public_access: true          # Enable public access
```

## Security Considerations

⚠️ **Important Security Notes:**

1. **Enable Authentication**: When allowing public access, always enable authentication:
   ```yaml
   auth:
     enabled: true
     require_api_key: true
     api_keys:
       - "your-secure-api-key-here"
   ```

2. **Configure Rate Limiting**: Protect against abuse with rate limiting:
   ```yaml
   auth:
     rate_limit:
       enabled: true
       max_requests: 100
       window_size: 60
   ```

3. **Firewall Configuration**: Ensure your firewall allows connections on the specified port
4. **Use Strong API Keys**: Generate cryptographically secure API keys
5. **Monitor Access**: Enable access logging to monitor connections

## Network Configuration

### Finding Your IP Address

To access the server from other devices, you'll need your machine's IP address:

**Windows:**
```cmd
ipconfig
```
Look for "IPv4 Address" under your active network adapter.

**Linux/Mac:**
```bash
ip addr show
# or
ifconfig
```

### Accessing from Other Devices

Once the server is running with public access enabled, other devices can connect using:
```
http://YOUR_IP_ADDRESS:PORT
```

For example:
- `http://192.168.1.100:8080/health`
- `http://192.168.1.100:8080/v1/chat/completions`

## Configuration Examples

### Development Setup (Less Secure)
```yaml
server:
  host: "0.0.0.0"
  port: "8080"
  allow_public_access: true

auth:
  enabled: false  # Not recommended for production
```

### Production Setup (Recommended)
```yaml
server:
  host: "0.0.0.0"
  port: "8080"
  allow_public_access: true

auth:
  enabled: true
  require_api_key: true
  api_key_header: "X-API-Key"
  api_keys:
    - "sk-1234567890abcdef1234567890abcdef"
  rate_limit:
    enabled: true
    max_requests: 100
    window_size: 60
  cors:
    enabled: true
    allowed_origins:
      - "http://localhost:3000"      # Your frontend app
      - "http://192.168.1.50:3000"   # Remote frontend
    allowed_methods:
      - "GET"
      - "POST"
      - "OPTIONS"
```

## Troubleshooting

### Common Issues

1. **Connection Refused**
   - Check if `allow_public_access` is set to `true`
   - Verify the server is binding to `0.0.0.0` and not `127.0.0.1`
   - Check firewall settings

2. **Server Only Accessible Locally**
   - Ensure `host` is set to `0.0.0.0` (not `127.0.0.1`)
   - Verify `allow_public_access: true` in config

3. **Firewall Blocking Connections**
   - Windows: Add rule in Windows Defender Firewall
   - Linux: Use `ufw allow PORT` or `iptables`
   - Mac: Check System Preferences > Security & Privacy > Firewall

### Testing Connectivity

From another device on the network:
```bash
# Test if the port is reachable
telnet YOUR_IP_ADDRESS PORT

# Test the health endpoint
curl http://YOUR_IP_ADDRESS:PORT/health
```

## Command Line Reference

### Enable Public Access
```bash
--public                      # Enable public access (short form)
--allow-public-access         # Enable public access (long form)
--no-public                   # Disable public access (short form)  
--disable-public-access       # Disable public access (long form)
```

### Example Commands
```bash
# Start server with public access and authentication
./kolosal-server --public --require-api-key --api-key "your-key-here"

# Start with config file that has public access enabled
./kolosal-server --config production-config.yaml

# Start on specific host and port with public access
./kolosal-server --host 0.0.0.0 --port 3000 --public
```

## Best Practices

1. **Always Use HTTPS in Production**: Consider putting a reverse proxy (nginx) in front for SSL termination
2. **Limit Network Scope**: Use router configuration to limit which networks can access the server
3. **Monitor Logs**: Enable access logging to monitor who's connecting
4. **Regular Updates**: Keep the server updated with security patches
5. **Backup Configuration**: Save your working configuration files
