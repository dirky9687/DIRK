#!/usr/bin/env bash
set -euo pipefail

DEST="${1:-$HOME/Projects/meshtastic-adv}"

if ! command -v git >/dev/null 2>&1; then
  echo "git is required" >&2
  exit 1
fi
if ! command -v pio >/dev/null 2>&1; then
  echo "PlatformIO (pio) is required" >&2
  exit 1
fi

if [ ! -d "$DEST/.git" ]; then
  git clone --recursive https://github.com/anton-vinogradov/meshtastic-adv "$DEST"
else
  git -C "$DEST" pull --ff-only
  git -C "$DEST" submodule update --init --recursive
fi

cd "$DEST"
./scripts/sync-overlay.sh
pio run -e m5stack-cardputer-adv-advui

echo
echo "Meshtastic ADV build complete."
echo "Project: $DEST"
echo "Use the upstream install/flash instructions for its merged/factory image."
