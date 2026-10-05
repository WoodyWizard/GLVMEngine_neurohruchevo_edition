#version 450
#extension GL_GOOGLE_include_directive : require
// Glass walls of the tank: Fresnel reflection of the sky with a faint green tint, alpha blended.
// Back faces are drawn before the fluid (they are refracted by it), front faces after it.

#include "scene_common.glsl"

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec3 localPosition;
layout(location = 3) flat in vec4 albedo;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 normal = normalize( worldNormal );
	const vec3 toEye = normalize( scene.cameraPosition.xyz - worldPosition );
	if ( dot( normal, toEye ) < 0.0 )
		normal = -normal;
	const float cosine = clamp( dot( normal, toEye ), 0.0, 1.0 );
	const float fresnel = 0.04 + 0.96 * pow( 1.0 - cosine, 5.0 );
	const vec3 reflection = skyColor( reflect( -toEye, normal ) );
	const vec3 halfVector = normalize( scene.sunDirection.xyz + toEye );
	const float highlight = pow( max( dot( normal, halfVector ), 0.0 ), 400.0 ) * scene.sunDirection.w * 12.0;
	const vec3 tint = vec3( 0.75, 0.9, 0.85 ) * albedo.rgb;
	const float alpha = clamp( 0.05 + fresnel * 0.8, 0.0, 0.9 );
	outColor = vec4( mix( tint * 0.15, reflection, fresnel / max( alpha, 1e-3 ) * 0.9 ) + scene.sunColor.rgb * highlight, alpha );
}
