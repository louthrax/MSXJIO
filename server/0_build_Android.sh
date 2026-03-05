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
# -------------------------------------

script_dir="$(cd "$(dirname "$0")" && pwd)"
project_dir="$script_dir"   # <-- IMPORTANT: anchor to script folder

# Find the single .pro file in script_dir
pro_file="$(ls "$script_dir"/*.pro)"
PROJECT_NAME="$(basename "$pro_file" .pro)"

BUILD_HASH="$(git -C "$script_dir" rev-parse HEAD)"
BUILD_VERSION="$(<"$script_dir/Version.txt")"

export ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$NDK_VER"
export JAVA_HOME
export PATH="$JAVA_HOME/bin:$PATH"

build_dir="/mnt/DataLinux/Tmp/${PROJECT_NAME}-Linux-Android_Qt_${QT_VER//./_}_Clang_arm64_v8a-Release"
android_build_dir="$build_dir/android-build"
deploy_json="$build_dir/android-${PROJECT_NAME}-deployment-settings.json"

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
  --gradle --release

mkdir -p "$project_dir/0_Builds"
apk="$(ls -t "$android_build_dir"/build/outputs/apk/release/*.apk | head -n1)"
cp -f "$apk" "$project_dir/0_Builds/${PROJECT_NAME}_Android_${BUILD_VERSION}.apk"
