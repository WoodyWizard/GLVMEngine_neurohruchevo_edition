// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "ArchetypeECS/ArchECS_Types.hpp"
#include "ArchetypeECS/ArchECS_Utils.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Archetypes/EnemyArchetype.hpp"
#include "Components/ActorComponent.hpp"
#include "Components/AttackComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/ControllerComponent.hpp"
#include "Components/DamageComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/PointLightComponent.hpp"
#include "Components/ProjectileBundle.hpp"
#include "Components/ProjectileComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Texture.hpp"
#include "VertexMath.hpp"
#include <Systems/ProjectileSystem.hpp>
#include <cstdint>
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "Archetypes/ProjectileArchetype.hpp"
#include "ArchetypeECS/ArchetypeEntityManager.hpp"
#include "Components/ItemComponent.hpp"

namespace GLVM::ecs
{
    CProjectileSystem::CProjectileSystem(core::CStack& inputStack) : inputStack (inputStack)
    {}
    
    void CProjectileSystem::Update()
    {
		namespace cm = GLVM::ecs::components;
		namespace arch = GLVM::ecs::arch;
		
        float cameraSpeed = 5.5f * deltaFrameTime;

		/// Only one player archetype and one projectile archetype are cached (capacity of each cache is 1)
		playerArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( playerRequiredMask, &archView.playerCachedArchetype, playerArchetypesNumber, 1 );

		projectileArchetypesNumber = 0;
		ecs::arch::world.searchCacheArchetypes( projectileRequiredMask, &archView.projectileArchetype, projectileArchetypesNumber, 1 );
		if ( projectileArchetypesNumber == 0 )
			return;

        if(projectileCooldown > 0)
            projectileCooldown -= cameraSpeed;

		/// Iterate on every player and create projectile if "LMB pressed" event found
		const uint32_t playersNumber = playerArchetypesNumber > 0 ? archView.playerCachedArchetype->entityCount : 0;
		if ( playersNumber > 0 ) {
			componentsView.playerTransforms = (ecs::components::transform*)archView.playerCachedArchetype->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			componentsView.playerViews      = (ecs::components::beholder*)archView.playerCachedArchetype->
				components[arch::ComponentsIndices::VIEW_COMPONENT];
		}

        for(unsigned int i = 0; i < playersNumber; ++i) {
			cm::beholder*  playerView      = &componentsView.playerViews[i];
			cm::transform* playerTransform = &componentsView.playerTransforms[i];
			const arch::entity playerEntity = archView.playerCachedArchetype->entities[i];
			if(!isInventoryOpened && projectileCooldown <= 0 &&
			   inputStack.SearchElement(core::EEvents::eMOUSE_LEFT_BUTTON) == core::EEvents::eMOUSE_LEFT_BUTTON) {
				ecs::components::MeshHandle meshHandle{};
				const u32 sphereMeshHandleIndex = 1;
				if ( meshHandlers.GetSize() > 1 )
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
				if ( !ecs::arch::world.addEntityToArchetype( projectileEntity, archView.projectileArchetype ) ) {
					archEntityManager->removeEntity( projectileEntity );                   ///< Projectile archetype is full, release the id
					continue;
				}

				const ecs::arch::EntityLocation& projectileLocation = ecs::arch::world.entityLocations[ecs::arch::getId( projectileEntity )];
				arch::ProjectileArchetype* projectileArch = static_cast<arch::ProjectileArchetype*>(projectileLocation.arch);
				const u32 projectileIndex = projectileLocation.index;
				ecs::components::health& projectileHealth = projectileArch->heath[projectileIndex];
				projectileHealth.randarable = false;

				core::CreateProjectile(playerTransform->position,
									   playerView->forward,
									   meshHandle,
									   material,
									   damage,
									   ecs::arch::getId( playerEntity ),
									   projectileLocation);

				if ( soundEngine )
					soundEngine->CreateSoundSample( "../laser2.wav", 5, 22050, 0.05 );
				projectileCooldown = 2.0;
			}
        }

		componentsView.projectileTransforms    = (ecs::components::transform*)archView.projectileArchetype->
			components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
		componentsView.projectileColliderFlags = (ecs::components::colliderFlags*)archView.projectileArchetype->
			components[arch::ComponentsIndices::COLLIDER_FLAGS_COMPONENT];
		componentsView.projectileColliders     = (ecs::components::collider*)archView.projectileArchetype->
			components[arch::ComponentsIndices::COLLIDER_COMPONENT];
		componentsView.projectileBundles     = (arch::ProjectileBundle*)archView.projectileArchetype->
			components[arch::ComponentsIndices::PROJECTILE_BUNDLE_COMPONENT];
		componentsView.projectileHealth     = (ecs::components::health*)archView.projectileArchetype->
			components[arch::ComponentsIndices::HEALTH_COMPONENT];
		componentsView.projectileAttacks     = (ecs::components::attack*)archView.projectileArchetype->
			components[arch::ComponentsIndices::ATTACK_COMPONENT];

		/*
		  Update position and life time of every projectile. Projectiles that fly too long or leave the world
		  are destroyed: health is set to 0 and DamageSystem removes the entity.
		*/
		const arch::SpatialGrid& spatialGrid = arch::world.spatialGrid;
        for(unsigned int x = 0; x < archView.projectileArchetype->entityCount; ++x) {
            cm::transform* projectileTransform = &componentsView.projectileTransforms[x];
			projectileTransform->position += Normalize(projectileTransform->forward) * cameraSpeed * 2.5;

			cm::projectile& projectileComponent = componentsView.projectileBundles[x].projectile;
			projectileComponent.lifeTime -= deltaFrameTime;
			if ( projectileComponent.lifeTime <= 0.0f || !spatialGrid.isInside( projectileTransform->position ) )
				componentsView.projectileHealth[x].currentHealth = 0;
		}

		/*
		  Iterate every projectile and check its collisions (computed by CollisionSystem). Projectile is destroyed
		  on a hit of any obstacle (ground, wall, actor) except its owner, other projectiles and items stored in inventory.
		  Damage info is updated if collided entity has health and attack components.
		*/
		const arch::componentMask damageableMask = (1ul << arch::ComponentsIndices::HEALTH_COMPONENT) |
			(1ul << arch::ComponentsIndices::ATTACK_COMPONENT);
		const arch::componentMask projectileTagMask = (1ul << arch::ComponentsIndices::PROJECTILE_TAG_COMPONENT);
		const arch::componentMask itemMask          = (1ul << arch::ComponentsIndices::ITEM_COMPONENT);
        for(unsigned int i = 0; i < archView.projectileArchetype->entityCount; ++i) {
			cm::health*         projectileHealth   = &componentsView.projectileHealth[i];
			const cm::damage*   projectileDamage   = &componentsView.projectileBundles[i].damage;
			const unsigned int  projectileOwner    = componentsView.projectileBundles[i].projectile.owner;
			const cm::collider* projectileCollider = &componentsView.projectileColliders[i];
			for ( unsigned int j = 0; j < projectileCollider->colliders.GetSize(); ++j ) {
				const unsigned int collidedEntity = projectileCollider->colliders[j];
				if ( collidedEntity == projectileOwner || collidedEntity >= arch::world.entityLocations.GetSize() )
					continue;

				const arch::EntityLocation& collidedEntityLocation = arch::world.entityLocations[collidedEntity];
				const arch::Archetype* collidedArch = collidedEntityLocation.arch;
				if ( collidedArch == nullptr || arch::matchesRequiredMask( collidedArch->mask, projectileTagMask ) )
					continue;

				if ( arch::matchesRequiredMask( collidedArch->mask, itemMask ) ) {
					const cm::item* items = (const cm::item*)collidedArch->components[arch::ComponentsIndices::ITEM_COMPONENT];
					if ( !items[collidedEntityLocation.index].isActor )
						continue;
				}

				if( arch::matchesRequiredMask( collidedArch->mask, damageableMask ) ) {
					ecs::components::attack* attacks = (ecs::components::attack*)collidedArch->components[arch::ComponentsIndices::ATTACK_COMPONENT];
					attacks[collidedEntityLocation.index].damage += projectileDamage->maximumDamage;
				}

				projectileHealth->currentHealth = 0;
				break;
			}
        }
    }
}
