// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// This file is part of Game Loop Versatile Modules (GLVM)
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Systems/CollisionSystem.hpp"
#include "Archetypes/CrosshairArchetype.hpp"
#include "Archetypes/DirectionalLightArchetype.hpp"
#include "Archetypes/InventoryArchetype.hpp"
#include "Archetypes/ItemArchetype.hpp"
#include "Archetypes/LevelChunkArchetype.hpp"
#include "Archetypes/PointLightArchetype.hpp"
#include "Archetypes/SpotLightArchetype.hpp"
#include "Common/CommonFunctions.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/MoveComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Vector.hpp"
#include "VertexMath.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "ArchetypeECS/ArchECS_Utils.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Archetypes/EnemyArchetype.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "VertexMath.hpp"
#include <Systems/ProjectileSystem.hpp>
#include <algorithm>
#include <cstdint>
#include <sys/types.h>
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "Archetypes/ProjectileArchetype.hpp"
#include "Archetypes/StaticMeshArchetype.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"

namespace GLVM::ecs
{
	void CCollisionSystem::Update()
	{
		namespace arch = GLVM::ecs::arch;
		namespace cm   = GLVM::ecs::components;

		/// Spatial grid common data
		const arch::SpatialGrid& spatialGrid = arch::world.spatialGrid;
		assert( spatialGrid.width > 0 && spatialGrid.height > 0 && spatialGrid.depth > 0 );

		cachedArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( requiredMask, cachedArchetypes, cachedArchetypesNumber );
			
		const float cameraSpeed = 5.5f * fDelta_Time_;
		/// Outer cycle on every archetype
		for( uint32_t x = 0; x < cachedArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedArchetypes[x];
			view.backtrackingTransforms    = (ecs::components::transform*)arch->components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			view.backtrackingColliders     = (ecs::components::collider*)arch->components[arch::ComponentsIndices::COLLIDER_COMPONENT];
			view.backtrackingColliderFlags = (ecs::components::colliderFlags*)arch->components[arch::ComponentsIndices::COLLIDER_FLAGS_COMPONENT];
			view.backtrackingMeshes        = (ecs::components::mesh*)arch->components[arch::ComponentsIndices::MESH_COMPONENT];

			for(unsigned int i = 0; i < arch->entityCount; ++i) {
				/// Count on every entity in current outer archetype
				uint32_t backtrackingEntityID = arch->entities[i];
				
				uint8_t groudCollisionTurnOffMask = (1u << 0) | (0u << 1) | (1u << 2) | (1u << 3);
				if( view.backtrackingColliderFlags && view.backtrackingColliders &&
					view.backtrackingMeshes && view.backtrackingTransforms ) {
				
					view.backtrackingColliderFlags[i].flags = view.backtrackingColliderFlags[i].flags & groudCollisionTurnOffMask;
					view.backtrackingColliders[i].colliders.clear();
					components::mesh backtrackinEntityMesh = view.backtrackingMeshes[i];
					components::MeshHandle backtrackingEntityMeshHandle = backtrackinEntityMesh.handle;
					components::transform* backtrackingTransformComponent = &view.backtrackingTransforms[i];
					vec3 backtrackingTransform = backtrackingTransformComponent->position;
					[[maybe_unused]] float backtrackingScale = backtrackingTransformComponent->scale;

					arch::componentMask moveRequiredMask = (1ul << arch::ComponentsIndices::MOVE_COMPONENT);
					/// Check if outer current archetype has move component
					const bool backtrackingHasMove = arch::matchesRequiredMask( arch->mask, moveRequiredMask );
					if ( backtrackingHasMove ) {
						view.backtrackingMove = (ecs::components::move*)arch->components[arch::ComponentsIndices::MOVE_COMPONENT];
						backtrackingTransform += Normalize(view.backtrackingMove[i].frameMovement) * cameraSpeed;
						backtrackingTransform += view.backtrackingMove[i].gravity;
					}

					if ( backtrackingEntityMeshHandle.id >= allMeshMaxAbsoluteValues.GetSize() )
						continue;

					///< Collect entities from grid chunks
					const core::MeshAxisMaxAbsoluteValues& entityChunkBounds = allMeshMaxAbsoluteValues[backtrackingEntityMeshHandle.id];

					/*
					  Need only left bottom back cornder point and right upper front
					  conrner point to obtain all box bounds
					*/
					vec3 minEntityPosition;
					vec3 maxEntityPosition;
					computeBoxCornerBounds( entityChunkBounds, backtrackingTransformComponent->position, backtrackingTransformComponent->scale,
											minEntityPosition, maxEntityPosition );

					collectedEntities.clear();                                       ///< Result array with collected entities
					arch::GridCellRange cellRange;
					if ( spatialGrid.computeCellRange( minEntityPosition, maxEntityPosition, cellRange ) ) {
						for( u32 i2 = cellRange.minZ; i2 <= cellRange.maxZ; ++i2 ) {
							for( u32 i3 = cellRange.minY; i3 <= cellRange.maxY; ++i3 ) {
								for( u32 i4 = cellRange.minX; i4 <= cellRange.maxX; ++i4 ) {
									const core::vector<u32>& chunkEntities = spatialGrid.grid[i2][i3][i4].entities;
									for( u32 i5 = 0; i5 < chunkEntities.GetSize(); ++i5 ) {
										const u32 entity = chunkEntities[i5];
										if( !core::isExist( collectedEntities, entity ) )
											collectedEntities.Push( entity );
									}
								}
							}
						}
					}

					/// Inner cycle on every archetype
					/// Count on every entity in current inner archetype
					for(unsigned int j = 0; j < collectedEntities.GetSize(); ++j) {
						/// Check for same entityID and iteration
						uint32_t comparedEntityID = collectedEntities[j];
						if( backtrackingEntityID == comparedEntityID || comparedEntityID >= arch::world.entityLocations.GetSize() ) {
							continue;
						}

						const arch::EntityLocation& comparedEntityLocation = arch::world.entityLocations[comparedEntityID];
						const arch::Archetype* comparedArch = comparedEntityLocation.arch;
						if( comparedArch == nullptr || !arch::matchesRequiredMask( comparedArch->mask, requiredMask ) ) {
							continue;
						}
						const uint32_t comparedEntityIndex = comparedEntityLocation.index;

						/// Components are taken for the current compared entity only. Move component exists not in every archetype.
						const components::transform* comparedTransformComponent =
							&((ecs::components::transform*)comparedArch->components[arch::ComponentsIndices::TRANSFORM_COMPONENT])[comparedEntityIndex];
						const components::mesh* comparedMeshComponent =
							&((ecs::components::mesh*)comparedArch->components[arch::ComponentsIndices::MESH_COMPONENT])[comparedEntityIndex];
						const components::MeshHandle comparedEntityMeshHandle = comparedMeshComponent->handle;
						if( comparedEntityMeshHandle.id >= allMeshMaxAbsoluteValues.GetSize() ) {
							continue;
						}

						const components::move* comparedMoveComponent = nullptr;
						const arch::componentMask moveRequiredMask = (1ul << arch::ComponentsIndices::MOVE_COMPONENT);
						if( arch::matchesRequiredMask( comparedArch->mask, moveRequiredMask ) ) {
							comparedMoveComponent = &((ecs::components::move*)comparedArch->components[arch::ComponentsIndices::MOVE_COMPONENT])[comparedEntityIndex];
						}

						vec3  comparedTransform = comparedTransformComponent->position;
						float comparedScale     = comparedTransformComponent->scale;

						if( comparedMoveComponent != nullptr ) {
							comparedTransform += Normalize(comparedMoveComponent->frameMovement) * cameraSpeed;
							comparedTransform += comparedMoveComponent->gravity;
						}

						bool boxColliderFlag = false;
						bool upperActorCheckFlag = false;

						const core::MeshAxisMaxAbsoluteValues& backtrackingMeshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[backtrackingEntityMeshHandle.id];
						const core::MeshAxisMaxAbsoluteValues& comparedMeshAxisMaxAbsoluteValues     = allMeshMaxAbsoluteValues[comparedEntityMeshHandle.id];

						boxColliderFlag = core::BoxCollider(backtrackingTransform,
															comparedTransform,
															backtrackingScale,
															comparedScale,
															backtrackingMeshAxisMaxAbsoluteValues,
															comparedMeshAxisMaxAbsoluteValues);

						/*
						  "Standing on top" is checked with the position before this frame's movement: a large fall step
						  (low FPS, frame stall) must not push the entity below the ground surface tolerance and turn the
						  ground into a wall.
						*/
						if ( boxColliderFlag ) {
							upperActorCheckFlag = UpperActorCheck(backtrackingTransformComponent->position,
																  comparedTransform,
																  backtrackingScale,
																  comparedScale,
																  backtrackingEntityMeshHandle,
																  comparedEntityMeshHandle);
						}

						if(upperActorCheckFlag && boxColliderFlag) {
									
							uint8_t groudCollisionTurnOnMask = (0u << 0) | (1u << 1) | (0u << 2) | (0u << 3);
							view.backtrackingColliderFlags[i].flags = view.backtrackingColliderFlags[i].flags | groudCollisionTurnOnMask;
							view.backtrackingColliders[i].colliders.Push(comparedEntityID);

							/// Limit the fall of this frame so the entity lands exactly on the ground surface
							if ( backtrackingHasMove ) {
								const float groundTop = comparedTransform[1] + comparedMeshAxisMaxAbsoluteValues.origin_offset_y +
									comparedMeshAxisMaxAbsoluteValues.absolute_y * comparedScale;
								const float entityBottom = backtrackingTransformComponent->position[1] + backtrackingMeshAxisMaxAbsoluteValues.origin_offset_y -
									backtrackingMeshAxisMaxAbsoluteValues.absolute_y * backtrackingScale;
								float& fallStep = view.backtrackingMove[i].gravity[1];
								fallStep = std::max( fallStep, groundTop - entityBottom );
							}

							continue;
						}
                    
						if(boxColliderFlag) {
							uint8_t wallCollisionTurnOnMask = (1u << 0) | (0u << 1) | (0u << 2) | (0u << 3);
							view.backtrackingColliderFlags[i].flags = view.backtrackingColliderFlags[i].flags | wallCollisionTurnOnMask;
							view.backtrackingColliders[i].colliders.Push(comparedEntityID);
							
							continue;
						}
					}
				}
			}
		}
		cachedArchetypesNumber = 0;
	}

    bool CCollisionSystem::UpperActorCheck(vec3 backtrackingPosition,
										   vec3 comparedPosition,
										   float backtrackingScale,
										   float comparedScale,
										   components::MeshHandle backtrackingMeshHandle,
										   components::MeshHandle comparedMeshHandle) {
		core::MeshAxisMaxAbsoluteValues backtrackingMeshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[backtrackingMeshHandle.id];

		// std::cout << "array size: " << allMeshMaxAbsoluteValues.GetSize() << std::endl;
		// std::cout << "mesh id: " << comparedMeshHandle.id << std::endl;
		
		core::MeshAxisMaxAbsoluteValues comparedMeshAxisMaxAbsoluteValues     = allMeshMaxAbsoluteValues[comparedMeshHandle.id];

		constexpr float epsilon = 0.15f;
        if( backtrackingPosition[1] + backtrackingMeshAxisMaxAbsoluteValues.origin_offset_y -
			backtrackingMeshAxisMaxAbsoluteValues.absolute_y * backtrackingScale + epsilon >
			comparedPosition[1] + comparedMeshAxisMaxAbsoluteValues.origin_offset_y +
			comparedMeshAxisMaxAbsoluteValues.absolute_y * comparedScale ) {
            return true;
        }

        return false;
    }
}
