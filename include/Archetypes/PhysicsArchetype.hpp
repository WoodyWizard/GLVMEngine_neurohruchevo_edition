#ifndef PHYSICS_ARCHETYPE
#define PHYSICS_ARCHETYPE

#include "Components/ColliderFlagsComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/MoveComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t PHYSICS_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::collider) +
		 sizeof(components::colliderFlags) +
		 sizeof(components::move) +
		 sizeof(components::rigidBody));
	
	struct PhysicsArchetype : Archetype {
		components::transform     transforms[PHYSICS_ARCH_CHUNK_SIZE];
		components::collider      colliders[PHYSICS_ARCH_CHUNK_SIZE];
		components::colliderFlags colliderFlags[PHYSICS_ARCH_CHUNK_SIZE];
		components::move          moves[PHYSICS_ARCH_CHUNK_SIZE];
		components::rigidBody     rigidBodies[PHYSICS_ARCH_CHUNK_SIZE];

		PhysicsArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::COLLIDER_COMPONENT]        = colliders;
			components[ComponentsIndices::COLLIDER_FLAGS_COMPONENT]  = colliderFlags;
			components[ComponentsIndices::MOVE_COMPONENT]            = moves;
			components[ComponentsIndices::RIGID_BODY_COMPONENT]      = rigidBodies;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_FLAGS_COMPONENT) |
				(1ull << ComponentsIndices::MOVE_COMPONENT) |
				(1ull << ComponentsIndices::RIGID_BODY_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::COLLIDER_COMPONENT;
			componentIds[2] = ComponentsIndices::COLLIDER_FLAGS_COMPONENT;
			componentIds[3] = ComponentsIndices::MOVE_COMPONENT;
			componentIds[4] = ComponentsIndices::RIGID_BODY_COMPONENT;
			componentCount = 5;
			static_assert( PHYSICS_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch

#endif
