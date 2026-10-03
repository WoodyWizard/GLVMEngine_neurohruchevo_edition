#ifndef INVENTORY_ARCHETYPE
#define INVENTORY_ARCHETYPE

#include "Components/TransformComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t INVENTORY_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::mesh) +
		 sizeof(components::inventory) +
		 sizeof(components::material));
	
	struct InventoryArchetype : Archetype {
		components::transform transforms[INVENTORY_ARCH_CHUNK_SIZE];
		components::mesh      meshes[INVENTORY_ARCH_CHUNK_SIZE];
		components::inventory invetories[INVENTORY_ARCH_CHUNK_SIZE];
		components::material  materials[INVENTORY_ARCH_CHUNK_SIZE];

		InventoryArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::INVENTORY_COMPONENT]       = invetories;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::INVENTORY_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::MESH_COMPONENT;
			componentIds[2] = ComponentsIndices::INVENTORY_COMPONENT;
			componentIds[3] = ComponentsIndices::MATERIAL_COMPONENT;
			componentCount = 4;
			static_assert( INVENTORY_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch

#endif
