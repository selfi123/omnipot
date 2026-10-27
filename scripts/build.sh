#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "Building PotPlayer Native Linux in $PROJECT_ROOT..."
cd "$PROJECT_ROOT"

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

echo "Running automated test suite..."
ctest --test-dir build --output-on-failure

echo "Build and tests completed successfully!"
echo "Binary available at: $PROJECT_ROOT/build/potplayer-linux"
