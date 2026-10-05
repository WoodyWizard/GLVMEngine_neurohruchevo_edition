#version 450
#extension GL_GOOGLE_include_directive : require
// Opaque scene materials (procedural pool tiles, concrete, metal, wooden crates, beach ball) lit by the sun with a
// PCF shadow map, colored water shadows with caustics from the fluid light maps, and hemispheric sky light.

#include "scene_common.glsl"

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

/// Position in the sun maps: uv and depth.
vec3 lightCoordinates( vec3 position ) {
	const vec4 clip = scene.lightViewProjection * vec4( position, 1.0 );
	return vec3( clip.xy / clip.w * 0.5 + 0.5, clip.z / clip.w );
}

float sunShadow( vec3 light ) {
	if ( any( lessThan( light.xy, vec2( 0.0 ) ) ) || any( greaterThan( light.xy, vec2( 1.0 ) ) ) || light.z >= 1.0 )
		return 1.0;
	const float texel = scene.options.x;
	float lit = 0.0;
	for ( int y = -1; y <= 1; ++y )
		for ( int x = -1; x <= 1; ++x )
			lit += light.z - 0.0015 > texture( shadowMap, light.xy + vec2( x, y ) * texel ).r ? 0.0 : 1.0;
	return lit / 9.0;
}

/// Sunlight below the water: Beer-Lambert through the fluid thickness, focused by the surface (caustics).
vec3 waterLight( vec3 light ) {
	if ( scene.options.y < 0.5 || any( lessThan( light.xy, vec2( 0.0 ) ) ) || any( greaterThan( light.xy, vec2( 1.0 ) ) ) )
		return vec3( 1.0 );
	if ( light.z <= texture( lightFrontDepth, light.xy ).r + 0.002 )
		return vec3( 1.0 );                                                  // Above the water surface
	const float texel = scene.options.x * 2.0;
	const float thickness = texture( lightThickness, light.xy ).r;
	const float laplacian = texture( lightThickness, light.xy + vec2( texel, 0.0 ) ).r + texture( lightThickness, light.xy - vec2( texel, 0.0 ) ).r +
		texture( lightThickness, light.xy + vec2( 0.0, texel ) ).r + texture( lightThickness, light.xy - vec2( 0.0, texel ) ).r - 4.0 * thickness;
	const float caustics = clamp( 1.0 - laplacian * scene.waterAbsorption.w, 0.35, 3.0 );
	return exp( -scene.waterAbsorption.rgb * thickness ) * caustics;
}

void main() {
	const vec3 normal = normalize( worldNormal ) * (gl_FrontFacing ? 1.0 : -1.0);
	const vec3 toEye = normalize( scene.cameraPosition.xyz - worldPosition );
	const int material = int( albedo.w + 0.5 );
	vec3 color = albedo.rgb;
	float shininess = 32.0;
	float specularStrength = 0.15;

	if ( material == MATERIAL_FLOOR ) {
		const bool isPool = all( greaterThan( worldPosition.xz, scene.tankMin.xz - 0.001 ) ) && all( lessThan( worldPosition.xz, scene.tankMax.xz + 0.001 ) );
		if ( isPool ) {
			/// Glazed pool tiles with grout lines.
			const vec2 tile = worldPosition.xz / 0.1;
			const vec2 f = fract( tile );
			const float grout = smoothstep( 0.0, 0.06, min( min( f.x, 1.0 - f.x ), min( f.y, 1.0 - f.y ) ) );
			const vec3 tileColor = mix( vec3( 0.55, 0.85, 0.9 ), vec3( 0.35, 0.7, 0.85 ), hash( floor( tile ) ) * 0.6 );
			color = mix( vec3( 0.85, 0.87, 0.86 ), tileColor, grout );
			shininess = 120.0;
			specularStrength = 0.5 * grout;
		} else {
			/// Concrete slabs.
			const vec2 slab = worldPosition.xz / 0.75;
			const vec2 f = fract( slab );
			const float joint = smoothstep( 0.0, 0.01, min( min( f.x, 1.0 - f.x ), min( f.y, 1.0 - f.y ) ) );
			const float grain = valueNoise( worldPosition.xz * 18.0 ) * 0.5 + valueNoise( worldPosition.xz * 3.0 ) * 0.5;
			color = vec3( 0.42, 0.4, 0.37 ) * (0.8 + 0.3 * grain) * mix( 0.6, 1.0, joint ) * (0.9 + 0.2 * hash( floor( slab ) ));
			shininess = 8.0;
			specularStrength = 0.05;
		}
	} else if ( material == MATERIAL_METAL ) {
		color = albedo.rgb * (0.9 + 0.1 * valueNoise( localPosition.xy * 40.0 ));
		shininess = 64.0;
		specularStrength = 0.8;
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

	const vec3 light = lightCoordinates( worldPosition + normal * 0.004 );
	const vec3 sunVisibility = sunShadow( light ) * waterLight( light );
	const float diffuse = max( dot( normal, scene.sunDirection.xyz ), 0.0 );
	const vec3 halfVector = normalize( scene.sunDirection.xyz + toEye );
	const float specular = pow( max( dot( normal, halfVector ), 0.0 ), shininess ) * specularStrength * (shininess + 8.0) / 25.0;
	const vec3 ambient = mix( scene.groundColor.rgb * 0.6, mix( scene.skyHorizon.rgb, scene.skyZenith.rgb, 0.5 ), normal.y * 0.5 + 0.5 ) * 0.5;
	const vec3 sun = scene.sunColor.rgb * scene.sunDirection.w;
	vec3 shaded = color * (ambient + sun * diffuse * sunVisibility) + sun * specular * sunVisibility * step( 0.0, diffuse );

	/// Slight aerial perspective.
	const float distanceToEye = length( scene.cameraPosition.xyz - worldPosition );
	shaded = mix( shaded, skyColor( -toEye ) * 0.8, clamp( (distanceToEye - 6.0) / 30.0, 0.0, 0.6 ) );
	outColor = vec4( shaded, 1.0 );
}
