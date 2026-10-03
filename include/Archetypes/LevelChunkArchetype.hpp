#ifndef LEVEL_CHUNK_ARCHETYPE
#define LEVEL_CHUNK_ARCHETYPE

#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/RotationComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "TagComponents/LevelChunkTagComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t LEVEL_CHUNK_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::material) +
		 sizeof(components::mesh) +
		 sizeof(components::collider) +
		 sizeof(components::colliderFlags) +
		 sizeof(components::rotation) +
		 sizeof(tagComponents::levelChunkTagComponent));
	
	struct LevelChunkArchetype : Archetype {
		components::transform                 transforms[LEVEL_CHUNK_ARCH_CHUNK_SIZE];
		components::material                  materials[LEVEL_CHUNK_ARCH_CHUNK_SIZE];
		components::mesh                      meshes[LEVEL_CHUNK_ARCH_CHUNK_SIZE];
		components::collider                  colliders[LEVEL_CHUNK_ARCH_CHUNK_SIZE];
		components::colliderFlags             colliderFlags[LEVEL_CHUNK_ARCH_CHUNK_SIZE];
		components::rotation                  rotations[LEVEL_CHUNK_ARCH_CHUNK_SIZE];
		tagComponents::levelChunkTagComponent levelChunkTagComponents[LEVEL_CHUNK_ARCH_CHUNK_SIZE];

		LevelChunkArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::COLLIDER_COMPONENT]        = colliders;
			components[ComponentsIndices::COLLIDER_FLAGS_COMPONENT]  = colliderFlags;
			components[ComponentsIndices::ROTATION_COMPONENT]        = rotations;
			components[ComponentsIndices::LEVEL_CHUNK_TAG_COMPONENT] = levelChunkTagComponents;

			mask =
				(1ull << ecs::arch::ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ecs::arch::ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ecs::arch::ComponentsIndices::MESH_COMPONENT) |
				(1ull << ecs::arch::ComponentsIndices::COLLIDER_COMPONENT) |
				(1ull << ecs::arch::ComponentsIndices::COLLIDER_FLAGS_COMPONENT) |
				(1ull << ecs::arch::ComponentsIndices::ROTATION_COMPONENT) |
				(1ull << ecs::arch::ComponentsIndices::LEVEL_CHUNK_TAG_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[2] = ComponentsIndices::MESH_COMPONENT;
			componentIds[3] = ComponentsIndices::COLLIDER_COMPONENT;
			componentIds[4] = ComponentsIndices::COLLIDER_FLAGS_COMPONENT;
			componentIds[5] = ComponentsIndices::ROTATION_COMPONENT;
			componentIds[6] = ComponentsIndices::LEVEL_CHUNK_TAG_COMPONENT;
			componentCount = 7;
			static_assert( LEVEL_CHUNK_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );

		}
	};
};

#endif
