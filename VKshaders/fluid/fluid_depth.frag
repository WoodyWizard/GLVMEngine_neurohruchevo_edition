#version 450
#extension GL_GOOGLE_include_directive : require
// Sphere impostor: the front surface of the particle sphere into the fluid depth (linear distance) and the depth buffer.
// Fragments behind the opaque scene are discarded.

#include "fluid_render_common.glsl"

layout(set = 0, binding = 4) uniform sampler2D sceneDepth;

layout(location = 0) in vec2 corner;
layout(location = 1) in vec3 viewCenter;
layout(location = 2) flat in uint particle;

layout(location = 0) out float outDistance;

void main() {
	const float d2 = dot( corner, corner );
	if ( d2 > 1.0 )
		discard;
	const float radius = params.cameraPosition.w;
	const vec3 surface = viewCenter + vec3( corner * radius, sqrt( 1.0 - d2 ) * radius );
	if ( -surface.z > linearDepth( texelFetch( sceneDepth, ivec2( gl_FragCoord.xy ), 0 ).r ) )
		discard;
	const vec4 clip = params.projection * vec4( surface, 1.0 );
	gl_FragDepth = clip.z / clip.w;
	outDistance = -surface.z;
}
