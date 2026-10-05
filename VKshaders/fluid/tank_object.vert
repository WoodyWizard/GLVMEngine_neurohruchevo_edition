#version 450
#extension GL_GOOGLE_include_directive : require
// Tank meshes pulled from the vertex buffer (model matrix per draw); bodies are instanced from the simulation body buffer.

#include "tank_common.glsl"

layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec3 localPosition;
layout(location = 3) flat out vec4 albedo;

void main() {
	const vec3 position = vertices[gl_VertexIndex * 2].xyz;
	const vec3 normal = vertices[gl_VertexIndex * 2 + 1].xyz;
	localPosition = position;

	if ( (push.flags & FLAG_BODIES) != 0u ) {
		const Body body = bodies[gl_InstanceIndex];
		if ( uint( body.positionType.w + 0.5 ) != push.bodyType || body.color.w < 0.0 ) {
			gl_Position = vec4( 2.0, 2.0, 2.0, 1.0 );                       // Other body type or not drawn: degenerate, clipped
			return;
		}
		const vec3 scale = push.bodyType == 1u ? vec3( body.halfExtents.x ) : body.halfExtents.xyz;
		worldPosition = body.positionType.xyz + quaternionRotate( body.rotation, position * scale );
		worldNormal = quaternionRotate( body.rotation, normalize( normal / scale ) );
		albedo = body.color;
	} else {
		worldPosition = (push.model * vec4( position, 1.0 )).xyz;
		worldNormal = normalize( transpose( inverse( mat3( push.model ) ) ) * normal );
		albedo = push.color;
	}
	gl_Position = tank.viewProjection * vec4( worldPosition, 1.0 );
}
