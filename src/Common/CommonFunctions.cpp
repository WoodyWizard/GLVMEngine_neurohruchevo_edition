// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Common/CommonFunctions.hpp"
#include "Components/ColliderComponent.hpp"

namespace GLVM::core {
	bool BoxCollider(const vec3 backtrackingPosition,
					 const vec3 comparedPosition,
					 const float backtrackingScale,
					 const float comparedScale,
					 const core::MeshAxisMaxAbsoluteValues& backtrackingMeshAxisMaxAbsoluteValues,
					 const core::MeshAxisMaxAbsoluteValues& comparedMeshAxisMaxAbsoluteValues)
	{
        if(backtrackingPosition[0] + (backtrackingMeshAxisMaxAbsoluteValues.origin_offset_x + backtrackingMeshAxisMaxAbsoluteValues.absolute_x) * backtrackingScale  >
		   comparedPosition[0] + (comparedMeshAxisMaxAbsoluteValues.origin_offset_x - comparedMeshAxisMaxAbsoluteValues.absolute_x) * comparedScale &&
           backtrackingPosition[0] + (backtrackingMeshAxisMaxAbsoluteValues.origin_offset_x - backtrackingMeshAxisMaxAbsoluteValues.absolute_x) * backtrackingScale  <
		   comparedPosition[0] + (comparedMeshAxisMaxAbsoluteValues.origin_offset_x + comparedMeshAxisMaxAbsoluteValues.absolute_x) * comparedScale &&
           backtrackingPosition[1] + (backtrackingMeshAxisMaxAbsoluteValues.origin_offset_y + backtrackingMeshAxisMaxAbsoluteValues.absolute_y) * backtrackingScale  >
		   comparedPosition[1] + (comparedMeshAxisMaxAbsoluteValues.origin_offset_y - comparedMeshAxisMaxAbsoluteValues.absolute_y) * comparedScale &&
           backtrackingPosition[1] + (backtrackingMeshAxisMaxAbsoluteValues.origin_offset_y - backtrackingMeshAxisMaxAbsoluteValues.absolute_y) * backtrackingScale  <
		   comparedPosition[1] + (comparedMeshAxisMaxAbsoluteValues.origin_offset_y + comparedMeshAxisMaxAbsoluteValues.absolute_y) * comparedScale &&
           backtrackingPosition[2] + (backtrackingMeshAxisMaxAbsoluteValues.origin_offset_z + backtrackingMeshAxisMaxAbsoluteValues.absolute_z) * backtrackingScale  >
		   comparedPosition[2] + (comparedMeshAxisMaxAbsoluteValues.origin_offset_z - comparedMeshAxisMaxAbsoluteValues.absolute_z) * comparedScale &&
           backtrackingPosition[2] + (backtrackingMeshAxisMaxAbsoluteValues.origin_offset_z - backtrackingMeshAxisMaxAbsoluteValues.absolute_z) * backtrackingScale  <
		   comparedPosition[2] + (comparedMeshAxisMaxAbsoluteValues.origin_offset_z + comparedMeshAxisMaxAbsoluteValues.absolute_z) * comparedScale) {
				return true;
		}
        
		return false;
	}

	void computeBoxCornerBounds(
		const core::MeshAxisMaxAbsoluteValues& entityChunkBounds,
		const vec3& entityPosition,
		const float scale,
		vec3& minPoint,
		vec3& maxPoint ) {
		const float halfWidht  = entityChunkBounds.absolute_x * scale;
		const float halfHeight = entityChunkBounds.absolute_y * scale;
		const float halfDepth  = entityChunkBounds.absolute_z * scale;

		vec3 position = entityPosition;
		minPoint = position + vec3( -halfWidht, -halfHeight, -halfDepth );  ///< left bottom back
		maxPoint = position + vec3( halfWidht, halfHeight, halfDepth );     ///< right upper front
	}

	void setMeshBounds( MeshAxisLimitingValues meshAxisLimitingValues ) {
		allMeshMaxAbsoluteValues.Push({});
		
		allMeshMaxAbsoluteValues[allMeshMaxAbsoluteValues.GetSize() - 1].absolute_x = (meshAxisLimitingValues.highest_x - meshAxisLimitingValues.lowest_x) / 2.0f;
		allMeshMaxAbsoluteValues[allMeshMaxAbsoluteValues.GetSize() - 1].absolute_y = (meshAxisLimitingValues.highest_y - meshAxisLimitingValues.lowest_y) / 2.0f;
		allMeshMaxAbsoluteValues[allMeshMaxAbsoluteValues.GetSize() - 1].absolute_z = (meshAxisLimitingValues.highest_z - meshAxisLimitingValues.lowest_z) / 2.0f;

		allMeshMaxAbsoluteValues[allMeshMaxAbsoluteValues.GetSize() - 1].origin_offset_x = (meshAxisLimitingValues.highest_x + meshAxisLimitingValues.lowest_x) / 2.0f;
		allMeshMaxAbsoluteValues[allMeshMaxAbsoluteValues.GetSize() - 1].origin_offset_y = (meshAxisLimitingValues.highest_y + meshAxisLimitingValues.lowest_y) / 2.0f;
		allMeshMaxAbsoluteValues[allMeshMaxAbsoluteValues.GetSize() - 1].origin_offset_z = (meshAxisLimitingValues.highest_z + meshAxisLimitingValues.lowest_z) / 2.0f;
	}

	void CreateProjectile(const vec3& originPosition,
						  const vec3& direction,
						  const ecs::components::MeshHandle& meshHandle,
						  const ecs::components::material& material,
						  const ecs::components::damage& damage,
						  const unsigned int ownerId,
						  const ecs::arch::EntityLocation& projectileLocation) {
		ecs::arch::ProjectileArchetype* projectileArch = static_cast<ecs::arch::ProjectileArchetype*>(projectileLocation.arch);
		const uint32_t projectileIndex = projectileLocation.index;

		ecs::components::mesh* projectileMesh = &projectileArch->meshes[projectileIndex];
		projectileMesh->handle = meshHandle;

		ecs::arch::ProjectileBundle* projectileBundle = &projectileArch->projectileBundles[projectileIndex];
		projectileBundle->material            = material;
		projectileBundle->damage              = damage;
		projectileBundle->projectile          = {};
		projectileBundle->projectile.owner    = ownerId;
		projectileBundle->projectile.lifeTime = projectileLifeTime;

		/// Slot can keep data of a previously removed entity, so every component is reset.
		ecs::components::health* projectileHealth = &projectileArch->heath[projectileIndex];
		projectileHealth->maxHealth     = 100;
		projectileHealth->currentHealth = 100;
		projectileArch->attacks[projectileIndex].damage      = 0.0f;
		projectileArch->colliderFlags[projectileIndex].flags = 0;
		projectileArch->colliders[projectileIndex].colliders.clear();
		projectileArch->rotations[projectileIndex]           = {};

		ecs::components::font& projectileFont = projectileArch->fonts[projectileIndex];
		projectileFont.font_string.clear();
		projectileFont.lifeTime  = 0.0f;
		projectileFont.removeble = false;

		/// Direction can be not normalized (for example vector from enemy to player)
		vec3 normalizedDirection = Normalize( direction );
		vec3 spawnPosition       = originPosition;
		ecs::components::transform* rTransformProjectile = &projectileArch->transforms[projectileIndex];
		*rTransformProjectile = {};
		rTransformProjectile->scale    = 0.3f;
		rTransformProjectile->forward  = normalizedDirection;
		rTransformProjectile->position = spawnPosition + normalizedDirection * projectileSpawnOffset;
	}
}; ///< namespace GLVM::core
