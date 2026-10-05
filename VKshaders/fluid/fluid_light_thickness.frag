#version 450
#extension GL_GOOGLE_include_directive : require
// Fluid seen from the sun (orthographic): the summed thickness (additive) and the depth of the nearest fluid surface
// (MIN blending). The host attenuates sunlight by exp(-absorption * thickness) at points deeper than that surface,
// so the water casts colored, soft shadows instead of opaque ones.

#include "fluid_render_common.glsl"

layout(location = 0) in vec2 corner;
layout(location = 1) in vec3 viewCenter;
layout(location = 2) flat in uint particle;

layout(location = 0) out float outThickness;
layout(location = 1) out float outFrontDepth;

void main() {
	const float d2 = dot( corner, corner );
	if ( d2 > 1.0 )
		discard;
	const float radius = params.cameraPosition.w;
	const float halfChord = sqrt( 1.0 - d2 ) * radius;
	outThickness = 2.0 * halfChord * params.fluidParams.x;
	const vec4 front = params.lightProjection * vec4( viewCenter + vec3( corner * radius, halfChord ), 1.0 );
	outFrontDepth = front.z / front.w;
}
