#ifndef SPOT_LIGHT_ARCHETYPE
#define SPOT_LIGHT_ARCHETYPE

#include "Components/SpotLightComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t SPOT_LIGHT_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::mesh) +
		 sizeof(components::material) +
		 sizeof(components::spotLight));
	
	struct SpotLightArchetype : Archetype {
		components::transform     transforms[SPOT_LIGHT_ARCH_CHUNK_SIZE];
		components::mesh          meshes[SPOT_LIGHT_ARCH_CHUNK_SIZE];
		components::material      materials[SPOT_LIGHT_ARCH_CHUNK_SIZE];
		components::spotLight     spotLights[SPOT_LIGHT_ARCH_CHUNK_SIZE];

		SpotLightArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;
			components[ComponentsIndices::SPOT_LIGHT_COMPONENT]      = spotLights;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::SPOT_LIGHT_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::MESH_COMPONENT;
			componentIds[2] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[3] = ComponentsIndices::SPOT_LIGHT_COMPONENT;
			componentCount = 4;
			static_assert( SPOT_LIGHT_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch

#endif
