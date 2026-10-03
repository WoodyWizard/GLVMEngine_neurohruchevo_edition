// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Systems/SpatialGridSystem.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Common/CommonFunctions.hpp"
#include "Vector.hpp"

namespace GLVM::ecs {
	void SpatialGridSystem::Update() {
		namespace arch = GLVM::ecs::arch;
		
		arch::SpatialGrid& spatialGrid = arch::world.spatialGrid;
		assert( spatialGrid.width > 0 && spatialGrid.height > 0 && spatialGrid.depth > 0 );

		cachedArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( requiredMask, cachedArchetypes, cachedArchetypesNumber );

		/// Only this system fills the grid, so clearing of previously filled cells is equal to clearing of the whole grid
		for( u32 n = 0; n < occupiedCells.GetSize(); ++n ) {
			const u32 flatIndex = occupiedCells[n];
			const u32 i4 = flatIndex % spatialGrid.width;
			const u32 i3 = (flatIndex / spatialGrid.width) % spatialGrid.height;
			const u32 i2 = flatIndex / (spatialGrid.width * spatialGrid.height);
			core::vector<u32>& cellEntities = spatialGrid.grid[i2][i3][i4].entities;
			for( u32 i5 = 0; i5 < cellEntities.GetSize(); ++i5 ) {
				const u32 entityId = cellEntities[i5];
				if ( entityId < ecs::arch::world.entityLocations.GetSize() )
					ecs::arch::world.entityLocations[entityId].gridCellCounter = 0;
			}
			cellEntities.clear();
		}
		occupiedCells.clear();

		for( uint32_t i0 = 0; i0 < cachedArchetypesNumber; ++i0 ) {
			arch::Archetype* arch = cachedArchetypes[i0];
			view.transforms = (ecs::components::transform*)arch->components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			view.meshes     = (ecs::components::mesh*)arch->components[arch::ComponentsIndices::MESH_COMPONENT];
			
			for( u32 i1 = 0; i1 < arch->entityCount; ++i1 ) {
				const arch::entity entity = arch->entities[i1];
				ecs::arch::EntityLocation& entityLocation = ecs::arch::world.entityLocations[ecs::arch::getId( entity )];
				if( !entityLocation.isDirty && isInitialized ) {
					continue;
				}
				
				const components::transform& transform = view.transforms[i1];
				const components::mesh& mesh           = view.meshes[i1];

				components::MeshHandle entityMeshHandle = mesh.handle;
				if ( entityMeshHandle.id >= allMeshMaxAbsoluteValues.GetSize() )
					continue;

				const core::MeshAxisMaxAbsoluteValues& entityChunkBounds = allMeshMaxAbsoluteValues[entityMeshHandle.id];

				/*
				  Need only left bottom back cornder point and right upper front
				  conrner point to obtain all box bounds
				*/
				vec3 minEntityPosition;
				vec3 maxEntityPosition;
				computeBoxCornerBounds( entityChunkBounds, transform.position, transform.scale, minEntityPosition, maxEntityPosition );

				/// Entities outside of the grid are not inserted. Parts of the box outside of the grid are clamped.
				arch::GridCellRange cellRange;
				if ( !spatialGrid.computeCellRange( minEntityPosition, maxEntityPosition, cellRange ) ) {
					if ( !isOutOfGridReported ) {
						std::cout << "SpatialGridSystem: entity " << ecs::arch::getId(entity) << " is outside of the spatial grid" << std::endl;
						isOutOfGridReported = true;
					}
					continue;
				}

				for( u32 i2 = cellRange.minZ; i2 <= cellRange.maxZ; ++i2 ) {
					for( u32 i3 = cellRange.minY; i3 <= cellRange.maxY; ++i3 ) {
						for( u32 i4 = cellRange.minX; i4 <= cellRange.maxX; ++i4 ) {
							core::vector<u32>& chunkEntities = spatialGrid.grid[i2][i3][i4].entities;
							if( !core::isExist<u32>( chunkEntities, ecs::arch::getId(entity) ) ) {
								const u32 currentGridCell = entityLocation.gridCellCounter;
								if ( currentGridCell >= ecs::arch::EntityLocation::maxGridCellNumber ) {
									std::cout << "SpatialGridSystem: entity " << ecs::arch::getId(entity) << " occupies too many grid cells" << std::endl;
									continue;
								}

								if ( chunkEntities.GetSize() == 0 )
									occupiedCells.Push( (i2 * spatialGrid.height + i3) * spatialGrid.width + i4 );

								chunkEntities.Push( ecs::arch::getId(entity) );
								entityLocation.gridCellIndicies[currentGridCell]  = vec3( i2, i3, i4 );
								entityLocation.cellEntityIndices[currentGridCell] = chunkEntities.GetSize() - 1;
								entityLocation.isDirty = false;
								++entityLocation.gridCellCounter;
							}
						}
					}
				}
			}
		}
		if( !isInitialized ) {
//			isInitialized = true;
		}
	}
	
}; ///< namespace GLVM::core
