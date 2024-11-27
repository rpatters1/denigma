#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

# Define paths
SCHEMA_URL="https://w3c.github.io/mnx/docs/mnx-schema.json"
OUTPUT_DIR="./generated"
SCHEMA_FILE="$OUTPUT_DIR/mnx-schema.json"
OUTPUT_FILE="$OUTPUT_DIR/mnx_model.h"

# Create output directory
mkdir -p $OUTPUT_DIR

# Download the latest schema
curl -o $SCHEMA_FILE $SCHEMA_URL

# Run Quicktype to generate C++ classes
quicktype --no-boost --namespace mnx \
	--src-lang schema \
	--src $SCHEMA_FILE \
	--lang c++ \
	--out $OUTPUT_FILE

echo "C++ classes generated at $OUTPUT_FILE"
