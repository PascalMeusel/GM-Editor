#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
build_dir="${repo_root}/build-linux"

# Ubuntu/Debian packages: build-essential cmake libgl1-mesa-dev xorg-dev libpng-dev
cmake -S "${repo_root}" -B "${build_dir}" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug "$@"
cmake --build "${build_dir}" --parallel
