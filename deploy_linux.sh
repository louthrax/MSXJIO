#!/bin/bash

APP="$1"
DEST="$2"

if [[ ! -f "$APP" ]]; then
    echo "ERROR: Binary not found: $APP"
    exit 1
fi

mkdir -p "$DEST"
cp "$APP" "$DEST/"

echo "→ Copying Qt 6 dependencies..."
ldd "$APP" | awk '{print $3}' | grep -E "^/.*libQt6.*\.so" | while read -r lib; do
    cp -v "$lib" "$DEST/"
done

echo "→ Copying platform plugin..."
mkdir -p "$DEST/platforms"
cp "$(qtpaths6 --plugin-dir)/platforms/libqxcb.so" "$DEST/platforms/"

echo "→ Creating zip..."
cd "$DEST" && rm -f "../$(basename "$APP")-linux.zip" && zip -r "../$(basename "$APP")-linux.zip" * && cd -

echo "✅ Deployment finished."
