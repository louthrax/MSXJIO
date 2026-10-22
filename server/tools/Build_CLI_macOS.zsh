#!/usr/bin/env zsh
set -euo pipefail

# Command line server: a zip with an application bundle (macdeployqt only deploys bundles) and a
# JIOServerCLI symlink to its executable, to run from a terminal.

# ---- Project settings (edit once) ----
QT_DIR="$HOME/Qt/6.8.3/macos"
# -------------------------------------

script_dir="$(cd "$(dirname "$0")" && pwd)/.."
BUILD_VERSION="$(<"$script_dir/Version.txt")"
package_name="JIOServerCLI_macOS_${BUILD_VERSION}"
build_dir="$HOME/BuildCLI"

export PATH="$HOME/Qt/Tools/Ninja:$PATH"
for _d in "$HOME"/Qt/Tools/CMake/CMake.app/Contents/bin "$HOME"/Qt/Tools/CMake/bin; do
  [[ -x "$_d/cmake" ]] && export PATH="$_d:$PATH" && break
done

rm -rf "$build_dir"
mkdir -p "$build_dir" "$script_dir/0_Builds"

if command -v ninja >/dev/null 2>&1; then GEN="Ninja"; else GEN="Unix Makefiles"; fi
cmake -G "$GEN" -S "$script_dir" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_DIR"
cmake --build "$build_dir" --target JIOServerCLI --parallel

"$QT_DIR/bin/macdeployqt" "$build_dir/JIOServerCLI.app" -no-plugins   # no GUI plugins (QtGui, QtWidgets...)

cd "$build_dir"
rm -rf "$package_name"
mkdir "$package_name"
mv JIOServerCLI.app "$package_name/"
ln -s "JIOServerCLI.app/Contents/MacOS/JIOServerCLI" "$package_name/JIOServerCLI"
"$package_name/JIOServerCLI" --version

rm -f "$script_dir/0_Builds/${package_name}.zip"
ditto -c -k --keepParent "$package_name" "$script_dir/0_Builds/${package_name}.zip"
echo "Package: $script_dir/0_Builds/${package_name}.zip"
