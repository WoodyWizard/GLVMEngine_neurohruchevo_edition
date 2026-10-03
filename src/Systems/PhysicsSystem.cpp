// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Systems/PhysicsSystem.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Archetypes/ItemArchetype.hpp"
#include "ComponentManager.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/MoveComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/ViewComponent.hpp"
#include "EntityManager.hpp"
#include "Event.hpp"
#include "Globals.hpp"
#include "VertexMath.hpp"
#include "Systems/DamageSystem.hpp"
#include "Components/AttackComponent.hpp"
#include "Components/FontComponent.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "Archetypes/ProjectileArchetype.hpp"
#include "Archetypes/StaticMeshArchetype.hpp"
#include "Archetypes/EnemyArchetype.hpp"

namespace GLVM::ecs
{
    /*! This update searching for refering to colliders entities and check their
     *  transform components for collision, and if collision detected check if
     *  backtracking entity had gravity component for call Gravity function.
     */
         
    void CPhysicsSystem::Update() 
    {
		namespace cm = GLVM::ecs::components;

		cachedArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( requiredMask, archView.cachedArchetypes, cachedArchetypesNumber );

		for( uint32_t x = 0; x < cachedArchetypesNumber; ++x ) {
			arch::Archetype* arch = archView.cachedArchetypes[x];

			componentsView.transformsView    = (ecs::components::transform*)archView.cachedArchetypes[x]->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			componentsView.movesView         = (ecs::components::move*)archView.cachedArchetypes[x]->
				components[arch::ComponentsIndices::MOVE_COMPONENT];
			componentsView.rigidBodiesView   = (ecs::components::rigidBody*)archView.cachedArchetypes[x]->
				components[arch::ComponentsIndices::RIGID_BODY_COMPONENT];
			componentsView.colliderFlagsView = (ecs::components::colliderFlags*)archView.cachedArchetypes[x]->
				components[arch::ComponentsIndices::COLLIDER_FLAGS_COMPONENT];

			float deltaTime = 5.5f * fDelta_Time_;
			for(unsigned int i = 0; i < arch->entityCount; ++i) {
				if( componentsView.transformsView && componentsView.colliderFlagsView &&
					componentsView.movesView && componentsView.rigidBodiesView ) {
					cm::transform& transformComponent = componentsView.transformsView[i];
					cm::move& move = componentsView.movesView[i];
//				cm::collider& collider = collidersView[i];
					cm::colliderFlags& colliderFlags = componentsView.colliderFlagsView[i];
					uint8_t isGroudCollisionMask = (0u << 0) | (1u << 1) | (0u << 2) | (0u << 3);
					/// On the ground move.gravity is already limited by the collision system to land on the surface
					if( colliderFlags.flags & isGroudCollisionMask ) {
						transformComponent.gravityAccumulator = 0.0f;
					}
					uint8_t isWallCollisionMask = (1u << 0) | (0u << 1) | (0u << 2) | (0u << 3);
					if( colliderFlags.flags & isWallCollisionMask ) {
						move.frameMovement = 0;
						uint8_t wallCollisionTurnOffMask = (0u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
						colliderFlags.flags &= wallCollisionTurnOffMask;
					}

					// u32 entity = arch->entities[i];
					// if( entity == 0 ) {
					// 	std::cout << "frame move: " << "x: " << move.frameMovement[0] << " y: " << move.frameMovement[1] <<
					// 		" z: " << move.frameMovement << std::endl;
					// }
					
					transformComponent.position += move.frameMovement;
					transformComponent.position += move.gravity;
					move.gravity       = 0.0f;
					move.frameMovement = 0.0f;
//				componentManager->RemoveComponent<cm::move>(entityRefMove);

					cm::rigidBody& rigidBody = componentsView.rigidBodiesView[i];
					if ( rigidBody.jumpAccumulator > 0.0f ) {
						rigidBody.jumpAccumulator -= deltaTime;
						vec3 jump = vec3{ 0.0f, 5.0f, 0.0f } * deltaTime;
						transformComponent.position += jump;
					}
				}
			}
		}
    }
}

