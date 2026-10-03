// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Systems/EnemySystem.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Archetypes/EnemyArchetype.hpp"
#include "Archetypes/ProjectileArchetype.hpp"
#include "Components/ActorComponent.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "ArchetypeECS/ArchetypeEntityManager.hpp"
#include "Components/AnimationComponent.hpp"
#include "Components/DamageComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/ProjectileBundle.hpp"
#include "Texture.hpp"
#include <algorithm>
#include <cstdint>

namespace GLVM::ecs
{
	void EnemySystem::Update() {
		namespace arch = GLVM::ecs::arch;

		/// Only one archetype of every kind is cached (capacity of each cache is 1)
		playerArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( playerRequiredMask, &archView.playerCachedArchetype, playerArchetypesNumber, 1 );
		enemyArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( enemyRequiredMask, &archView.enemyCachedArchetype, enemyArchetypesNumber, 1 );
		projectileArchetypesNumber = 0;
		ecs::arch::world.searchCacheArchetypes( projectileRequiredMask, &archView.projectileArchetype, projectileArchetypesNumber, 1 );
		if ( playerArchetypesNumber == 0 || enemyArchetypesNumber == 0 )
			return;

		componentsView.playerTransforms = (ecs::components::transform*)archView.playerCachedArchetype->
			components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
		componentsView.enemyTransforms = (ecs::components::transform*)archView.enemyCachedArchetype->
			components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
		componentsView.enemyStates     = (ecs::components::state*)archView.enemyCachedArchetype->
			components[arch::ComponentsIndices::STATE_COMPONENT];
		componentsView.enemyAnimation  = (ecs::components::animation*)archView.enemyCachedArchetype->
			components[arch::ComponentsIndices::ANIMATION_COMPONENT];
		componentsView.enemies         = (ecs::components::enemy*)archView.enemyCachedArchetype->
			components[arch::ComponentsIndices::ENEMY_COMPONENT];

		/// Cooldown of every enemy decreases once per frame
		const float cameraSpeed = 5.5f * deltaFrameTime;
		for ( unsigned int i = 0; i < archView.enemyCachedArchetype->entityCount; ++i ) {
			components::enemy* enemyComponent = &componentsView.enemies[i];
			if ( enemyComponent->attackCooldown > 0 )
				enemyComponent->attackCooldown -= cameraSpeed;
		}

		for( uint32_t j = 0; j < archView.playerCachedArchetype->entityCount; ++j ) {
			components::transform* playerTransformComponent = &componentsView.playerTransforms[j];
			for ( unsigned int i = 0; i < archView.enemyCachedArchetype->entityCount; ++i ) {
				components::transform* enemyTransformComponent = &componentsView.enemyTransforms[i];
				components::state*     stateEnemyComponent     = &componentsView.enemyStates[i];
				components::animation* enemyAnimatin           = &componentsView.enemyAnimation[i];
				components::enemy*     enemyComponent          = &componentsView.enemies[i];

				enemyAnimatin->isAnimatedOnFrame = true;       ///< FIXME: DELETE CRINGE

				vec3 distance = playerTransformComponent->position - enemyTransformComponent->position;
				const float distanceLength = distance.Length();

				/// Attacking enemy chases the player on XZ plane with limited speed until it is on detect radius distance
				if ( stateEnemyComponent->state == core::States::ATTACK ) {
					const vec3 horizontalDistance = vec3( distance[0], 0.0f, distance[2] );
					const float horizontalDistanceLength = horizontalDistance.Length();
					if ( horizontalDistanceLength > enemyComponent->detectRadius ) {
						constexpr float enemyChaseSpeed = 5.5f;              ///< units per second
						const float step = std::min( enemyChaseSpeed * deltaFrameTime, horizontalDistanceLength - enemyComponent->detectRadius );
						enemyTransformComponent->position += horizontalDistance * (step / horizontalDistanceLength);
					}
				}

				if ( distanceLength <= enemyComponent->detectRadius ) {
					if( enemyComponent->attackCooldown <= 0 && projectileArchetypesNumber > 0 ) {
						ecs::components::MeshHandle meshHandle{};
						const u32 sphereMeshHandleIndex = 2;
						if ( meshHandlers.GetSize() > 2 )
							meshHandle = meshHandlers[sphereMeshHandleIndex];

						ecs::TextureHandle textureHandle{};
						const u32 grayTextureHandle = 2;
						if ( textureHandlers.GetSize() > 2 )
							textureHandle = textureHandlers[grayTextureHandle];

						const components::material material = { .diffuseTextureID_ = textureHandle,
							.specularTextureID_ = textureHandle, .ambient = { 0.05f, 0.05f, 0.05f },
							.shininess = 128.0f * 0.078125f };

						const components::damage damage = { .maximumDamage = 40, .minimumDamage = 20, .criticalHitRate = 0, .criticalModifier = 0 };

						ecs::arch::ArchetypeEntityManager* archEntityManager = ecs::arch::ArchetypeEntityManager::getInstance();
						ecs::arch::entity projectileEntity = archEntityManager->createEntity();
						if ( ecs::arch::world.addEntityToArchetype( projectileEntity, archView.projectileArchetype ) ) {
							const ecs::arch::EntityLocation& projectileLocation = ecs::arch::world.entityLocations[ecs::arch::getId( projectileEntity )];
							arch::ProjectileArchetype* projectileArch = static_cast<arch::ProjectileArchetype*>(projectileLocation.arch);
							const u32 projectileIndex = projectileLocation.index;
							ecs::components::health& projectileHealth = projectileArch->heath[projectileIndex];
							projectileHealth.randarable = false;

							/// Projectile is spawned near the enemy and flies to the player (direction is normalized inside)
							core::CreateProjectile(enemyTransformComponent->position,
												   playerTransformComponent->position - enemyTransformComponent->position,
												   meshHandle,
												   material,
												   damage,
												   ecs::arch::getId( archView.enemyCachedArchetype->entities[i] ),
												   projectileLocation);

							if ( soundEngine )
								soundEngine->CreateSoundSample( "../laser2.wav", 5, 22050, 0.05 );
						} else {
							archEntityManager->removeEntity( projectileEntity );                   ///< Projectile archetype is full, release the id
						}
						enemyComponent->attackCooldown = 15.0f;
					}

					stateEnemyComponent->state = core::States::ATTACK;
				}
			}
		}
	}
} // namespace GLVM::ecs
