#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Components/AnimationComponent.hpp"
#include "Components/AttackComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/DamageComponent.hpp"
#include "Components/DirectionalLightComponent.hpp"
#include "Components/EnemyComponent.hpp"
#include "Components/FontComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "Components/ItemComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/PointLightComponent.hpp"
#include "Components/ProjectileBundle.hpp"
#include "Components/ProjectileComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/SpotLightComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Components/ViewComponent.hpp"
#include "TagComponents/LevelChunkTagComponent.hpp"
#include "TagComponents/PlayerTagComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/StateComponent.hpp"
#include "Components/MoveComponent.hpp"
#include "TagComponents/ProjectileTagComponent.hpp"
#include "TagComponents/CrosshairTagComponent.hpp"
#include "TagComponents/StaticMeshTagComponent.hpp"
#include "TagComponents/MathObjectComponent.hpp"
#include "Components/RotationComponent.hpp"
#include "Components/MeshGenerationComponent.hpp"
#include <climits>
#include <iostream>
#include <utility>

namespace GLVM::ecs::arch {
	namespace {
		/// Move component of the last entity into the slot of removed one.
		template< typename T >
		void moveComponent( void* componentArray, const uint32_t index, const uint32_t last ) {
			T* typedArray = static_cast<T*>( componentArray );
			typedArray[index] = std::move( typedArray[last] );
		}
	}

	/// Returns UINT32_MAX if entities array is full. Note that real capacity of derived archetypes
	/// (size of their component arrays) is checked by World::addEntityToArchetype.
	uint32_t Archetype::addEntity( entity entity_ ) {
		if ( entityCount >= CAPACITY ) {
			std::cerr << "Archetype::addEntity: entities array is full" << std::endl;
			return UINT32_MAX;
		}

		uint32_t index = entityCount++;
		entities[index] = entity_;
		
		return index;
	}
	
	/// Swap-remove
	entity Archetype::removeEntity( uint32_t index ) {
		assert( entityCount > 0 && index < entityCount );
		uint32_t last = entityCount - 1;

		static_assert( ComponentsIndices::COMPONENTS_COUNT == 29, "New component type must be handled in Archetype::removeEntity" );

		if ( index != last ) {
			for( uint32_t i = 0; i < componentCount; ++i ) {
				const uint32_t componentId = componentIds[i];

				switch( componentId ) {
				case ComponentsIndices::TRANSFORM_COMPONENT:
					moveComponent<components::transform>( components[componentId], index, last );
					break;
				case ComponentsIndices::RIGID_BODY_COMPONENT:
					moveComponent<components::rigidBody>( components[componentId], index, last );
					break;
				case ComponentsIndices::MESH_COMPONENT:
					moveComponent<components::mesh>( components[componentId], index, last );
					break;
				case ComponentsIndices::FONT_COMPONENT:
					moveComponent<components::font>( components[componentId], index, last );
					break;
				case ComponentsIndices::COLLIDER_COMPONENT:
					moveComponent<components::collider>( components[componentId], index, last );
					break;
				case ComponentsIndices::COLLIDER_FLAGS_COMPONENT:
					moveComponent<components::colliderFlags>( components[componentId], index, last );
					break;
				case ComponentsIndices::MATERIAL_COMPONENT:
					moveComponent<components::material>( components[componentId], index, last );
					break;
				case ComponentsIndices::VIEW_COMPONENT:
					moveComponent<components::beholder>( components[componentId], index, last );
					break;
				case ComponentsIndices::HEALTH_COMPONENT:
					moveComponent<components::health>( components[componentId], index, last );
					break;
				case ComponentsIndices::ANIMATION_COMPONENT:
					moveComponent<components::animation>( components[componentId], index, last );
					break;
				case ComponentsIndices::STATE_COMPONENT:
					moveComponent<components::state>( components[componentId], index, last );
					break;
				case ComponentsIndices::ENEMY_COMPONENT:
					moveComponent<components::enemy>( components[componentId], index, last );
					break;
				case ComponentsIndices::DAMAGE_COMPONENT:
					moveComponent<components::damage>( components[componentId], index, last );
					break;
				case ComponentsIndices::ATTACK_COMPONENT:
					moveComponent<components::attack>( components[componentId], index, last );
					break;
				case ComponentsIndices::INVENTORY_COMPONENT:
					moveComponent<components::inventory>( components[componentId], index, last );
					break;
				case ComponentsIndices::DIRECTIONAL_LIGHT_COMPONENT:
					moveComponent<components::directionalLight>( components[componentId], index, last );
					break;
				case ComponentsIndices::SPOT_LIGHT_COMPONENT:
					moveComponent<components::spotLight>( components[componentId], index, last );
					break;
				case ComponentsIndices::POINT_LIGHT_COMPONENT:
					moveComponent<components::pointLight>( components[componentId], index, last );
					break;
				case ComponentsIndices::ITEM_COMPONENT:
					moveComponent<components::item>( components[componentId], index, last );
					break;
				case ComponentsIndices::MOVE_COMPONENT:
					moveComponent<components::move>( components[componentId], index, last );
					break;
				case ComponentsIndices::PROJECTILE_BUNDLE_COMPONENT:
					moveComponent<ProjectileBundle>( components[componentId], index, last );
					break;
				case ComponentsIndices::ROTATION_COMPONENT:
					moveComponent<components::rotation>( components[componentId], index, last );
					break;
				case ComponentsIndices::MESH_GENERATION_COMPONENT:
					moveComponent<components::meshGeneration>( components[componentId], index, last );
					break;
				case ComponentsIndices::LEVEL_CHUNK_TAG_COMPONENT:
					moveComponent<tagComponents::levelChunkTagComponent>( components[componentId], index, last );
					break;
				case ComponentsIndices::PLAYER_TAG_COMPONENT:
					moveComponent<tagComponents::playerTagComponent>( components[componentId], index, last );
					break;
				case ComponentsIndices::CROSSHAIR_TAG_COMPONENT:
					moveComponent<tagComponents::crossHairTagComponent>( components[componentId], index, last );
					break;
				case ComponentsIndices::STATIC_MESH_TAG_COMPONENT:
					moveComponent<tagComponents::staticMeshTagComponent>( components[componentId], index, last );
					break;
				case ComponentsIndices::PROJECTILE_TAG_COMPONENT:
					moveComponent<tagComponents::projectileTagComponent>( components[componentId], index, last );
					break;
				case ComponentsIndices::MATH_OBJECT_COMPONENT:
					moveComponent<tagComponents::mathObjectTagComponent>( components[componentId], index, last );
					break;
				default:
					std::cerr << "Archetype::removeEntity: unknown component id " << componentId << std::endl;
					break;
				}
			}
		}
		
		entity moved = entities[last];
		entities[index] = moved;
		--entityCount;

		return moved;
	}
}; // namespace GLVM::ecs::arch
