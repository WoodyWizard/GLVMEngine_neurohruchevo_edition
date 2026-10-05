#version 450
#extension GL_GOOGLE_include_directive : require
// Additive thickness of the fluid along the view ray (chord of every particle sphere, scaled to the water volume),
// plus dye and foam weighted by the thickness. Rendered at half resolution. Pass 1 renders the sun view.

#include "fluid_render_common.glsl"

layout(set = 0, binding = 3, std430) readonly buffer Colors { vec4 colors[]; };
layout(set = 0, binding = 4) uniform sampler2D sceneDepth;

layout(push_constant) uniform Push { uint pass; } push;     // 0 camera, 1 sun (light thickness map)

layout(location = 0) in vec2 corner;
layout(location = 1) in vec3 viewCenter;
layout(location = 2) flat in uint particle;

layout(location = 0) out float outThickness;
layout(location = 1) out vec4 outDyeFoam;

void main() {
	const float d2 = dot( corner, corner );
	if ( d2 > 1.0 )
		discard;
	const float radius = params.cameraPosition.w;
	const float halfChord = sqrt( 1.0 - d2 ) * radius;
	if ( push.pass == 0u ) {
		/// Thickness buffer is half the resolution of the scene depth.
		const float scene = linearDepth( texelFetch( sceneDepth, ivec2( gl_FragCoord.xy * 2.0 ), 0 ).r );
		if ( -(viewCenter.z + halfChord) > scene )
			discard;
	}
	const float thickness = 2.0 * halfChord * params.fluidParams.x;
	const vec4 color = colors[particle];
	outThickness = thickness;
	outDyeFoam = vec4( color.rgb * thickness, color.w * thickness );
}
