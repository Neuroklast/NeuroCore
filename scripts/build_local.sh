#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${NEUROKORE_BUILD_DIR:-$repo_dir/build}"
config="${NEUROKORE_BUILD_CONFIG:-Release}"
jobs="${NEUROKORE_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

node "$repo_dir/scripts/sync_living_docs.mjs"

cmake_args=(-S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE="$config")
if [[ -n "${JUCE_DIR:-}" ]]; then
  cmake_args+=("-DJUCE_DIR=$JUCE_DIR")
fi

cmake "${cmake_args[@]}"
cmake --build "$build_dir" --config "$config" --target NeuroKore_All --parallel "$jobs"

label="$(sed -n 's/^set(NEUROKORE_VERSION_LABEL "\(.*\)")/\1/p' "$repo_dir/CMakeLists.txt" | head -n 1)"
if [[ -z "$label" ]]; then
  echo "NEUROKORE_VERSION_LABEL missing from CMakeLists.txt" >&2
  exit 1
fi
printf 'NEUROKORE %s build complete: %s/NeuroKore_artefacts/%s\n' "$label" "$build_dir" "$config"
