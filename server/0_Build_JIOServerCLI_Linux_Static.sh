#!/usr/bin/env bash
set -euo pipefail

# JIOServerCLI (command line server): static Linux executable (zip).
# Built in the qt_static_builder image of the Dockers project (Alpine, static Qt, musl): runs on any
# x86_64 Linux. --arm64: for ARM64, in the qt_static_builder_arm64 image (emulated by qemu-user-static:
# much slower).
# Usage: 0_Build_JIOServerCLI_Linux_Static.sh [--arm64]

# the package name, the CMake target (and its executable), the project files added to the zip
NAME=JIOServerCLI
TARGET=JIOServerCLI
EXTRA_FILES=()

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"   # mounted at /src (CMake runs git there)
SRC_DIR="/src${SCRIPT_DIR#"$REPO_DIR"}"                        # the project, in the container
VERSION="$(<"$SCRIPT_DIR/Version.txt")"

IMAGE=qt_static_builder PLATFORM=linux/amd64 KIND=LinuxStatic
if [[ ${1:-} == --arm64 ]]; then
  IMAGE=qt_static_builder_arm64 PLATFORM=linux/arm64 KIND=LinuxStaticARM64
fi

PACKAGE="${NAME}_${KIND}_${VERSION}"
BUILD_DIR="/mnt/DataLinux/Tmp/$PACKAGE"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" "$SCRIPT_DIR/0_Builds"

docker run --rm --platform "$PLATFORM" -u "$(id -u):$(id -g)" \
  -e HOME=/tmp -e SRC_DIR="$SRC_DIR" -e TARGET="$TARGET" \
  --mount type=bind,src="$REPO_DIR",dst=/src \
  --mount type=bind,src="$BUILD_DIR",dst=/build \
  -w /build "$IMAGE" bash -lc '
    set -e
    cmake -G Ninja -S "$SRC_DIR" -B /build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/qt/install
    cmake --build /build --target "$TARGET"
    strip "$TARGET"
    upx "$TARGET"
  '

rm -f "$SCRIPT_DIR/0_Builds/$PACKAGE.zip"
(cd "$SCRIPT_DIR" && 7z a -tzip "0_Builds/$PACKAGE.zip" "$BUILD_DIR/$TARGET" "${EXTRA_FILES[@]}")
echo "Package: $SCRIPT_DIR/0_Builds/$PACKAGE.zip"
