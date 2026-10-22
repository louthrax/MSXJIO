#!/usr/bin/env bash
set -euo pipefail

# ---- Project settings (edit once) ----
QT_VER="6.9.3"
QT_ANDROID="$HOME/bin/Qt/${QT_VER}/android_arm64_v8a"
QT_HOST="$HOME/bin/Qt/${QT_VER}/gcc_64"

ANDROID_SDK_ROOT="$HOME/Android/Sdk"
NDK_VER="27.2.12479018"
JAVA_HOME="$HOME/bin/Qt/jdk-23.0.2+7"
BUILD_TOOLS_VER="35.0.0"
PROJECT_NAME="JIOServer"

# APK signing: QT_ANDROID_KEYSTORE_PATH/ALIAS/STORE_PASS/KEY_PASS; the debug keystore by default.
# -------------------------------------

script_dir="$(cd "$(dirname "$0")" && pwd)"
BUILD_VERSION="$(<"$script_dir/Version.txt")"

export ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$NDK_VER"
export JAVA_HOME
export PATH="$JAVA_HOME/bin:$PATH"

if [[ -z "${QT_ANDROID_KEYSTORE_PATH:-}" ]]; then
  echo "Warning: signing with the debug keystore (set QT_ANDROID_KEYSTORE_* for a release key)" >&2
  export QT_ANDROID_KEYSTORE_PATH="$HOME/.android/debug.keystore"
  export QT_ANDROID_KEYSTORE_ALIAS="androiddebugkey"
  export QT_ANDROID_KEYSTORE_STORE_PASS="android"
  export QT_ANDROID_KEYSTORE_KEY_PASS="android"
fi
if [[ ! -f "$QT_ANDROID_KEYSTORE_PATH" ]]; then
  echo "Keystore not found: $QT_ANDROID_KEYSTORE_PATH" >&2
  echo "Create the debug keystore with:" >&2
  echo "  keytool -genkeypair -keystore ~/.android/debug.keystore -alias androiddebugkey -storepass android -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=Android Debug,O=Android,C=US'" >&2
  exit 1
fi

build_dir="/mnt/DataLinux/Tmp/${PROJECT_NAME}-Android-${QT_VER//./_}-arm64_v8a"
rm -rf "$build_dir"
mkdir -p "$build_dir" "$script_dir/0_Builds"

# Configure with the Android Qt kit (qt-cmake sets the NDK toolchain), then the apk target builds
# the (unsigned) APK with androiddeployqt.
"$QT_ANDROID/bin/qt-cmake" -G Ninja -S "$script_dir" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH="$QT_HOST" \
  -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
  -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT"

cmake --build "$build_dir" --target apk

rel="$build_dir/android-build/build/outputs/apk/release"
unsigned="$(ls -t "$rel"/*.apk | head -n1)"
signed="$rel/JIOServer-release-signed.apk"

APKSIGNER="$ANDROID_SDK_ROOT/build-tools/$BUILD_TOOLS_VER/apksigner"
"$APKSIGNER" sign \
  --ks "$QT_ANDROID_KEYSTORE_PATH" \
  --ks-key-alias "$QT_ANDROID_KEYSTORE_ALIAS" \
  --ks-pass "pass:$QT_ANDROID_KEYSTORE_STORE_PASS" \
  --key-pass "pass:$QT_ANDROID_KEYSTORE_KEY_PASS" \
  --out "$signed" "$unsigned"
"$APKSIGNER" verify "$signed"

cp -f "$signed" "$script_dir/0_Builds/${PROJECT_NAME}_Android_${BUILD_VERSION}.apk"
echo "Package: $script_dir/0_Builds/${PROJECT_NAME}_Android_${BUILD_VERSION}.apk"
