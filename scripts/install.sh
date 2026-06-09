#!/bin/sh
# Install agy-native-shim on Termux
# Usage: curl -sL https://github.com/adarshkr6238/agy-native-shim/raw/main/scripts/install.sh | sh

set -e

SHIM_URL="https://github.com/adarshkr6238/agy-native-shim/raw/main/bin/agy_native_shim.so"
LAUNCHER_URL="https://github.com/adarshkr6238/agy-native-shim/raw/main/scripts/agy.native"
DEST="$HOME/.local/lib/agy_native_shim.so"
AGY_BIN="$HOME/antigravity/bin/agy"
AGY_QEMU="$HOME/antigravity/bin/agy.qemu"
AGY_NATIVE="$HOME/antigravity/bin/agy.native"

echo "=== agy-native-shim installer ==="
echo ""

echo "[1/3] Downloading native shim..."
mkdir -p "$(dirname "$DEST")"
curl -sL "$SHIM_URL" -o "$DEST"
echo "  -> Installed to $DEST"

echo "[2/3] Installing native launcher..."
curl -sL "$LAUNCHER_URL" -o "$AGY_NATIVE"
chmod +x "$AGY_NATIVE"
echo "  -> Installed to $AGY_NATIVE"

echo "[3/3] Swapping launcher..."
if [ -f "$AGY_BIN" ] && [ ! -f "$AGY_QEMU" ]; then
    cp "$AGY_BIN" "$AGY_QEMU"
    echo "  -> Backed up QEMU launcher to $AGY_QEMU"
fi
cp "$AGY_NATIVE" "$AGY_BIN"
echo "  -> Native launcher activated"

echo ""
echo "=== Done! ==="
echo "Antigravity now runs WITHOUT QEMU emulation."
echo "Restart Antigravity to use native mode."
echo ""
echo "To revert: cp ~/antigravity/bin/agy.qemu ~/antigravity/bin/agy"
