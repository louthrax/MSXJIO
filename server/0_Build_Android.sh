#!/usr/bin/env bash
set -euo pipefail

# ---- Project settings (edit once) ----
QT_VER="6.9.3"
QT_ANDROID="$HOME/bin/Qt/${QT_VER}/android_arm64_v8a"
QT_HOST="$HOME/bin/Qt/${QT_VER}/gcc_64"

ANDROID_SDK_ROOT="$HOME/Android/Sdk"
NDK_VER="27.2.12479018"
ANDROID_PLATFORM="android-35"

JAVA_HOME="$HOME/bin/Qt/jdk-23.0.2+7"
BUILD_TOOLS_VER="35.0.0"

# APK signing (an unsigned APK cannot be installed) uses Qt's standard
# androiddeployqt variables: QT_ANDROID_KEYSTORE_PATH, QT_ANDROID_KEYSTORE_ALIAS,
# QT_ANDROID_KEYSTORE_STORE_PASS and QT_ANDROID_KEYSTORE_KEY_PASS.
# When they are not set, the Android debug keystore is used.
# -------------------------------------

script_dir="$(cd "$(dirname "$0")" && pwd)"
project_dir="$script_dir"   # <-- IMPORTANT: anchor to script folder

# Graphical server (JIOServerAll.pro: both servers, for Qt Creator)
pro_file="$script_dir/JIOServer.pro"
PROJECT_NAME="JIOServer"

BUILD_HASH="$(git -C "$script_dir" rev-parse HEAD)"
BUILD_VERSION="$(<"$script_dir/Version.txt")"

export ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$NDK_VER"
export JAVA_HOME
export PATH="$JAVA_HOME/bin:$PATH"

build_dir="/mnt/DataLinux/Tmp/${PROJECT_NAME}-Linux-Android_Qt_${QT_VER//./_}_Clang_arm64_v8a-Release"
android_build_dir="$build_dir/android-build"
deploy_json="$build_dir/android-${PROJECT_NAME}-deployment-settings.json"

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

rm -rf "$build_dir"
mkdir -p "$build_dir"
cd "$build_dir"

"$QT_ANDROID/bin/qmake" "$pro_file" -spec android-clang CONFIG+=qtquickcompiler
make -j"$(nproc)"
make INSTALL_ROOT="$android_build_dir" install

"$QT_HOST/bin/androiddeployqt" \
  --input "$deploy_json" \
  --output "$android_build_dir" \
  --android-platform "$ANDROID_PLATFORM" \
  --jdk "$JAVA_HOME" \
  --gradle --release --sign

mkdir -p "$project_dir/0_Builds"
apk="$(ls -t "$android_build_dir"/build/outputs/apk/release/*-signed.apk | head -n1)"
"$ANDROID_SDK_ROOT/build-tools/$BUILD_TOOLS_VER/apksigner" verify "$apk"
cp -f "$apk" "$project_dir/0_Builds/${PROJECT_NAME}_Android_${BUILD_VERSION}.apk"
