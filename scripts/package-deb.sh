#!/bin/bash
set -e

cmake -S . -B build-deb -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-deb --parallel "$(nproc)"
cmake --build build-deb --target package
