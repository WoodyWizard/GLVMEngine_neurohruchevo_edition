// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Systems/MovementSystem.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Archetypes/EnemyArchetype.hpp"
#include "Archetypes/ItemArchetype.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "Components/AnimationComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/MoveComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/ViewComponent.hpp"
#include "Event.hpp"
#include "VertexMath.hpp"
#include <cstdint>
#include <sys/types.h>

namespace GLVM::ecs
{
    CMovementSystem::CMovementSystem(core::CStack& inputStack) :
        inputStack(inputStack) {}
        
    void CMovementSystem::Update()
    {
		namespace cm   = GLVM::ecs::components;
		namespace arch = GLVM::ecs::arch;

		/// Only one player archetype is cached (capacity of the cache is 1)
		arch::world.searchCacheArchetypes( playerRequiredMask, &archView.playerCachedArchetype, playerArchetypesNumber, 1 );
		const uint32_t playersNumber = playerArchetypesNumber > 0 ? archView.playerCachedArchetype->entityCount : 0;
		if ( playersNumber > 0 ) {
			componentsView.playerMoves         = (ecs::components::move*)archView.playerCachedArchetype->
				components[arch::ComponentsIndices::MOVE_COMPONENT];
			componentsView.playerAnimation     = (ecs::components::animation*)archView.playerCachedArchetype->
				components[arch::ComponentsIndices::ANIMATION_COMPONENT];
			componentsView.playerViews         = (ecs::components::beholder*)archView.playerCachedArchetype->
				components[arch::ComponentsIndices::VIEW_COMPONENT];
			componentsView.playerColliderFlags = (ecs::components::colliderFlags*)archView.playerCachedArchetype->
				components[arch::ComponentsIndices::COLLIDER_FLAGS_COMPONENT];
			componentsView.playerRigidBody     = (ecs::components::rigidBody*)archView.playerCachedArchetype->
				components[arch::ComponentsIndices::RIGID_BODY_COMPONENT];
			componentsView.playerTransform     = (ecs::components::transform*)archView.playerCachedArchetype->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
		}

        const float cameraSpeed = 3.0f * deltaFrameTime;
        for(unsigned int i = 0; i < playersNumber; ++i) {
			const arch::entity entity = archView.playerCachedArchetype->entities[i];
			ecs::arch::EntityLocation& entityLocation = ecs::arch::world.entityLocations[ecs::arch::getId( entity )];
			cm::beholder*      playerView          = &componentsView.playerViews[i];
			cm::move*          playerMove          = &componentsView.playerMoves[i];
			cm::colliderFlags* playerColliderFlags = &componentsView.playerColliderFlags[i];
			cm::rigidBody*     playerRigidBody     = &componentsView.playerRigidBody[i];
			cm::animation*     playerAnimation     = &componentsView.playerAnimation[i];
			cm::transform*     playerTransform     = &componentsView.playerTransform[i];
			playerTransform->frameMovement = 0.0f;
            for(int n = 0; n < 6; ++n) {
				vec3 right;
				vec3 forward;
                switch(inputStack[n])
                {
                case core::EEvents::eMOVE_LEFT:
					right = CalculateVectorRL(*playerView);
					playerMove->frameMovement -= right * cameraSpeed;
					playerTransform->frameMovement = playerMove->frameMovement;
//					playerMove->frameMovement += right * cameraSpeed;
					playerAnimation->isAnimatedOnFrame = true;
					entityLocation.isDirty = true;
                    break;
                case core::EEvents::eMOVE_RIGHT:
					right = CalculateVectorRL(*playerView);
					playerMove->frameMovement += right * cameraSpeed;
					playerTransform->frameMovement = playerMove->frameMovement;
//					playerMove->frameMovement -= right * cameraSpeed;
					playerAnimation->isAnimatedOnFrame = true;
					entityLocation.isDirty = true;
                    break;
                case core::EEvents::eMOVE_BACKWARD:
                    forward = CalculateVectorFB(*playerView, g_eEvent);
					playerMove->frameMovement -= forward * cameraSpeed;
					playerTransform->frameMovement = playerMove->frameMovement;
//					playerMove->frameMovement += forward * cameraSpeed;
					playerAnimation->isAnimatedOnFrame = true;
					entityLocation.isDirty = true;
                    break;
                case core::EEvents::eMOVE_FORWARD:
//					std::cout << "forw: " << playerView->forward << std::endl;
					forward = CalculateVectorFB(*playerView, g_eEvent);
					playerMove->frameMovement += forward * cameraSpeed;
					playerTransform->frameMovement = playerMove->frameMovement;
//					playerMove->frameMovement -= forward * cameraSpeed;
					playerAnimation->isAnimatedOnFrame = true;
					entityLocation.isDirty = true;
                    break;
                case core::EEvents::eJUMP:
				{
					entityLocation.isDirty = true;
					uint8_t isGroudCollisionMask = (0u << 0) | (1u << 1) | (0u << 2) | (0u << 3);
					if ( playerColliderFlags->flags & isGroudCollisionMask ) {
						playerRigidBody->jumpAccumulator = 1.0f;
					}
				}
                    break;
                default:
                    break;
                }
            }
        }

		rigidBodyContainedArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( rigidBodyRequiredMask, archView.rigidBodyContainedArchetypesCache , rigidBodyContainedArchetypesNumber );

		for( uint32_t i0 = 0; i0 < rigidBodyContainedArchetypesNumber; ++i0 ) {
			arch::Archetype* currentArch = archView.rigidBodyContainedArchetypesCache[i0];
			componentsView.transforms  = (ecs::components::transform*)currentArch->components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			componentsView.rigidBodies = (ecs::components::rigidBody*)currentArch->components[arch::ComponentsIndices::RIGID_BODY_COMPONENT];
			componentsView.moves       = (ecs::components::move*)currentArch->components[arch::ComponentsIndices::MOVE_COMPONENT];
			componentsView.items       = (ecs::components::item*)currentArch->components[arch::ComponentsIndices::ITEM_COMPONENT];

			for( uint32_t i1 = 0; i1 < currentArch->entityCount; ++i1 ) {
				// const arch::entity entity = currentArch->entities[i1];
				// ecs::arch::EntityLocation entityLocation = ecs::arch::world.entityLocations[ecs::arch::getId( entity )];
				// entityLocation.isDirty = true;

				if( componentsView.items && !componentsView.items[i1].isActor )
					continue;
						
				cm::transform* rTransform_Component = &componentsView.transforms[i1];
				cm::rigidBody* rigidBodyComponennt  = &componentsView.rigidBodies[i1];
				cm::move*      moveComponent        = &componentsView.moves[i1];

				/*
				  Fall speed grows linearly with time in the air (gravityAccumulator, seconds) and is limited.
				  Displacement is scaled by frame time, so falling does not depend on FPS. The player model is
				  about 3 units tall and has mass 3, so it gets 20 units/s^2 - close to real gravity for a human.
				*/
				constexpr float gravityAccelerationPerMass = 20.0f / 3.0f;   ///< units / s^2 per unit of mass
				constexpr float terminalFallSpeed          = 30.0f;          ///< units / s
				rTransform_Component->gravityAccumulator += deltaFrameTime;
				float fallSpeed = gravityAccelerationPerMass * rigidBodyComponennt->fMass_ * rTransform_Component->gravityAccumulator;
				if ( fallSpeed > terminalFallSpeed )
					fallSpeed = terminalFallSpeed;

				moveComponent->gravity[1] -= fallSpeed * deltaFrameTime;
			}
		}
    }

    Vector<float, 3> CMovementSystem::CalculateVectorRL(components::beholder& beholder) {
        Vector<float, 3> normalizedVector = Normalize(Cross(CalculateVectorFB(beholder, g_eEvent), vec3{ 0.0f, -1.0f, 0.0 }));
        return normalizedVector;
    }

    Vector<float, 3> CMovementSystem::CalculateVectorFB(components::beholder& beholder,
                                                        core::CEvent& event) {
		/*
		  Camera forward direction on XZ plane. offset_X is the mouse movement (pixels) of the current frame,
		  the orbit camera applies the same yaw change later in this frame (Engine::SetViewMatrix).
		  Camera position is orbitRadius * (cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)) and
		  camera looks at the player, so horizontal forward is (-sin(yaw), 0, -cos(yaw)).
		*/
		const float predictedYaw = beholder.yaw + static_cast<float>(event.mousePointerPosition.offset_X) * components::kOrbitCameraSensitivity;
        Vector<float, 3> forward( -std::sin(predictedYaw), 0.0f, -std::cos(predictedYaw) );
        return forward;
    }
}


