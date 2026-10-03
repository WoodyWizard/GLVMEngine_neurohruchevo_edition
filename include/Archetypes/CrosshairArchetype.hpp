#ifndef CROSSHAIR_ARCHETYPE
#define CROSSHAIR_ARCHETYPE

#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "TagComponents/CrosshairTagComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t CROSSHAIR_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::mesh) +
		 sizeof(components::material) +
		 sizeof(tagComponents::crossHairTagComponent));
	
	struct CrosshairArchetype : Archetype {
		components::transform                transforms[CROSSHAIR_ARCH_CHUNK_SIZE];
		components::mesh                     meshes[CROSSHAIR_ARCH_CHUNK_SIZE];
		components::material                 materials[CROSSHAIR_ARCH_CHUNK_SIZE];
		tagComponents::crossHairTagComponent crosshairTagComponents[CROSSHAIR_ARCH_CHUNK_SIZE];

		CrosshairArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]     = transforms;
			components[ComponentsIndices::MESH_COMPONENT]          = meshes;
			components[ComponentsIndices::MATERIAL_COMPONENT]      = materials;
			components[ComponentsIndices::CROSSHAIR_TAG_COMPONENT] = crosshairTagComponents;  

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::CROSSHAIR_TAG_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::MESH_COMPONENT;
			componentIds[2] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[3] = ComponentsIndices::CROSSHAIR_TAG_COMPONENT;
			componentCount = 4;
			static_assert( CROSSHAIR_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch

#endif
