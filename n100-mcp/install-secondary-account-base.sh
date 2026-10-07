#!/usr/bin/env bash
set -euo pipefail
SOURCE_USER="${SOURCE_USER:-chatgpt-runner2}"
TARGET_USER="${TARGET_USER:-chat-runner1}"
TARGET_PORT="${TARGET_PORT:-8766}"
TARGET_HEALTH_PORT="${TARGET_HEALTH_PORT:-18081}"
SOURCE_HOME="/home/$SOURCE_USER"
TARGET_HOME="/home/$TARGET_USER"
if [ "$(id -u)" -ne 0 ]; then echo "Run with sudo" >&2; exit 2; fi
id "$TARGET_USER" >/dev/null 2>&1 || useradd -m -s /bin/bash "$TARGET_USER"
install -d -o "$TARGET_USER" -g "$TARGET_USER" "$TARGET_HOME/n100-mcp" "$TARGET_HOME/n100-mcp/helpers" "$TARGET_HOME/.local/bin" "$TARGET_HOME/.config/n100-mcp-tunnel" "$TARGET_HOME/.local/state/n100-mcp"
cp "$SOURCE_HOME/n100-mcp/server.mjs" "$SOURCE_HOME/n100-mcp/package.json" "$SOURCE_HOME/n100-mcp/package-lock.json" "$TARGET_HOME/n100-mcp/"
cp "$SOURCE_HOME/n100-mcp/helpers/media_archive.py" "$TARGET_HOME/n100-mcp/helpers/"
rm -rf "$TARGET_HOME/n100-mcp/node_modules"
cp -a "$SOURCE_HOME/n100-mcp/node_modules" "$TARGET_HOME/n100-mcp/node_modules"
install -m 0755 -o "$TARGET_USER" -g "$TARGET_USER" "$SOURCE_HOME/.local/bin/tunnel-client" "$TARGET_HOME/.local/bin/tunnel-client"
chown -R "$TARGET_USER:$TARGET_USER" "$TARGET_HOME/n100-mcp" "$TARGET_HOME/.local" "$TARGET_HOME/.config"
echo "Base files installed for $TARGET_USER. Use account-specific start/configure scripts with MCP port $TARGET_PORT and health port $TARGET_HEALTH_PORT."
