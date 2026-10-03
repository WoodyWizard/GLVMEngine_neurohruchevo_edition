#ifndef POINT_LIGHT_ARCHETYPE
#define POINT_LIGHT_ARCHETYPE

#include "Components/PointLightComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t POINT_LIGHT_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::mesh) +
		 sizeof(components::material) +
		 sizeof(components::pointLight));
	
	struct PointLightArchetype : Archetype {
		components::transform     transforms[POINT_LIGHT_ARCH_CHUNK_SIZE];
		components::mesh          meshes[POINT_LIGHT_ARCH_CHUNK_SIZE];
		components::material      materials[POINT_LIGHT_ARCH_CHUNK_SIZE];
		components::pointLight    pointLights[POINT_LIGHT_ARCH_CHUNK_SIZE];

		PointLightArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;
			components[ComponentsIndices::POINT_LIGHT_COMPONENT]     = pointLights;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::POINT_LIGHT_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::MESH_COMPONENT;
			componentIds[2] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[3] = ComponentsIndices::POINT_LIGHT_COMPONENT;
			componentCount = 4;
			static_assert( POINT_LIGHT_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch


#endif
