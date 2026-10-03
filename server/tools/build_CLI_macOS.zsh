#!/usr/bin/env zsh
set -euo pipefail

# Command line server (JIOServerCLI.pro): zip archive with an application bundle (for its Qt frameworks,
# macdeployqt only deploys bundles) and a JIOServerCLI link to its executable, to run from a terminal

# ---- Project settings (edit once) ----
QT_DIR="$HOME/Qt/6.8.3/macos"
# -------------------------------------

script_dir="$(cd "$(dirname "$0")" && pwd)/.."
project_dir="$(pwd)"

PROJECT_NAME="JIOServerCLI"

BUILD_VERSION="$(<"$script_dir/Version.txt")"

package_name="${PROJECT_NAME}_macOS_${BUILD_VERSION}"
build_dir="$HOME/BuildCLI"

rm -rf "$build_dir"
mkdir -p "$build_dir"
cd "$build_dir"

"$QT_DIR/bin/qmake" "$project_dir/${PROJECT_NAME}.pro" CONFIG+=release -after "CONFIG+=app_bundle"
make -j"$(sysctl -n hw.ncpu)"
"$QT_DIR/bin/macdeployqt" "${PROJECT_NAME}.app" -no-plugins      # no GUI plugins (they bring QtGui, QtWidgets...)

mkdir "$package_name"
mv "${PROJECT_NAME}.app" "$package_name/"
ln -s "${PROJECT_NAME}.app/Contents/MacOS/${PROJECT_NAME}" "$package_name/${PROJECT_NAME}"
"$package_name/${PROJECT_NAME}" --version

mkdir -p "$project_dir/0_Builds"
rm -f "$project_dir/0_Builds/${package_name}.zip"
ditto -c -k --keepParent "$package_name" "$project_dir/0_Builds/${package_name}.zip"
