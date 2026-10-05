#version 450
#extension GL_GOOGLE_include_directive : require
// Camera (or sun, pass 1) facing quad per particle, 6 vertices per instance, particle data pulled from the buffers.

#include "fluid_render_common.glsl"

layout(std430, set = 0, binding = 1) readonly buffer Positions  { vec4 positions[]; };
layout(std430, set = 0, binding = 2) readonly buffer Velocities { vec4 velocities[]; };
layout(std430, set = 0, binding = 3) readonly buffer Colors     { vec4 colors[]; };

layout(push_constant) uniform Push { uint pass; } push;     // 0 camera, 1 sun (light thickness map)

layout(location = 0) out vec2 corner;
layout(location = 1) out vec3 viewCenter;
layout(location = 2) flat out uint particle;

const vec2 CORNERS[6] = vec2[]( vec2( -1.0, -1.0 ), vec2( 1.0, -1.0 ), vec2( 1.0, 1.0 ),
								vec2( -1.0, -1.0 ), vec2( 1.0, 1.0 ), vec2( -1.0, 1.0 ) );

void main() {
	const uint i = gl_InstanceIndex;
	corner = CORNERS[gl_VertexIndex];
	particle = i;
	const float radius = params.cameraPosition.w;
	const mat4 viewMatrix = push.pass == 0u ? params.view : params.lightView;
	viewCenter = (viewMatrix * vec4( positions[i].xyz, 1.0 )).xyz;
	const vec3 cornerPosition = viewCenter + vec3( corner * radius, 0.0 );
	gl_Position = (push.pass == 0u ? params.projection : params.lightProjection) * vec4( cornerPosition, 1.0 );
}
