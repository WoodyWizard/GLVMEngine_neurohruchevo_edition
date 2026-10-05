// Screen space fluid rendering (Green 2010, van der Laan et al. 2009): shared declarations.

#ifndef FLUID_RENDER_COMMON_GLSL
#define FLUID_RENDER_COMMON_GLSL

layout(std140, set = 0, binding = 0) uniform RenderParams {
	mat4 view;
	mat4 projection;
	mat4 inverseView;
	mat4 inverseProjection;
	mat4 lightView;             // Sun view of the light thickness map
	mat4 lightProjection;       // Orthographic
	vec4 cameraPosition;        // xyz camera, w particle render radius
	vec4 projectionParams;      // x A, y B of the depth: linear distance = B / (A + depth); z pixels per meter at 1 m, w far plane
	vec4 viewport;              // xy full resolution, zw 1 / full resolution
	vec4 sunDirection;          // xyz towards the sun, w intensity
	vec4 sunColor;              // rgb
	vec4 skyZenith;             // rgb
	vec4 skyHorizon;            // rgb
	vec4 groundColor;           // rgb
	vec4 absorption;            // rgb base absorption per meter, w dye absorption strength per meter
	vec4 scattering;            // rgb in-scattering strength, w refraction strength
	vec4 material;              // x Fresnel F0, y specular exponent, z foam strength, w smoothing radius (meters)
	vec4 particleParams;        // x spacing, y speed of the full color ramp, z render mode, w time
	vec4 fluidParams;           // x thickness scale (water volume / rendered sphere volume), y edge thickness, z normal smoothing, w exposure
} params;

const float NO_FLUID = 1e6;     // Cleared fluid depth

/// Linear distance along the view axis from a depth buffer value.
float linearDepth( float depth ) {
	return params.projectionParams.y / (params.projectionParams.x + depth);
}

/// View space position of a pixel (uv in [0, 1]) at a linear distance.
vec3 viewPosition( vec2 uv, float viewDistance ) {
	const vec4 clip = vec4( uv * 2.0 - 1.0, 0.5, 1.0 );
	vec4 ray = params.inverseProjection * clip;
	ray.xyz /= ray.w;
	return ray.xyz * (viewDistance / -ray.z);
}

/// Procedural sky: zenith / horizon gradient, darker ground, sun disk with a glow.
vec3 skyColor( vec3 direction ) {
	const float height = direction.y;
	vec3 color = height >= 0.0
		? mix( params.skyHorizon.rgb, params.skyZenith.rgb, pow( clamp( height, 0.0, 1.0 ), 0.55 ) )
		: mix( params.skyHorizon.rgb * 0.7, params.groundColor.rgb, clamp( -height * 3.0, 0.0, 1.0 ) );
	const float sun = max( dot( direction, params.sunDirection.xyz ), 0.0 );
	color += params.sunColor.rgb * (pow( sun, 2000.0 ) * 40.0 + pow( sun, 40.0 ) * 0.4);
	return color;
}

#endif
