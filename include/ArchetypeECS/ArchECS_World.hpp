#ifndef ARCH_ECS_WORLD_HPP
#define ARCH_ECS_WORLD_HPP

#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "ArchetypeECS/ArchECS_Utils.hpp"
#include "Vector.hpp"
#include "typenames.hpp"
#include <cstddef>

namespace GLVM::ecs::arch {
	struct GridChunk {
		vec3 position;
		constexpr static float size = 32;
		core::vector<u32> entities;
	};

	/// Inclusive range of grid cells: grid[z][y][x] for x in [minX, maxX] and so on.
	struct GridCellRange {
		u32 minX = 0;
		u32 minY = 0;
		u32 minZ = 0;
		u32 maxX = 0;
		u32 maxY = 0;
		u32 maxZ = 0;
	};

	struct SpatialGrid {
		static const u32 width  = 32;
		static const u32 height = 32;
		static const u32 depth  = 32;
		GridChunk grid[width][height][depth];

		/// Convert world space box into a range of grid cells. Parts of the box outside of the grid are clamped.
		/// Returns false if the box is completely outside of the grid or has not finite coordinates.
		bool computeCellRange( const vec3& minPosition, const vec3& maxPosition, GridCellRange& range ) const;
		/// Is world space position inside of the grid bounds
		bool isInside( const vec3& position ) const;
	};
	
	struct World {
		World();
		~World();

		SpatialGrid spatialGrid;
		core::vector<Archetype*> archetypes;
		core::vector<EntityLocation> entityLocations;

		/// Returns false if entity can not be added (archetype is full or entity already has an archetype).
		bool addEntityToArchetype(entity entity_, Archetype* arch);
		/// Returns false if entity is not alive in any archetype (already removed or stale handle).
		bool removeEntity(entity entity_);
		/// Cache up to cachedArchetypesCapacity archetypes that contain all components of requiredMask.
		/// cachedArchetypesNumber is reset and receives the number of cached archetypes.
		void searchCacheArchetypes( arch::componentMask requiredMask, arch::Archetype* cachedArchetypes[], uint32_t& cachedArchetypesNumber,
									const uint32_t cachedArchetypesCapacity );
		template< std::size_t N >
		void searchCacheArchetypes( arch::componentMask requiredMask, arch::Archetype* (&cachedArchetypes)[N], uint32_t& cachedArchetypesNumber ) {
			searchCacheArchetypes( requiredMask, cachedArchetypes, cachedArchetypesNumber, static_cast<uint32_t>(N) );
		}
	};

	/// Real number of entities that archetype can hold (size of its component arrays).
	uint32_t getArchetypeCapacity( const Archetype* arch );

	extern World world;
}; // namespace GLVM::ecs::arch


#endif
