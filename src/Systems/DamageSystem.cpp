// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Systems/DamageSystem.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "ArchetypeECS/ArchetypeEntityManager.hpp"
#include "Components/AttackComponent.hpp"
#include "Components/FontComponent.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "Archetypes/ProjectileArchetype.hpp"
#include "Archetypes/StaticMeshArchetype.hpp"
#include "Archetypes/EnemyArchetype.hpp"
#include <string>

namespace GLVM::ecs
{
	void DamageSystem::Update() {
		namespace cm = GLVM::ecs::components;

		arch::SpatialGrid& spatialGrid = arch::world.spatialGrid;
		
		cachedAttackableArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( attackableRequiredMask, archView.cachedAttackableArchetypes, cachedAttackableArchetypesNumber );

		for( uint32_t x = 0; x < cachedAttackableArchetypesNumber; ++x ) {
			arch::Archetype* arch = archView.cachedAttackableArchetypes[x];
			componentsView.attackableAttacks = (ecs::components::attack*)arch->components[arch::ComponentsIndices::ATTACK_COMPONENT];
			componentsView.attackableHealth  = (ecs::components::health*)arch->components[arch::ComponentsIndices::HEALTH_COMPONENT];
			componentsView.attackableFonts   = (ecs::components::font*)arch->components[arch::ComponentsIndices::FONT_COMPONENT];
			
			if( !componentsView.attackableHealth || !componentsView.attackableAttacks || !componentsView.attackableFonts )
				continue;

			const bool isPlayerArchetype = arch::matchesRequiredMask( arch->mask, playerRequiredMask );

			/// Iterate from the end: swap-remove moves the last entity into the slot of the removed one, and the last entity is already processed.
			for ( unsigned int i = arch->entityCount; i-- > 0; ) {
				arch::entity entity = arch->entities[i];
				cm::health& healthComponent = componentsView.attackableHealth[i];
				cm::attack& attackComponent = componentsView.attackableAttacks[i];

				const float receivedDamage = attackComponent.damage;
				attackComponent.damage = 0;
				if ( receivedDamage > 0.0f ) {
					healthComponent.currentHealth -= receivedDamage;

					/// Show received damage above the entity. Text floats up and is hidden after a while (see fonts loop below).
					cm::font& fontComponent = componentsView.attackableFonts[i];
					fontComponent.font_string.clear();
					const std::string damageText = std::to_string( static_cast<int>(receivedDamage) );
					for ( const char symbol : damageText )
						fontComponent.font_string.Push( symbol );
					fontComponent.lifeTime  = 0;
					fontComponent.removeble = true;
				}

				if ( healthComponent.currentHealth <= 0 ) {
					/// Player entity is never removed: other systems and inventory keep references on it. There is no game over state yet.
					if ( isPlayerArchetype ) {
						healthComponent.currentHealth = 0;
						if ( !isPlayerDeathReported ) {
							std::cout << "Player is dead" << std::endl;
							isPlayerDeathReported = true;
						}
						continue;
					}

					ecs::arch::ArchetypeEntityManager* archEntityManager = ecs::arch::ArchetypeEntityManager::getInstance();
					ecs::arch::EntityLocation& entityLocation = ecs::arch::world.entityLocations[ecs::arch::getId( entity )];
					if( entityLocation.gridCellCounter > 0 ) {
						for( u32 i2 = 0; i2 < entityLocation.gridCellCounter && i2 < entityLocation.maxGridCellNumber; ++i2 ) {
							const vec3 entityCurrentGridCell = entityLocation.gridCellIndicies[i2];
							u32 z = entityCurrentGridCell[0];
							u32 y = entityCurrentGridCell[1];
							u32 x = entityCurrentGridCell[2];
							if ( z >= spatialGrid.depth || y >= spatialGrid.height || x >= spatialGrid.width )
								continue;

							core::vector<u32>& cellEntities = spatialGrid.grid[z][y][x].entities;
							u32 cellEntityIndex = entityLocation.cellEntityIndices[i2];
							if ( cellEntityIndex >= cellEntities.GetSize() )
								continue;

							const u32 lastElementIndex = cellEntities.GetSize() - 1;
							if( lastElementIndex > cellEntityIndex ) {       ///< Swap case
								const u32 swapedEntity = cellEntities[lastElementIndex];
								cellEntities[cellEntityIndex] = swapedEntity;
								cellEntities.Remove( lastElementIndex );

								ecs::arch::EntityLocation& swapedEntityLocation = ecs::arch::world.entityLocations[ecs::arch::getId( swapedEntity )];
								for( u32 i3 = 0; i3 < swapedEntityLocation.gridCellCounter; ++i3 ) {
									const vec3 swapedEntityCurrentGridCell = swapedEntityLocation.gridCellIndicies[i3];
									if( swapedEntityCurrentGridCell == entityCurrentGridCell ) {
										swapedEntityLocation.cellEntityIndices[i3] = cellEntityIndex;
									}
								}
							} else {   ///< Remove last case
								cellEntities.Remove( cellEntityIndex );
							}
						}
						entityLocation.gridCellCounter = 0;
//						entityLocation.isDirty         = true;     TODO: Have to make it work
					}

					archEntityManager->removeEntity( entity );
					arch::world.removeEntity( entity );
				}
			}
		}

		cachedFontArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( fontRequiredMask, archView.cachedFontArchetypes, cachedFontArchetypesNumber );
		
		for( uint32_t x = 0; x < cachedFontArchetypesNumber; ++x ) {
			arch::Archetype* arch = archView.cachedFontArchetypes[x];
			componentsView.fonts = (ecs::components::font*)arch->components[arch::ComponentsIndices::FONT_COMPONENT];

			for ( unsigned int i = 0; i < arch->entityCount; ++i ) {
				if( componentsView.fonts ) {
					cm::font& fontComponent = componentsView.fonts[i];
					if ( fontComponent.removeble )
						fontComponent.lifeTime += deltaTime;

					/// Hide removable text (received damage) after its life time
					constexpr float fontMaximumLifeTime = 1.5f;
					if ( fontComponent.removeble && fontComponent.lifeTime >= fontMaximumLifeTime ) {
						fontComponent.font_string.clear();
						fontComponent.lifeTime  = 0.0f;
						fontComponent.removeble = false;
					}
				}
			}
		}
	}
} // namespace GLVM::ecs
