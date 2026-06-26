#!/bin/bash
set -e

echo "Building NexKey..."
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build --parallel "$(nproc)"

echo "Build complete."
echo "CLI: build/nexkey-cli/nexkey"
echo "Running tests..."
ctest --test-dir build --output-on-failure
