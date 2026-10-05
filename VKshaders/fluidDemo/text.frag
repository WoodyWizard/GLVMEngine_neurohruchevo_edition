#version 450
// The engine font atlas has black glyphs on a transparent background: the alpha is the coverage.

layout(set = 0, binding = 1) uniform sampler2D atlas;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4( color.rgb, color.a * texture( atlas, uv ).a );
}
