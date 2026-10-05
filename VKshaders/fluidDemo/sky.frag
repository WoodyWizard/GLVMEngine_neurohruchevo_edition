#version 450
#extension GL_GOOGLE_include_directive : require
// Background: the procedural sky along the view ray of every pixel.

#include "scene_common.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
	const vec4 far = scene.inverseViewProjection * vec4( uv * 2.0 - 1.0, 1.0, 1.0 );
	const vec3 direction = normalize( far.xyz / far.w - scene.cameraPosition.xyz );
	outColor = vec4( skyColor( direction ), 1.0 );
}
