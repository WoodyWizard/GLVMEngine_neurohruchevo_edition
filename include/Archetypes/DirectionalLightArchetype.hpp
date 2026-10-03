#ifndef DIRECTIONAL_LIGHT_ARCHETYPE
#define DIRECTIONAL_LIGHT_ARCHETYPE

#include "Components/DirectionalLightComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t DIRECTIONAL_LIGHT_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::mesh) +
		 sizeof(components::material) +
		 sizeof(components::directionalLight));
	
	struct DirectionalLightArchetype : Archetype {
		components::transform        transforms[DIRECTIONAL_LIGHT_ARCH_CHUNK_SIZE];
		components::mesh             meshes[DIRECTIONAL_LIGHT_ARCH_CHUNK_SIZE];
		components::material         materials[DIRECTIONAL_LIGHT_ARCH_CHUNK_SIZE];
		components::directionalLight directionalLights[DIRECTIONAL_LIGHT_ARCH_CHUNK_SIZE];

		DirectionalLightArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]         = transforms;
			components[ComponentsIndices::MESH_COMPONENT]              = meshes;
			components[ComponentsIndices::MATERIAL_COMPONENT]          = materials;
			components[ComponentsIndices::DIRECTIONAL_LIGHT_COMPONENT] = directionalLights;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::DIRECTIONAL_LIGHT_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::MESH_COMPONENT;
			componentIds[2] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[3] = ComponentsIndices::DIRECTIONAL_LIGHT_COMPONENT;
			componentCount = 4;
			static_assert( DIRECTIONAL_LIGHT_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch

#endif
