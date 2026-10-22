#!/bin/bash
set -euo pipefail

# Builds the Android graphical server (0_Build_Android.sh), installs it on the phone connected with adb (settings
# kept) and starts it.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PACKAGE="net.louthrax.jioserver"
ADB="${ADB:-$(command -v adb || echo "$HOME/Android/Sdk/platform-tools/adb")}"

# phone checked before the build (a few minutes)
if [[ "$("$ADB" get-state 2>/dev/null)" != "device" ]]; then
  echo "No phone found by adb (USB cable, USB debugging enabled and authorized ?)" >&2
  "$ADB" devices -l >&2
  exit 1
fi

"$SCRIPT_DIR/0_Build_Android.sh"

apk="$SCRIPT_DIR/0_Builds/JIOServer_Android_$(<"$SCRIPT_DIR/Version.txt").apk"

"$ADB" install -r "$apk"
"$ADB" shell am force-stop "$PACKAGE"
"$ADB" shell monkey -p "$PACKAGE" -c android.intent.category.LAUNCHER 1 > /dev/null 2>&1
echo "Installed and started on $("$ADB" shell getprop ro.product.model | tr -d '\r')"
