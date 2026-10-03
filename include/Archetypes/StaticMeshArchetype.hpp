#ifndef STATIC_MESH_ARCHETYPE_HPP
#define STATIC_MESH_ARCHETYPE_HPP

#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/FontComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Components/RotationComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "TagComponents/StaticMeshTagComponent.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t STATIC_MESH_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::collider) +
		 sizeof(components::colliderFlags) +
		 sizeof(components::mesh) +
		 sizeof(components::material) +
		 sizeof(components::font) +
		 sizeof(components::rotation) +
		 sizeof(tagComponents::staticMeshTagComponent));
	
	struct StaticMeshArchetype : Archetype {
		components::transform                 transforms[STATIC_MESH_ARCH_CHUNK_SIZE];
		components::collider                  colliders[STATIC_MESH_ARCH_CHUNK_SIZE];
		components::colliderFlags             colliderFlags[STATIC_MESH_ARCH_CHUNK_SIZE];
		components::mesh                      meshes[STATIC_MESH_ARCH_CHUNK_SIZE];
		components::material                  materials[STATIC_MESH_ARCH_CHUNK_SIZE];
		components::font                      fonts[STATIC_MESH_ARCH_CHUNK_SIZE];
		components::rotation                  rotations[STATIC_MESH_ARCH_CHUNK_SIZE];
		tagComponents::staticMeshTagComponent staticMeshTagComponents[STATIC_MESH_ARCH_CHUNK_SIZE];

		StaticMeshArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::COLLIDER_COMPONENT]        = colliders;
			components[ComponentsIndices::COLLIDER_FLAGS_COMPONENT]  = colliderFlags;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;
			components[ComponentsIndices::FONT_COMPONENT]            = fonts;
			components[ComponentsIndices::ROTATION_COMPONENT]        = rotations;
			components[ComponentsIndices::STATIC_MESH_TAG_COMPONENT] = staticMeshTagComponents;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_FLAGS_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::FONT_COMPONENT) |
				(1ull << ComponentsIndices::ROTATION_COMPONENT) |
				(1ull << ComponentsIndices::STATIC_MESH_TAG_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::COLLIDER_COMPONENT;
			componentIds[2] = ComponentsIndices::COLLIDER_FLAGS_COMPONENT;
			componentIds[3] = ComponentsIndices::MESH_COMPONENT;
			componentIds[4] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[5] = ComponentsIndices::FONT_COMPONENT;
			componentIds[6] = ComponentsIndices::ROTATION_COMPONENT;
			componentIds[7] = ComponentsIndices::STATIC_MESH_TAG_COMPONENT;
			componentCount = 8;
			static_assert( STATIC_MESH_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch


#endif
