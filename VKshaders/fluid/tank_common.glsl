// Fluid tank in a host scene (FluidTank): shared declarations of the tank meshes, bodies and glass.

#ifndef TANK_COMMON_GLSL
#define TANK_COMMON_GLSL

struct Body {
	vec4 positionType;       // xyz center, w type: 0 inactive, 1 sphere, 2 box
	vec4 rotation;           // quaternion xyzw
	vec4 halfExtents;        // xyz box half extents (x = radius for spheres), w bounding radius
	vec4 velocityInvMass;
	vec4 angularVelocity;
	vec4 inverseInertia;
	vec4 color;              // rgb albedo, w material (< 0: not drawn)
};

layout(std140, set = 0, binding = 0) uniform TankParams {
	mat4 viewProjection;
	mat4 lightViewProjection;    // Fluid light maps (the view of the host light)
	vec4 cameraPosition;         // xyz, w time
	vec4 lightDirection;         // xyz towards the light, w intensity
	vec4 lightColor;
	vec4 ambientColor;
	vec4 environmentColor;       // Reflected surroundings
	vec4 waterAbsorption;        // rgb per meter, w caustics strength
	vec4 options;                // x light map texel size, y water light enabled
} tank;

layout(std430, set = 0, binding = 1) readonly buffer Vertices { vec4 vertices[]; };   // position, normal per vertex
layout(std430, set = 0, binding = 2) readonly buffer Bodies   { Body bodies[]; };
layout(set = 0, binding = 3) uniform sampler2D lightThickness;
layout(set = 0, binding = 4) uniform sampler2D lightFrontDepth;

layout(push_constant) uniform Push {
	mat4 model;
	vec4 color;                  // rgb albedo, w material
	uint flags;                  // FLAG_BODIES: instances are the bodies of bodyType; FLAG_NEAR_GLASS: glass faces towards the camera
	uint bodyType;
} push;

const uint FLAG_BODIES = 1u, FLAG_NEAR_GLASS = 2u;
const int MATERIAL_TILES = 0, MATERIAL_STEEL = 1, MATERIAL_PAINT = 2, MATERIAL_WOOD = 3, MATERIAL_BALL = 4;

vec3 quaternionRotate( vec4 q, vec3 v ) {
	const vec3 t = 2.0 * cross( q.xyz, v );
	return v + q.w * t + cross( q.xyz, t );
}

#endif
