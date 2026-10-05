#version 450
#extension GL_GOOGLE_include_directive : require
// Debug view: shaded particle spheres colored by speed (blue - slow, white - fast) or by dye.

#include "fluid_render_common.glsl"

layout(std430, set = 0, binding = 2) readonly buffer Velocities { vec4 velocities[]; };
layout(std430, set = 0, binding = 3) readonly buffer Colors     { vec4 colors[]; };
layout(set = 0, binding = 4) uniform sampler2D sceneDepth;

layout(location = 0) in vec2 corner;
layout(location = 1) in vec3 viewCenter;
layout(location = 2) flat in uint particle;

layout(location = 0) out vec4 outColor;

void main() {
	const float d2 = dot( corner, corner );
	if ( d2 > 1.0 )
		discard;
	const float radius = params.cameraPosition.w * 0.6;
	if ( d2 > 0.36 )
		discard;
	const vec3 normal = vec3( corner / 0.6, sqrt( max( 1.0 - d2 / 0.36, 0.0 ) ) );
	const vec3 surface = viewCenter + normal * radius;
	if ( -surface.z > linearDepth( texelFetch( sceneDepth, ivec2( gl_FragCoord.xy ), 0 ).r ) )
		discard;
	const vec4 clip = params.projection * vec4( surface, 1.0 );
	gl_FragDepth = clip.z / clip.w;

	const float speed = length( velocities[particle].xyz ) / params.particleParams.y;
	const vec3 slow = vec3( 0.05, 0.25, 0.9 ), medium = vec3( 0.1, 0.8, 0.9 ), fast = vec3( 1.0 );
	vec3 albedo = speed < 0.5 ? mix( slow, medium, speed * 2.0 ) : mix( medium, fast, clamp( speed * 2.0 - 1.0, 0.0, 1.0 ) );
	albedo = mix( albedo, vec3( 1.0 ), colors[particle].w );
	const vec3 sunView = normalize( mat3( params.view ) * params.sunDirection.xyz );
	const float diffuse = max( dot( normal, sunView ), 0.0 );
	outColor = vec4( albedo * (0.25 + diffuse * params.sunDirection.w * 0.25) , 1.0 );
}
