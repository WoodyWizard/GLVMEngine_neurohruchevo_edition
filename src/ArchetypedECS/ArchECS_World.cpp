#include "ArchetypeECS/ArchECS_World.hpp"

#include "Archetypes/CrosshairArchetype.hpp"
#include "Archetypes/InventoryArchetype.hpp"
#include "Archetypes/LevelChunkArchetype.hpp"
#include "Archetypes/RigidBodyArchetype.hpp"
#include "Archetypes/SpotLightArchetype.hpp"
#include "Archetypes/StaticMeshArchetype.hpp"
#include "Archetypes/ProjectileArchetype.hpp"
#include "Archetypes/ItemArchetype.hpp"
#include "Archetypes/DirectionalLightArchetype.hpp"
#include "Archetypes/PointLightArchetype.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "Archetypes/EnemyArchetype.hpp"
#include "Archetypes/MathObjectaArchetype.hpp"
#include "Archetypes/DamageArchetype.hpp"
#include "Archetypes/PhysicsArchetype.hpp"
#include <algorithm>
#include <climits>
#include <cmath>
#include <iostream>

namespace GLVM::ecs::arch {
	World world = {};

	World::World() {
		assert( spatialGrid.width > 0 && spatialGrid.height > 0 && spatialGrid.depth > 0 );
		
		const float chunkSize       = spatialGrid.grid[0][0][0].size;
		const float halfWorldWidth  = spatialGrid.width * chunkSize * 0.5f;
		const float halfWorldHeight = spatialGrid.height * chunkSize * 0.5f;
		const float halfWorldDepth  = spatialGrid.depth * chunkSize * 0.5f;
		const float halfChunkSize   = chunkSize * 0.5f;
		const vec3 pivot = vec3( -halfWorldWidth + halfChunkSize, -halfWorldHeight + halfChunkSize, -halfWorldDepth + halfChunkSize );
		for( u32 i0 = 0; i0 < spatialGrid.depth; ++i0 ) {
			for( u32 i1 = 0; i1 < spatialGrid.height; ++i1 ) {
				for( u32 i2 = 0; i2 < spatialGrid.width; ++i2 ) {
					spatialGrid.grid[i0][i1][i2].position = vec3( i2 * chunkSize, i1 * chunkSize, i0 * chunkSize ) + pivot;
				}
			}
		}
	}
	
	World::~World() {
		for( unsigned int i = 0; i < archetypes.GetSize(); ++i ) {
			delete archetypes[i];
			archetypes[i] = nullptr;
		}
	}
	
	uint32_t getArchetypeCapacity( const Archetype* arch ) {
		if ( dynamic_cast<const PlayerArchetype*>( arch ) )           return PLAYER_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const EnemyArchetype*>( arch ) )            return ENEMY_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const ProjectileArchetype*>( arch ) )       return PROJECTILE_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const LevelChunkArchetype*>( arch ) )       return LEVEL_CHUNK_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const StaticMeshArchetype*>( arch ) )       return STATIC_MESH_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const CrosshairArchetype*>( arch ) )        return CROSSHAIR_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const InventoryArchetype*>( arch ) )        return INVENTORY_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const ItemArchetype*>( arch ) )             return ITEM_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const DirectionalLightArchetype*>( arch ) ) return DIRECTIONAL_LIGHT_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const PointLightArchetype*>( arch ) )       return POINT_LIGHT_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const SpotLightArchetype*>( arch ) )        return SPOT_LIGHT_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const MathObjectArchetype*>( arch ) )       return MATH_OBJECT_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const DamageArchetype*>( arch ) )           return DAMAGE_ARCH_CHUNK_SIZE;
		if ( dynamic_cast<const PhysicsArchetype*>( arch ) )          return PHYSICS_ARCH_CHUNK_SIZE;

		std::cerr << "getArchetypeCapacity: unknown archetype type, size of component arrays can not be checked" << std::endl;
		return Archetype::CAPACITY;
	}

	namespace {
		/// Convert one axis of a box to inclusive range of cell indices. Returns false if the box is outside of the grid on this axis.
		bool computeAxisCellRange( const float minValue, const float maxValue, const float halfSize, const u32 cellsNumber,
								   u32& minIndex, u32& maxIndex ) {
			if ( !std::isfinite( minValue ) || !std::isfinite( maxValue ) )
				return false;

			const float chunkSize = GridChunk::size;
			const float minCell = std::floor( (minValue + halfSize) / chunkSize );
			const float maxCell = std::floor( (maxValue + halfSize) / chunkSize );
			if ( maxCell < 0.0f || minCell >= static_cast<float>(cellsNumber) || minCell > maxCell )
				return false;

			minIndex = static_cast<u32>( std::max( minCell, 0.0f ) );
			maxIndex = static_cast<u32>( std::min( maxCell, static_cast<float>(cellsNumber - 1) ) );
			return true;
		}
	}

	bool SpatialGrid::computeCellRange( const vec3& minPosition, const vec3& maxPosition, GridCellRange& range ) const {
		const float halfWidth  = width * GridChunk::size * 0.5f;
		const float halfHeight = height * GridChunk::size * 0.5f;
		const float halfDepth  = depth * GridChunk::size * 0.5f;

		return computeAxisCellRange( minPosition[0], maxPosition[0], halfWidth, width, range.minX, range.maxX ) &&
			computeAxisCellRange( minPosition[1], maxPosition[1], halfHeight, height, range.minY, range.maxY ) &&
			computeAxisCellRange( minPosition[2], maxPosition[2], halfDepth, depth, range.minZ, range.maxZ );
	}

	bool SpatialGrid::isInside( const vec3& position ) const {
		const float halfWidth  = width * GridChunk::size * 0.5f;
		const float halfHeight = height * GridChunk::size * 0.5f;
		const float halfDepth  = depth * GridChunk::size * 0.5f;

		return std::abs( position[0] ) < halfWidth && std::abs( position[1] ) < halfHeight && std::abs( position[2] ) < halfDepth;
	}

	bool World::addEntityToArchetype(entity entity_, Archetype* arch) {
		if ( arch == nullptr ) {
			std::cerr << "World::addEntityToArchetype: archetype is nullptr" << std::endl;
			return false;
		}

        id id_ = getId(entity_);

        if (id_ >= entityLocations.GetSize())
            entityLocations.Resize(id_ + 1);

        EntityLocation& location = entityLocations[id_];

        if (location.arch != nullptr) {
			std::cerr << "World::addEntityToArchetype: entity " << id_ << " already assigned to archetype" << std::endl;
			return false;
        }

		if ( arch->entityCount >= getArchetypeCapacity( arch ) ) {
			std::cerr << "World::addEntityToArchetype: archetype is full, entity " << id_ << " is not added" << std::endl;
			return false;
		}

        uint32_t index = arch->addEntity(entity_);
		if ( index == UINT32_MAX )
			return false;

        location.arch            = arch;
        location.index           = index;
		location.gridCellCounter = 0;
		location.isDirty         = true;
		return true;
    }

	bool World::removeEntity(entity entity_) {
        id id_ = getId(entity_);
		if ( id_ >= entityLocations.GetSize() )
			return false;

        EntityLocation& location = entityLocations[id_];

        Archetype* arch = location.arch;
        uint32_t index  = location.index;
		if ( arch == nullptr || index >= arch->entityCount || arch->entities[index] != entity_ ) {
			std::cerr << "World::removeEntity: entity " << id_ << " is not alive" << std::endl;
			return false;
		}

        entity moved = arch->removeEntity(index);

        if (moved != entity_) {
            id movedId = getId(moved);
            entityLocations[movedId].index = index;
			entityLocations[movedId].arch  = arch;
        }
		std::cout << "remove entity with id: " << id_ << std::endl;
        location.arch            = nullptr;
		location.gridCellCounter = 0;
		return true;
    }

	void World::searchCacheArchetypes( arch::componentMask requiredMask, arch::Archetype* cachedArchetypes[], uint32_t& cachedArchetypesNumber,
									   const uint32_t cachedArchetypesCapacity ) {
		cachedArchetypesNumber = 0;
		for( uint32_t i = 0; i < archetypes.GetSize(); ++i ) {
			arch::Archetype* arch = archetypes[i];

			if( (arch->mask & requiredMask) == requiredMask ) {
				if ( cachedArchetypesNumber >= cachedArchetypesCapacity ) {
					static bool isReported = false;               ///< Function is called every frame, so the message is printed once
					if ( !isReported ) {
						std::cerr << "World::searchCacheArchetypes: cache array is too small for all matched archetypes" << std::endl;
						isReported = true;
					}
					break;
				}

				cachedArchetypes[cachedArchetypesNumber] = arch;
				++cachedArchetypesNumber;
			}
		}
	}
}; // namespace GLVM::ecs::arch
