#version 450
#extension GL_GOOGLE_include_directive : require
// Opaque parts of the tank: pool tiles, steel frame, painted piston, wooden crates, beach balls. Lit by the host light
// (Blinn-Phong) and ambient light; under the water the light is colored by the water and focused into caustics
// (fluid thickness map from the light, FluidRenderer::recordLightThickness).

#include "tank_common.glsl"

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec3 localPosition;
layout(location = 3) flat in vec4 albedo;

layout(location = 0) out vec4 outColor;

float hash( vec2 p ) {
	return fract( sin( dot( p, vec2( 127.1, 311.7 ) ) ) * 43758.5453 );
}

float valueNoise( vec2 p ) {
	const vec2 cell = floor( p );
	const vec2 f = fract( p );
	const vec2 u = f * f * (3.0 - 2.0 * f);
	return mix( mix( hash( cell ), hash( cell + vec2( 1.0, 0.0 ) ), u.x ),
				mix( hash( cell + vec2( 0.0, 1.0 ) ), hash( cell + vec2( 1.0, 1.0 ) ), u.x ), u.y );
}

/// Light under the water: Beer-Lambert through the fluid thickness, focused by the surface (caustics).
vec3 waterLight( vec3 position ) {
	if ( tank.options.y < 0.5 )
		return vec3( 1.0 );
	const vec4 clip = tank.lightViewProjection * vec4( position, 1.0 );
	const vec3 light = vec3( clip.xy / clip.w * 0.5 + 0.5, clip.z / clip.w );
	if ( any( lessThan( light.xy, vec2( 0.0 ) ) ) || any( greaterThan( light.xy, vec2( 1.0 ) ) ) )
		return vec3( 1.0 );
	if ( light.z <= texture( lightFrontDepth, light.xy ).r + 0.002 )
		return vec3( 1.0 );                                                  // Above the water surface
	const float texel = tank.options.x * 2.0;
	const float thickness = texture( lightThickness, light.xy ).r;
	const float laplacian = texture( lightThickness, light.xy + vec2( texel, 0.0 ) ).r + texture( lightThickness, light.xy - vec2( texel, 0.0 ) ).r +
		texture( lightThickness, light.xy + vec2( 0.0, texel ) ).r + texture( lightThickness, light.xy - vec2( 0.0, texel ) ).r - 4.0 * thickness;
	const float caustics = clamp( 1.0 - laplacian * tank.waterAbsorption.w, 0.35, 3.0 );
	return exp( -tank.waterAbsorption.rgb * thickness ) * caustics;
}

void main() {
	const vec3 toEye = normalize( tank.cameraPosition.xyz - worldPosition );
	vec3 normal = normalize( worldNormal );
	if ( dot( normal, toEye ) < 0.0 )
		normal = -normal;                                                    // Inner faces (no culling: hosts may mirror the view)
	const int material = int( albedo.w + 0.5 );
	vec3 color = albedo.rgb;
	float shininess = 32.0;
	float specularStrength = 0.15;

	if ( material == MATERIAL_TILES ) {
		/// Glazed pool tiles with grout lines.
		const vec2 tile = worldPosition.xz / 0.12;
		const vec2 f = fract( tile );
		const float grout = smoothstep( 0.0, 0.06, min( min( f.x, 1.0 - f.x ), min( f.y, 1.0 - f.y ) ) );
		const vec3 tileColor = mix( vec3( 0.55, 0.85, 0.9 ), vec3( 0.35, 0.7, 0.85 ), hash( floor( tile ) ) * 0.6 );
		color = mix( vec3( 0.85, 0.87, 0.86 ), tileColor, grout );
		shininess = 120.0;
		specularStrength = 0.5 * grout;
	} else if ( material == MATERIAL_STEEL ) {
		color = albedo.rgb * (0.9 + 0.1 * valueNoise( (localPosition.xy + localPosition.z) * 40.0 ));
		shininess = 64.0;
		specularStrength = 0.8;
	} else if ( material == MATERIAL_PAINT ) {
		shininess = 48.0;
		specularStrength = 0.35;
	} else if ( material == MATERIAL_WOOD ) {
		/// Crate: planks with grain, darker frame along the edges of the box.
		const vec3 a = abs( localPosition );
		const float edge = max( max( min( a.x, a.y ), min( a.y, a.z ) ), min( a.x, a.z ) );
		const float plank = floor( localPosition.y * 3.0 + 3.0 );
		const float grain = valueNoise( vec2( (localPosition.x + localPosition.z) * 3.0, localPosition.y * 40.0 + plank * 7.0 ) );
		color = albedo.rgb * (0.75 + 0.35 * grain) * (0.9 + 0.2 * hash( vec2( plank, albedo.r ) ));
		color = mix( color, albedo.rgb * 0.55, smoothstep( 0.8, 0.86, edge ) );
		shininess = 16.0;
		specularStrength = 0.08;
	} else if ( material == MATERIAL_BALL ) {
		/// Beach ball: colored segments around the vertical axis, white caps.
		const float segment = floor( (atan( localPosition.z, localPosition.x ) / 6.2831853 + 0.5) * 6.0 );
		const vec3 colors[3] = vec3[]( vec3( 0.9, 0.15, 0.1 ), vec3( 0.1, 0.35, 0.9 ), vec3( 1.0, 0.8, 0.1 ) );
		color = mod( segment, 2.0 ) < 0.5 ? colors[int( segment / 2.0 ) % 3] : vec3( 0.95 );
		color = mix( color, vec3( 0.95 ), smoothstep( 0.85, 0.9, abs( localPosition.y ) ) );
		shininess = 80.0;
		specularStrength = 0.6;
	}

	const vec3 lightDirection = tank.lightDirection.xyz;
	const vec3 light = tank.lightColor.rgb * tank.lightDirection.w * waterLight( worldPosition + normal * 0.004 );
	const float diffuse = max( dot( normal, lightDirection ), 0.0 );
	const vec3 halfVector = normalize( lightDirection + toEye );
	const float specular = pow( max( dot( normal, halfVector ), 0.0 ), shininess ) * specularStrength * (shininess + 8.0) / 25.0;
	const vec3 ambient = tank.ambientColor.rgb * (0.75 + 0.25 * normal.y);
	vec3 shaded = color * (ambient + light * diffuse) + light * specular * step( 0.0, diffuse );
	if ( material == MATERIAL_STEEL )
		shaded += tank.environmentColor.rgb * 0.15 * pow( 1.0 - max( dot( normal, toEye ), 0.0 ), 3.0 );
	outColor = vec4( shaded, 1.0 );
}
