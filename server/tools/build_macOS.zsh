#!/usr/bin/env zsh
set -euo pipefail

# ---- Project settings (edit once) ----
QT_DIR="$HOME/Qt/6.8.3/macos"
# -------------------------------------

script_dir="$(cd "$(dirname "$0")" && pwd)/.."
project_dir="$(pwd)"

PROJECT_NAME="$(basename "$(ls "$script_dir"/*.pro)" .pro)"

BUILD_HASH="$(git -C "$script_dir" rev-parse HEAD)"
BUILD_VERSION="$(<"$script_dir/Version.txt")"

out_dmg="${project_dir}/${PROJECT_NAME}_macOS_${BUILD_VERSION}.dmg"
build_dir="$HOME/Build"

rm -f "$out_dmg"

rm -rf "$build_dir"
mkdir -p "$build_dir"
cd "$build_dir"

"$QT_DIR/bin/qmake" "$project_dir/${PROJECT_NAME}.pro" CONFIG+=release
make -j"$(sysctl -n hw.ncpu)"
"$QT_DIR/bin/macdeployqt" "${PROJECT_NAME}.app" -dmg

mkdir -p "$project_dir/0_Builds"
cp -f "${PROJECT_NAME}.dmg" "$project_dir/0_Builds/${PROJECT_NAME}_macOS_${BUILD_VERSION}.dmg"
