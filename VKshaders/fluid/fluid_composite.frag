#version 450
#extension GL_GOOGLE_include_directive : require
// Final fluid shading over the scene:
//  - normals from the smoothed depth (the smaller one sided difference, no bleeding over silhouettes),
//  - refraction of the scene behind (screen space offset along the normal, rejected where the scene is in front),
//  - Beer-Lambert absorption of the light through the fluid thickness, colored by the dye; in-scattering,
//  - Fresnel (Schlick) reflection of the sky and the sun, foam as a white diffuse layer,
//  - soft edges where the fluid is thin. Debug modes show depth, thickness and normals.

#include "fluid_render_common.glsl"

layout(set = 0, binding = 4) uniform sampler2D sceneDepth;
layout(set = 1, binding = 0) uniform sampler2D sceneColor;
layout(set = 1, binding = 1) uniform sampler2D fluidDepth;
layout(set = 1, binding = 2) uniform sampler2D thicknessMap;
layout(set = 1, binding = 3) uniform sampler2D dyeFoamMap;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

const int MODE_FINAL = 0, MODE_PARTICLES = 1, MODE_DEPTH = 2, MODE_THICKNESS = 3, MODE_NORMALS = 4;

float hash( vec3 p ) {
	return fract( sin( dot( p, vec3( 127.1, 311.7, 74.7 ) ) ) * 43758.5453 );
}

/// Trilinear value noise in [0, 1].
float valueNoise( vec3 p ) {
	const vec3 cell = floor( p );
	const vec3 f = fract( p );
	const vec3 u = f * f * (3.0 - 2.0 * f);
	const float bottom = mix( mix( hash( cell ), hash( cell + vec3( 1, 0, 0 ) ), u.x ),
							  mix( hash( cell + vec3( 0, 1, 0 ) ), hash( cell + vec3( 1, 1, 0 ) ), u.x ), u.y );
	const float top = mix( mix( hash( cell + vec3( 0, 0, 1 ) ), hash( cell + vec3( 1, 0, 1 ) ), u.x ),
						   mix( hash( cell + vec3( 0, 1, 1 ) ), hash( cell + vec3( 1, 1, 1 ) ), u.x ), u.y );
	return mix( bottom, top, u.z );
}

float fluidDistance( ivec2 pixel ) {
	return texelFetch( fluidDepth, clamp( pixel, ivec2( 0 ), ivec2( params.viewport.xy ) - 1 ), 0 ).r;
}

/// One sided difference with the smaller depth step: silhouettes don't tilt the normals.
vec3 differential( ivec2 pixel, ivec2 direction, vec3 center, float surfaceDistance ) {
	const float forward = fluidDistance( pixel + direction );
	const float backward = fluidDistance( pixel - direction );
	const vec2 texel = params.viewport.zw;
	const vec3 forwardPosition = viewPosition( uv + vec2( direction ) * texel, forward );
	const vec3 backwardPosition = viewPosition( uv - vec2( direction ) * texel, backward );
	const bool hasForward = forward < NO_FLUID, hasBackward = backward < NO_FLUID;
	if ( hasForward && (!hasBackward || abs( forward - surfaceDistance ) < abs( surfaceDistance - backward )) )
		return forwardPosition - center;
	if ( hasBackward )
		return center - backwardPosition;
	return vec3( 0.0 );
}

void main() {
	const ivec2 pixel = ivec2( gl_FragCoord.xy );
	const vec3 scene = texelFetch( sceneColor, pixel, 0 ).rgb;
	const float sceneDistance = linearDepth( texelFetch( sceneDepth, pixel, 0 ).r );
	const float surfaceDistance = texelFetch( fluidDepth, pixel, 0 ).r;
	const int mode = int( params.particleParams.z );
	const float thickness = texture( thicknessMap, uv ).r;

	if ( mode == MODE_THICKNESS ) {
		outColor = vec4( mix( scene * 0.3, vec3( 0.2, 0.6, 1.0 ), clamp( thickness * 4.0, 0.0, 1.0 ) ), 1.0 );
		return;
	}
	if ( surfaceDistance >= NO_FLUID || surfaceDistance > sceneDistance + 1e-3 || mode == MODE_PARTICLES ) {
		outColor = vec4( scene, 1.0 );
		return;
	}
	if ( mode == MODE_DEPTH ) {
		outColor = vec4( vec3( fract( surfaceDistance * 4.0 ) * 0.8 + 0.1 ), 1.0 );
		return;
	}

	/// Surface position and normal in view space, then world space.
	const vec3 position = viewPosition( uv, surfaceDistance );
	const vec3 dx = differential( pixel, ivec2( 1, 0 ), position, surfaceDistance );
	const vec3 dy = differential( pixel, ivec2( 0, 1 ), position, surfaceDistance );
	vec3 normalView = normalize( cross( dy, dx ) );
	if ( dot( dx, dx ) == 0.0 || dot( dy, dy ) == 0.0 || any( isnan( normalView ) ) )
		normalView = vec3( 0.0, 0.0, 1.0 );
	if ( normalView.z < 0.0 )
		normalView = -normalView;
	if ( mode == MODE_NORMALS ) {
		outColor = vec4( normalize( mat3( params.inverseView ) * normalView ) * 0.5 + 0.5, 1.0 );
		return;
	}
	const vec3 normal = normalize( mat3( params.inverseView ) * normalView );
	const vec3 worldPosition = (params.inverseView * vec4( position, 1.0 )).xyz;
	const vec3 toEye = normalize( params.cameraPosition.xyz - worldPosition );

	/// Dye and foam are averaged over the thickness.
	const vec4 dyeFoam = texture( dyeFoamMap, uv );
	const float safeThickness = max( thickness, 1e-5 );
	const vec3 dye = clamp( dyeFoam.rgb / safeThickness, 0.0, 1.0 );
	const float foam = clamp( dyeFoam.a / safeThickness * params.material.z, 0.0, 1.0 );

	/// Beer-Lambert: the dye color is what the fluid does not absorb.
	const vec3 extinction = params.absorption.rgb + params.absorption.w * (1.0 - dye);
	const vec3 transmittance = exp( -extinction * thickness );

	/// Refraction: the surface bends the view ray by about refraction * normal (the angle between the incident and the
	/// refracted ray), the scene behind is displaced by that angle times the distance from the surface to the scene, as
	/// behind a lens. Thin jets and drops distort the background strongly, like real water does.
	const float leverArm = clamp( sceneDistance - surfaceDistance, 0.0, 0.35 );
	/// View x and y map to screen u and v with the signs of the projection (Vulkan projections flip y or mirror the view).
	const vec2 screenAxes = sign( vec2( params.projection[0][0], params.projection[1][1] ) );
	const vec2 displacement = normalView.xy * screenAxes * params.scattering.w * leverArm;                          // Meters
	const vec2 offset = displacement * params.projectionParams.z / max( surfaceDistance, 0.05 ) * params.viewport.zw;
	vec2 refractedUv = clamp( uv + offset, vec2( 0.0 ), vec2( 1.0 ) );
	if ( linearDepth( texture( sceneDepth, refractedUv ).r ) < surfaceDistance )
		refractedUv = uv;                                                    // The displaced point is in front of the fluid
	const vec3 refracted = texture( sceneColor, refractedUv ).rgb;

	const vec3 ambient = mix( params.skyHorizon.rgb, params.skyZenith.rgb, 0.5 );
	const float sunLight = max( dot( normal, params.sunDirection.xyz ), 0.0 ) * params.sunDirection.w;
	const vec3 inScattering = params.scattering.rgb * dye * (ambient + params.sunColor.rgb * sunLight * 0.3) * (1.0 - transmittance);
	vec3 water = refracted * transmittance + inScattering;

	/// Reflection: Schlick Fresnel with the sky and a sharp sun highlight.
	const vec3 reflected = reflect( -toEye, normal );
	const float cosine = clamp( dot( normal, toEye ), 0.0, 1.0 );
	const float fresnel = params.material.x + (1.0 - params.material.x) * pow( 1.0 - cosine, 5.0 );
	const vec3 halfVector = normalize( params.sunDirection.xyz + toEye );
	const float specular = pow( max( dot( normal, halfVector ), 0.0 ), params.material.y ) * params.sunDirection.w * (params.material.y + 8.0) / 25.0;
	vec3 color = mix( water, skyColor( reflected ), fresnel ) + params.sunColor.rgb * specular * fresnel * 4.0;

	/// Foam: a white, diffusely lit layer of bubbles. Bubble noise in world space (it moves with the surface position, two
	/// octaves of about 1 and 0.4 particle spacings) and a threshold turn the averaged foam measure into patches of
	/// whitewater instead of a uniform white haze.
	const float spacing = params.particleParams.x;
	const float bubbles = valueNoise( worldPosition / spacing ) * 0.6 + valueNoise( worldPosition / (spacing * 0.4) + 17.0 ) * 0.4;
	const float foamCover = smoothstep( 0.3, 0.7, foam * (0.5 + bubbles) );
	const vec3 foamColor = vec3( 0.92, 0.95, 0.97 ) * (ambient * 0.8 + params.sunColor.rgb * (0.35 + 0.65 * sunLight));
	color = mix( color, foamColor, foamCover );

	const float edge = smoothstep( 0.0, params.fluidParams.y, thickness );
	outColor = vec4( mix( scene, color, edge ), 1.0 );
}
