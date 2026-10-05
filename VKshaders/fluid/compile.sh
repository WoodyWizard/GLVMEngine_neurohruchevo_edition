#!/bin/sh
# Compiles the fluid shaders to SPIR-V (needs glslangValidator from glslang or the Vulkan SDK).
# Run from this directory: ./compile.sh [path to glslangValidator]
GLSLANG=${1:-glslangValidator}
for shader in *.comp *.vert *.frag; do
	[ -f "$shader" ] || continue
	"$GLSLANG" -V --target-env vulkan1.3 -I. "$shader" -o "$shader.spv" || exit 1
done
