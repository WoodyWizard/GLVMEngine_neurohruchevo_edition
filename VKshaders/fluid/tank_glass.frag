#version 450
#extension GL_GOOGLE_include_directive : require
// Glass walls of the tank: Fresnel reflection of the surroundings with a faint green tint, alpha blended.
// The far walls (facing away from the camera) are drawn before the fluid, it refracts them; the near walls after it.
// Faces are told apart by the outward normal, not by the winding: hosts may use mirrored projections.

#include "tank_common.glsl"

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec3 localPosition;
layout(location = 3) flat in vec4 albedo;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 normal = normalize( worldNormal );
	const vec3 toEye = normalize( tank.cameraPosition.xyz - worldPosition );
	const bool isNear = dot( normal, toEye ) > 0.0;
	if ( isNear != ((push.flags & FLAG_NEAR_GLASS) != 0u) )
		discard;
	if ( !isNear )
		normal = -normal;
	const float cosine = clamp( dot( normal, toEye ), 0.0, 1.0 );
	const float fresnel = 0.04 + 0.96 * pow( 1.0 - cosine, 5.0 );
	const vec3 halfVector = normalize( tank.lightDirection.xyz + toEye );
	const float highlight = pow( max( dot( normal, halfVector ), 0.0 ), 400.0 ) * tank.lightDirection.w * 6.0;
	const vec3 tint = vec3( 0.75, 0.9, 0.85 ) * albedo.rgb;
	const float alpha = clamp( 0.06 + fresnel * 0.8, 0.0, 0.9 );
	const vec3 reflection = tank.environmentColor.rgb * (0.8 + 0.4 * normal.y);
	outColor = vec4( mix( tint * tank.ambientColor.rgb, reflection, fresnel / max( alpha, 1e-3 ) * 0.9 ) + tank.lightColor.rgb * highlight, alpha );
}
