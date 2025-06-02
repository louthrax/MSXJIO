#!/bin/bash

# --- Step 1: Parse static-lib list from command-line ---
# Read and collect all leading `-lxxx` arguments from .pro
REPOSITION_LIBS=()
while [[ "$1" =~ ^-l ]]; do
  REPOSITION_LIBS+=("$1")
  shift
done

# Remaining args are the real linker command line
args=("$@")

# --- Step 2: Find last occurrence of each -lxxx to reposition ---
declare -A last_seen
for ((i=0; i<${#args[@]}; i++)); do
  arg="${args[$i]}"
  for lib in "${REPOSITION_LIBS[@]}"; do
    if [[ "$arg" == "$lib" ]]; then
      last_seen["$lib"]=$i
    fi
  done
done

# --- Step 3: Build the cleaned command line ---
KEEP=()
TO_APPEND=()
for ((i=0; i<${#args[@]}; i++)); do
  arg="${args[$i]}"
  skip=false
  for lib in "${REPOSITION_LIBS[@]}"; do
    if [[ "$arg" == "$lib" && $i -eq ${last_seen[$lib]} ]]; then
      TO_APPEND+=("$arg")  # store for re-adding at the end
      skip=true
      break
    fi
  done
  $skip || KEEP+=("$arg")
done

# --- Step 4: Append static block at the end ---
if [[ ${#REPOSITION_LIBS[@]} -gt 0 ]]; then
  KEEP+=("-Wl,-Bstatic")
  KEEP+=("${REPOSITION_LIBS[@]}")
  KEEP+=("-Wl,-Bdynamic")
fi

# --- Step 5: Run actual linker ---
echo ------------------------
echo exec g++ "${KEEP[@]}"
exec g++ "${KEEP[@]}"
