#!/usr/bin/env bash
set -euo pipefail

# JIOServer (graphical server): Android APK (arm64-v8a).
# Built with the Qt for Android of this machine, signed with the keystore of
# QT_ANDROID_KEYSTORE_PATH/ALIAS/STORE_PASS/KEY_PASS (the debug keystore by default).
# Usage: 0_Build_JIOServer_Android.sh

# the package name
NAME=JIOServer

# ---- Tools of this machine (edit once) ----
QT_VER="6.9.3"
QT_ANDROID="$HOME/bin/Qt/$QT_VER/android_arm64_v8a"
QT_HOST="$HOME/bin/Qt/$QT_VER/gcc_64"
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/27.2.12479018"
export JAVA_HOME="$HOME/bin/Qt/jdk-23.0.2+7"
APKSIGNER="$ANDROID_SDK_ROOT/build-tools/35.0.0/apksigner"
# -------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(<"$SCRIPT_DIR/Version.txt")"
export PATH="$JAVA_HOME/bin:$PATH"

if [[ -z ${QT_ANDROID_KEYSTORE_PATH:-} ]]; then
  echo "Warning: signing with the debug keystore (set QT_ANDROID_KEYSTORE_* for a release key)" >&2
  export QT_ANDROID_KEYSTORE_PATH="$HOME/.android/debug.keystore"
  export QT_ANDROID_KEYSTORE_ALIAS="androiddebugkey"
  export QT_ANDROID_KEYSTORE_STORE_PASS="android"
  export QT_ANDROID_KEYSTORE_KEY_PASS="android"
fi
if [[ ! -f $QT_ANDROID_KEYSTORE_PATH ]]; then
  echo "Keystore not found: $QT_ANDROID_KEYSTORE_PATH" >&2
  echo "Create the debug keystore with:" >&2
  echo "  keytool -genkeypair -keystore ~/.android/debug.keystore -alias androiddebugkey -storepass android -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=Android Debug,O=Android,C=US'" >&2
  exit 1
fi

PACKAGE="${NAME}_Android_${VERSION}"
BUILD_DIR="/mnt/DataLinux/Tmp/$PACKAGE"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" "$SCRIPT_DIR/0_Builds"

# Configure with the Android Qt kit (qt-cmake sets the NDK toolchain), then the apk target builds the
# (unsigned) APK with androiddeployqt.
"$QT_ANDROID/bin/qt-cmake" -G Ninja -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH="$QT_HOST" -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT"
cmake --build "$BUILD_DIR" --target apk

"$APKSIGNER" sign --ks "$QT_ANDROID_KEYSTORE_PATH" --ks-key-alias "$QT_ANDROID_KEYSTORE_ALIAS" \
  --ks-pass "pass:$QT_ANDROID_KEYSTORE_STORE_PASS" --key-pass "pass:$QT_ANDROID_KEYSTORE_KEY_PASS" \
  --out "$BUILD_DIR/$PACKAGE.apk" "$BUILD_DIR"/android-build/build/outputs/apk/release/*.apk
"$APKSIGNER" verify "$BUILD_DIR/$PACKAGE.apk"
cp -f "$BUILD_DIR/$PACKAGE.apk" "$SCRIPT_DIR/0_Builds/"
echo "Package: $SCRIPT_DIR/0_Builds/$PACKAGE.apk"
