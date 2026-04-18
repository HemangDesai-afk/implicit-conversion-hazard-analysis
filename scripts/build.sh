#!/usr/bin/env bash
# Build script for the Implicit Conversion Hazard Analyzer
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

echo "=== Configuring build ==="
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "=== Building ==="
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "=== Build complete ==="
echo "Binary: ${BUILD_DIR}/implicit-conversion-hazard"
