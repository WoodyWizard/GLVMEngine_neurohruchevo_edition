// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef COMMON_FUNCTIONS_HPP
#define COMMON_FUNCTIONS_HPP

#include "VertexMath.hpp"
#include "Components/VertexComponent.hpp"
#include "Globals.hpp"
#include "typenames.hpp"
#include "ComponentsFullSet.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Archetypes/ProjectileArchetype.hpp"
#include "ArchetypeECS/ArchetypeEntityManager.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"

namespace GLVM::core {
	bool BoxCollider(
		const vec3 backtrackingPosition,
		const vec3 comparedPosition,
		const float backtrackingScale,
		const float comparedScale,
		const core::MeshAxisMaxAbsoluteValues& backtrackingMeshAxisMaxAbsoluteValues,
		const core::MeshAxisMaxAbsoluteValues& comparedMeshAxisMaxAbsoluteValues);

	/// Compute left bottom back (minPoint) and right upper front (maxPoint) corners of entity bounding box.
	void computeBoxCornerBounds(
		const core::MeshAxisMaxAbsoluteValues& entityChunkBounds,
		const vec3& entityPosition,
		const float scale,
		vec3& minPoint,
		vec3& maxPoint );
	
	template< typename T >
	bool isExist( const core::vector<T>& array, const T& element ) {
		for( u32 i0 = 0; i0 < array.GetSize(); ++i0 ) {
			if( element == array[i0] )
				return true;
		}

		return false;
	}

	void setMeshBounds( MeshAxisLimitingValues meshAxisLimitingValues );
	/// Projectile is spawned at projectileSpawnOffset units from origin position along the direction.
	constexpr float projectileSpawnOffset = 1.0f;
	/// Seconds of flight after which a projectile is destroyed
	constexpr float projectileLifeTime    = 5.0f;

	void CreateProjectile(const vec3& originPosition,
						  const vec3& direction,
						  const ecs::components::MeshHandle& meshHandle,
						  const ecs::components::material& material,
						  const ecs::components::damage& damage,
						  const unsigned int ownerId,
						  const ecs::arch::EntityLocation& projectileLocation);
}; ///< namespace GLVM::core

#endif
