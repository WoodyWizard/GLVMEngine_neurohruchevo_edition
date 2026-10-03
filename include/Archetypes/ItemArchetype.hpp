#ifndef ITEM_ARCHETYPE
#define ITEM_ARCHETYPE

#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/ItemComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/MoveComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/RotationComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t ITEM_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::collider) +
		 sizeof(components::colliderFlags) +
		 sizeof(components::mesh) +
		 sizeof(components::rigidBody) +
		 sizeof(components::material) +
		 sizeof(components::rotation) +
		 sizeof(components::move) +
		 sizeof(components::item));
	
	struct ItemArchetype : Archetype {
		components::transform     transforms[ITEM_ARCH_CHUNK_SIZE];
		components::collider      colliders[ITEM_ARCH_CHUNK_SIZE];
		components::colliderFlags colliderFlags[ITEM_ARCH_CHUNK_SIZE];
		components::mesh          meshes[ITEM_ARCH_CHUNK_SIZE];
		components::rigidBody     rigidBodies[ITEM_ARCH_CHUNK_SIZE];
		components::material      materials[ITEM_ARCH_CHUNK_SIZE];
		components::rotation      rotations[ITEM_ARCH_CHUNK_SIZE];
		components::move          moves[ITEM_ARCH_CHUNK_SIZE];
		components::item          items[ITEM_ARCH_CHUNK_SIZE];

		ItemArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::COLLIDER_COMPONENT]        = colliders;
			components[ComponentsIndices::COLLIDER_FLAGS_COMPONENT]  = colliderFlags;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::RIGID_BODY_COMPONENT]      = rigidBodies;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;
			components[ComponentsIndices::ROTATION_COMPONENT]        = rotations;
			components[ComponentsIndices::MOVE_COMPONENT]            = moves;
			components[ComponentsIndices::ITEM_COMPONENT]            = items;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_FLAGS_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::RIGID_BODY_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::ROTATION_COMPONENT) |
				(1ull << ComponentsIndices::MOVE_COMPONENT) |
				(1ull << ComponentsIndices::ITEM_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::COLLIDER_COMPONENT;
			componentIds[2] = ComponentsIndices::COLLIDER_FLAGS_COMPONENT;
			componentIds[3] = ComponentsIndices::MESH_COMPONENT;
			componentIds[4] = ComponentsIndices::RIGID_BODY_COMPONENT;
			componentIds[5] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[6] = ComponentsIndices::ROTATION_COMPONENT;
			componentIds[7] = ComponentsIndices::MOVE_COMPONENT;
			componentIds[8] = ComponentsIndices::ITEM_COMPONENT;
			componentCount = 9;
			static_assert( ITEM_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch

#endif
