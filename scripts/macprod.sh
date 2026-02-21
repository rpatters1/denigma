#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset

# Change to the parent directory of the script
cd "$(dirname "$0")/.."

BUILD_DIR="build-release"
rm -rf "$BUILD_DIR"

# Configure the project with Ninja for Release
echo "Configuring the project with Ninja..."
cmake -G Ninja -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

# Build the project
echo "Building the project..."
cmake --build "$BUILD_DIR"

echo "Build completed successfully."
