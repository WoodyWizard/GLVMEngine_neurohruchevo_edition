// Position Based Fluids (Macklin & Mueller 2013): shared declarations of the simulation kernels.
//
// Particles are stored sorted by grid cell (cell size = kernel radius h). The state buffers are double
// buffered: the reorder kernel sorts "in" into "out", after that "out" is the current state. Predicted
// positions are double buffered too, every solver iteration reads "in" and writes "out" (Jacobi).

#ifndef PBF_COMMON_GLSL
#define PBF_COMMON_GLSL

struct Body {
	vec4 positionType;       // xyz center, w type: 0 inactive, 1 sphere, 2 box
	vec4 rotation;           // quaternion xyzw
	vec4 halfExtents;        // xyz box half extents (x = radius for spheres), w bounding sphere radius
	vec4 velocityInvMass;    // xyz linear velocity, w inverse mass (0 = kinematic, moved only by the application)
	vec4 angularVelocity;    // xyz angular velocity (world), w unused
	vec4 inverseInertia;     // xyz inverse inertia (body space diagonal), w linear damping
	vec4 color;              // rgb albedo, w material
};

layout(std140, set = 0, binding = 0) uniform SimParams {
	vec4  gravityDt;          // xyz gravity, w substep time
	vec4  domainMinH;         // xyz domain minimum, w kernel radius h
	vec4  domainMaxInvH;      // xyz domain maximum, w 1 / h
	uvec4 gridDims;           // xyz grid cells, w particle count
	vec4  kernel;             // x poly6 coefficient, y spiky gradient coefficient, z rest density, w 1 / rest density
	vec4  solver;             // x relaxation epsilon, y s_corr coefficient, z 1 / W(dq), w particle radius
	vec4  motion;             // x XSPH viscosity, y vorticity confinement, z maximum speed, w wall friction
	vec4  bodyParams;         // x body count, y impulse fixed point scale, z particle mass, w body restitution
	vec4  wall;               // x wall density coefficient (n/rho0 * pi K / 4), y dye diffusion, z wall adhesion, w unused
	uvec4 counts;             // x scan elements (cells + 1), y scan blocks, z recycled particles budget, w particle capacity
	vec4  drainMin;           // xyz drain box minimum, w 1 = pump enabled
	vec4  drainMax;           // xyz drain box maximum
	vec4  emitterPosition;    // xyz nozzle center, w nozzle radius
	vec4  emitterVelocity;    // xyz jet velocity, w particle spacing
	vec4  emitterColor;       // rgb dye of pumped water
} params;

layout(std430, set = 0, binding = 1)  buffer PositionsIn     { vec4 positionsIn[]; };
layout(std430, set = 0, binding = 2)  buffer VelocitiesIn    { vec4 velocitiesIn[]; };     // w = particle id
layout(std430, set = 0, binding = 3)  buffer PositionsOut    { vec4 positionsOut[]; };
layout(std430, set = 0, binding = 4)  buffer VelocitiesOut   { vec4 velocitiesOut[]; };
layout(std430, set = 0, binding = 5)  buffer ColorsIn        { vec4 colorsIn[]; };         // rgb dye, w foam
layout(std430, set = 0, binding = 6)  buffer ColorsOut       { vec4 colorsOut[]; };
layout(std430, set = 0, binding = 7)  buffer CellData        { uint cellData[]; };         // counts, after the scan: first particle of every cell
layout(std430, set = 0, binding = 8)  buffer ParticleCell    { uint particleCell[]; };
layout(std430, set = 0, binding = 9)  buffer ParticleOffset  { uint particleOffset[]; };
layout(std430, set = 0, binding = 10) buffer BlockSums       { uint blockSums[]; };
layout(std430, set = 0, binding = 11) buffer Lambdas         { float lambdas[]; };
layout(std430, set = 0, binding = 12) buffer Omegas          { vec4 omegas[]; };            // xyz vorticity, w |vorticity|
layout(std430, set = 0, binding = 13) buffer NeighborCounts  { uint neighborCounts[]; };
layout(std430, set = 0, binding = 14) buffer Bodies          { Body bodies[]; };
layout(std430, set = 0, binding = 15) buffer BodyImpulses    { int bodyImpulses[]; };      // 8 per body: linear xyz, angular xyz
layout(std430, set = 0, binding = 16) buffer Counters        { uint counters[]; };
layout(std430, set = 0, binding = 17) buffer PredictedIn     { vec4 predictedIn[]; };
layout(std430, set = 0, binding = 18) buffer PredictedOut    { vec4 predictedOut[]; };
layout(std430, set = 0, binding = 19) buffer Neighbors       { uint neighbors[]; };          // neighbors[k * maxParticles + i], k < neighborCounts[i]

uint particleCount() { return params.gridDims.w; }
float kernelRadius() { return params.domainMinH.w; }

ivec3 cellCoordinate( vec3 position ) {
	const ivec3 cell = ivec3( floor( (position - params.domainMinH.xyz) * params.domainMaxInvH.w ) );
	return clamp( cell, ivec3(0), ivec3(params.gridDims.xyz) - 1 );
}

uint cellIndex( ivec3 cell ) {
	return uint(cell.x) + params.gridDims.x * (uint(cell.y) + params.gridDims.y * uint(cell.z));
}

/// Range of sorted particles of the 3 consecutive cells (x - 1 ... x + 1) of neighbor row 0..8 around a cell.
uvec2 neighborRow( ivec3 cell, int row ) {
	const int y = cell.y + row % 3 - 1;
	const int z = cell.z + row / 3 - 1;
	if ( y < 0 || z < 0 || y >= int(params.gridDims.y) || z >= int(params.gridDims.z) )
		return uvec2( 0u );
	const int x0 = max( cell.x - 1, 0 );
	const int x1 = min( cell.x + 1, int(params.gridDims.x) - 1 );
	return uvec2( cellData[cellIndex( ivec3(x0, y, z) )], cellData[cellIndex( ivec3(x1, y, z) ) + 1u] );
}

/// Poly6 kernel W(r) from r^2.
float poly6( float r2 ) {
	const float h2 = kernelRadius() * kernelRadius();
	if ( r2 >= h2 )
		return 0.0;
	const float x = h2 - r2;
	return params.kernel.x * x * x * x;
}

/// Gradient of the spiky kernel at r (vector from the neighbor to the particle) of length rLength.
vec3 spikyGradient( vec3 r, float rLength ) {
	const float h = kernelRadius();
	if ( rLength >= h || rLength < 1e-9 )
		return vec3( 0.0 );
	const float x = h - rLength;
	return -params.kernel.y * x * x * (r / rLength);
}

/*
  Density contribution of the virtual fluid behind a wall at distance d, divided by the rest density:
  number density * integral of poly6 over the half space beyond the wall, which is a polynomial.
  Returns (contribution, derivative by d).
*/
vec2 wallDensity( float d ) {
	const float h = kernelRadius();
	if ( d >= h )
		return vec2( 0.0 );
	d = max( d, 0.0 );
	const float h2 = h * h;
	const float h4 = h2 * h2;
	const float h6 = h4 * h2;
	const float h8 = h4 * h4;
	const float d2 = d * d;
	// F(z) = h^8 z - 4/3 h^6 z^3 + 6/5 h^4 z^5 - 4/7 h^2 z^7 + z^9 / 9,  F(h) = 128/315 h^9
	const float fd = d * (h8 + d2 * (-4.0 / 3.0 * h6 + d2 * (1.2 * h4 + d2 * (-4.0 / 7.0 * h2 + d2 / 9.0))));
	const float fh = 128.0 / 315.0 * h8 * h;
	const float x = h2 - d2;
	return params.wall.x * vec2( fh - fd, -(x * x) * (x * x) );
}

/*
  Neighbor lists: built once per substep (pbf_neighbors), then every solver and velocity pass reads only the real
  neighbors (~33) instead of the 27 grid cells (~216 candidates). Slot major layout: neighbors of the same slot of
  consecutive particles are consecutive in memory.
*/
#define MAX_NEIGHBORS 64u

uint neighborAt( uint i, uint slot ) {
	return neighbors[slot * params.counts.w + i];
}

/// Adds the wall terms to the density sums and stores lambda_i = -C_i / (sum_k |grad_k C_i|^2 + epsilon).
void storeLambda( uint i, vec3 position, float density, vec3 gradientI, float sumGradient2 ) {
	/// The distance to a "low" wall grows along +axis, to a "high" wall along -axis.
	float wallRatio = 0.0;
	const vec3 low  = position - params.domainMinH.xyz;
	const vec3 high = params.domainMaxInvH.xyz - position;
	for ( int axis = 0; axis < 3; ++axis ) {
		vec3 direction = vec3( 0.0 );
		direction[axis] = 1.0;
		const vec2 lowWall  = wallDensity( low[axis] );
		const vec2 highWall = wallDensity( high[axis] );
		wallRatio += lowWall.x + highWall.x;
		gradientI += (lowWall.y - highWall.y) * direction;
	}
	/// Only compression is corrected: a free surface is not pulled together (s_corr handles the particle distribution).
	const float constraint = max( density * params.kernel.w + wallRatio - 1.0, 0.0 );
	sumGradient2 += dot( gradientI, gradientI );
	lambdas[i] = -constraint / (sumGradient2 + params.solver.x);
}

/// Velocity of a particle after the solver: v = (x* - x) / dt, with friction of the particles touching a wall.
vec3 solvedVelocity( uint j ) {
	const vec3 predicted = predictedIn[j].xyz;
	const vec3 velocity = (predicted - positionsIn[j].xyz) / params.gravityDt.w;
	const float contact = params.solver.w * 1.05;
	const vec3 low  = predicted - params.domainMinH.xyz;
	const vec3 high = params.domainMaxInvH.xyz - predicted;
	vec3 friction = vec3( 1.0 );
	for ( int axis = 0; axis < 3; ++axis ) {
		if ( low[axis] < contact || high[axis] < contact ) {
			vec3 tangential = vec3( 1.0 - params.motion.w );
			tangential[axis] = 1.0;
			friction *= tangential;
		}
	}
	return velocity * friction;
}

vec3 quaternionRotate( vec4 q, vec3 v ) {
	const vec3 t = 2.0 * cross( q.xyz, v );
	return v + q.w * t + cross( q.xyz, t );
}

vec3 quaternionRotateInverse( vec4 q, vec3 v ) {
	return quaternionRotate( vec4( -q.xyz, q.w ), v );
}

/// Signed distance and outward normal of a body at a world point.
float bodyDistance( Body body, vec3 point, out vec3 normal ) {
	const vec3 local = point - body.positionType.xyz;
	if ( body.positionType.w < 1.5 ) {
		const float len = length( local );
		normal = len > 1e-9 ? local / len : vec3( 0.0, 1.0, 0.0 );
		return len - body.halfExtents.x;
	}
	const vec3 p = quaternionRotateInverse( body.rotation, local );
	const vec3 q = abs( p ) - body.halfExtents.xyz;
	vec3 localNormal;
	float surfaceDistance;
	if ( max( q.x, max( q.y, q.z ) ) > 0.0 ) {
		const vec3 outside = max( q, vec3( 0.0 ) );
		surfaceDistance = length( outside );
		localNormal = sign( p ) * outside / max( surfaceDistance, 1e-9 );
	} else {
		surfaceDistance = max( q.x, max( q.y, q.z ) );
		localNormal = q.x >= q.y && q.x >= q.z ? vec3( sign( p.x ), 0.0, 0.0 )
			: (q.y >= q.z ? vec3( 0.0, sign( p.y ), 0.0 ) : vec3( 0.0, 0.0, sign( p.z ) ));
	}
	normal = quaternionRotate( body.rotation, localNormal );
	return surfaceDistance;
}

#endif
