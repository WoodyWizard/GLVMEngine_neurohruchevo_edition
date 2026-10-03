#ifndef MATH_OBJECT_ARCHETYPE
#define MATH_OBJECT_ARCHETYPE

#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Components/PointLightComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "TagComponents/MathObjectComponent.hpp"
#include "Components/RotationComponent.hpp"
#include "Components/MeshGenerationComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
constexpr uint32_t MATH_OBJECT_ARCH_CHUNK_SIZE =
    ARCHETYPE_CHUNK_SIZE / (sizeof(components::transform) +
                            sizeof(components::mesh) +
							sizeof(components::material) +
							sizeof(components::rotation) +
							sizeof(components::meshGeneration) +
							sizeof(components::pointLight));
	
	struct MathObjectArchetype : Archetype {
		components::transform                    transforms[MATH_OBJECT_ARCH_CHUNK_SIZE];
		components::mesh                         meshes[MATH_OBJECT_ARCH_CHUNK_SIZE];
		components::material                     materials[MATH_OBJECT_ARCH_CHUNK_SIZE];
		components::rotation                     rotations[MATH_OBJECT_ARCH_CHUNK_SIZE];
		components::meshGeneration               generatedMesh[MATH_OBJECT_ARCH_CHUNK_SIZE];
		tagComponents::mathObjectTagComponent    mathObjects[MATH_OBJECT_ARCH_CHUNK_SIZE];

		MathObjectArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;
			components[ComponentsIndices::ROTATION_COMPONENT]        = rotations;
			components[ComponentsIndices::MESH_GENERATION_COMPONENT] = generatedMesh;
			components[ComponentsIndices::MATH_OBJECT_COMPONENT]     = mathObjects;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::ROTATION_COMPONENT) |
				(1ull << ComponentsIndices::MESH_GENERATION_COMPONENT) |
				(1ull << ComponentsIndices::MATH_OBJECT_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::MESH_COMPONENT;
			componentIds[2] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[3] = ComponentsIndices::ROTATION_COMPONENT;
			componentIds[4] = ComponentsIndices::MESH_GENERATION_COMPONENT;
			componentIds[5] = ComponentsIndices::MATH_OBJECT_COMPONENT;
			componentCount = 6;
			static_assert( MATH_OBJECT_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch


#endif
