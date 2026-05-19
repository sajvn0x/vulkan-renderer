#!/usr/bin/env bash

set -e

SHADER_DIR="assets/shaders"
OUTPUT_DIR="build/assets/shaders"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

find "$SHADER_DIR" -type f | while read -r shader; do
    rel_path="${shader#$SHADER_DIR/}"
    output_path="$OUTPUT_DIR/${rel_path}.spv"

    mkdir -p "$(dirname "$output_path")"

    echo "Compiling: $shader -> $output_path"

    glslangValidator -V "$shader" -o "$output_path"

done

echo "Shader compilation completed."

