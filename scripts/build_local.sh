#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${NEUROKORE_BUILD_DIR:-$repo_dir/build}"
config="${NEUROKORE_BUILD_CONFIG:-Release}"
jobs="${NEUROKORE_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

cmake_args=(-S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE="$config")
if [[ -n "${JUCE_DIR:-}" ]]; then
  cmake_args+=("-DJUCE_DIR=$JUCE_DIR")
fi

cmake "${cmake_args[@]}"
cmake --build "$build_dir" --config "$config" --target NeuroKore_All --parallel "$jobs"

printf 'NEUROKORE %s build complete: %s/NeuroKore_artefacts/%s\n' "0.6.4-beta" "$build_dir" "$config"
