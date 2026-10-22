#!/usr/bin/env zsh
set -euo pipefail

# ---- Project settings (edit once) ----
QT_DIR="$HOME/Qt/6.8.3/macos"
# -------------------------------------

# tools/ -> project (server) directory, shared with the host; build locally on the VM.
script_dir="$(cd "$(dirname "$0")" && pwd)/.."
BUILD_VERSION="$(<"$script_dir/Version.txt")"
out_dmg="$script_dir/0_Builds/JIOServer_macOS_${BUILD_VERSION}.dmg"
build_dir="$HOME/Build"

# cmake and ninja ship with the Qt Tools but are not on PATH
export PATH="$HOME/Qt/Tools/Ninja:$PATH"
for _d in "$HOME"/Qt/Tools/CMake/CMake.app/Contents/bin "$HOME"/Qt/Tools/CMake/bin; do
  [[ -x "$_d/cmake" ]] && export PATH="$_d:$PATH" && break
done

rm -rf "$build_dir"
mkdir -p "$build_dir" "$script_dir/0_Builds"

if command -v ninja >/dev/null 2>&1; then GEN="Ninja"; else GEN="Unix Makefiles"; fi
cmake -G "$GEN" -S "$script_dir" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_DIR"
cmake --build "$build_dir" --parallel        # both JIOServer and JIOServerCLI

# the command line server rides inside the GUI bundle (shares its Qt frameworks), as the .pro build did
cp -f "$build_dir/JIOServerCLI.app/Contents/MacOS/JIOServerCLI" \
      "$build_dir/JIOServer.app/Contents/MacOS/JIOServerCLI"

"$QT_DIR/bin/macdeployqt" "$build_dir/JIOServer.app" \
  -executable="$build_dir/JIOServer.app/Contents/MacOS/JIOServerCLI" -dmg

cp -f "$build_dir/JIOServer.dmg" "$out_dmg"
echo "Package: $out_dmg"
