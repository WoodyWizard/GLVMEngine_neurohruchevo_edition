// Fluid demo scene: shared declarations.

#ifndef SCENE_COMMON_GLSL
#define SCENE_COMMON_GLSL

struct Body {
	vec4 positionType;       // xyz center, w type: 0 inactive, 1 sphere, 2 box
	vec4 rotation;           // quaternion xyzw
	vec4 halfExtents;        // xyz box half extents (x = radius for spheres), w bounding radius
	vec4 velocityInvMass;
	vec4 angularVelocity;
	vec4 inverseInertia;
	vec4 color;              // rgb albedo, w material
};

layout(std140, set = 0, binding = 0) uniform SceneParams {
	mat4 viewProjection;
	mat4 inverseViewProjection;
	mat4 lightViewProjection;    // Sun: shadow map and fluid light maps
	vec4 cameraPosition;         // xyz, w time
	vec4 sunDirection;           // xyz towards the sun, w intensity
	vec4 sunColor;
	vec4 skyZenith;
	vec4 skyHorizon;
	vec4 groundColor;
	vec4 tankMin;                // Inner box of the glass tank
	vec4 tankMax;
	vec4 waterAbsorption;        // rgb per meter, w caustics strength
	vec4 options;                // x shadow texel size, y water shadows enabled, z exposure, w unused
} scene;

layout(std430, set = 0, binding = 1) readonly buffer Vertices { vec4 vertices[]; };   // position, normal per vertex
layout(std430, set = 0, binding = 2) readonly buffer Bodies   { Body bodies[]; };
layout(set = 0, binding = 3) uniform sampler2D shadowMap;
layout(set = 0, binding = 4) uniform sampler2D lightThickness;
layout(set = 0, binding = 5) uniform sampler2D lightFrontDepth;

layout(push_constant) uniform Push {
	mat4 model;
	vec4 color;                  // rgb albedo, w material
	uint flags;                  // 1: instances are bodies of bodyType, 2: shadow pass
	uint bodyType;
} push;

const uint FLAG_BODIES = 1u, FLAG_SHADOW = 2u;
const int MATERIAL_FLOOR = 0, MATERIAL_GLASS = 1, MATERIAL_METAL = 2, MATERIAL_WOOD = 3, MATERIAL_BALL = 4, MATERIAL_PLASTIC = 5;

vec3 quaternionRotate( vec4 q, vec3 v ) {
	const vec3 t = 2.0 * cross( q.xyz, v );
	return v + q.w * t + cross( q.xyz, t );
}

vec3 skyColor( vec3 direction ) {
	const float height = direction.y;
	vec3 color = height >= 0.0
		? mix( scene.skyHorizon.rgb, scene.skyZenith.rgb, pow( clamp( height, 0.0, 1.0 ), 0.55 ) )
		: mix( scene.skyHorizon.rgb * 0.7, scene.groundColor.rgb, clamp( -height * 3.0, 0.0, 1.0 ) );
	const float sun = max( dot( direction, scene.sunDirection.xyz ), 0.0 );
	color += scene.sunColor.rgb * (pow( sun, 2000.0 ) * 40.0 + pow( sun, 40.0 ) * 0.4);
	return color;
}

#endif
