#version 450
// Text overlay: one quad per glyph of the engine font atlas (12 x 12 cells of 7 x 11 pixels in an 84 x 132 texture).

layout(std430, set = 0, binding = 0) readonly buffer Glyphs { vec4 glyphs[]; };   // x, y (pixels), scale, glyph; color
layout(push_constant) uniform Push { vec2 screenSize; } push;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 color;

const vec2 CORNERS[6] = vec2[]( vec2( 0.0, 0.0 ), vec2( 1.0, 0.0 ), vec2( 1.0, 1.0 ), vec2( 0.0, 0.0 ), vec2( 1.0, 1.0 ), vec2( 0.0, 1.0 ) );

void main() {
	const vec4 glyph = glyphs[gl_InstanceIndex * 2];
	color = glyphs[gl_InstanceIndex * 2 + 1];
	const vec2 corner = CORNERS[gl_VertexIndex];
	const vec2 size = vec2( 7.0, 11.0 ) * glyph.z;
	const vec2 pixel = glyph.xy + corner * size;
	gl_Position = vec4( pixel / push.screenSize * 2.0 - 1.0, 0.0, 1.0 );

	const float index = glyph.w;
	const vec2 cell = vec2( mod( index, 12.0 ), floor( index / 12.0 ) );
	uv = (cell + corner) * vec2( 1.0 / 12.0, 11.0 / 132.0 );
}
