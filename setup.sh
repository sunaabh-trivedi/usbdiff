USBDIFF_DIR="$(cd "$(dirname "$0")" && pwd)"
USBDIFF_BIN="$USBDIFF_DIR/usbdiff"

if [ ! -f "$USBDIFF_BIN" ]; then
    echo "usbdiff executable not found at $USBDIFF_BIN. Run make in $USBDIFF_DIR."
    exit 1
fi

# Add to ~/.profile if not already present
if ! grep -q "$USBDIFF_DIR" ~/.profile; then
    echo "export PATH=\"\$PATH:$USBDIFF_DIR\"" >> ~/.profile
    echo "usbdiff directory added to PATH in ~/.profile"
    echo "Please restart your terminal or run: source ~/.profile"
else
    echo "usbdiff directory already in PATH"
fi