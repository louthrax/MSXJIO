#!/usr/bin/env bash
set -euo pipefail

# All the builds of the servers (JIOServer and JIOServerCLI), one after another: a failure does not stop the next builds; the failed ones are listed
# at the end. Packages in 0_Builds.
# Usage: 0_Build_All.sh

cd "$(dirname "$0")"

BUILDS=(
  "0_Build_JIOServer_Linux_Static.sh"
  "0_Build_JIOServer_Linux_Static.sh --arm64"
  "0_Build_JIOServer_Linux_AppImage.sh"
  "0_Build_JIOServer_Linux_AppImage.sh --arm64"
  "0_Build_JIOServer_macOS.sh"
  "0_Build_JIOServer_Windows.sh"
  "0_Build_JIOServer_Android.sh"
  "0_Build_JIOServerCLI_Linux_Static.sh"
  "0_Build_JIOServerCLI_Linux_Static.sh --arm64"
  "0_Build_JIOServerCLI_macOS.sh"
  "0_Build_JIOServerCLI_Windows.sh"
)

failed=()
for build in "${BUILDS[@]}"; do
  printf '\n========================================\n==> %s\n========================================\n' "$build"
  ./$build || failed+=("$build")
done

echo
if (( ${#failed[@]} == 0 )); then
  echo "==> All builds OK"
else
  printf '==> FAILED: %s\n' "${failed[@]}"
  exit 1
fi
