// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef MOVEMENT_SYSTEM_HPP
#define MOVEMENT_SYSTEM_HPP

#include "Components/ColliderFlagsComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Event.hpp"
#include "Components/TransformComponent.hpp"
#include "ISoundEngine.hpp"
#include "Vector.hpp"
#include "Components/MoveComponent.hpp"
#include "ComponentManager.hpp"
#include "ISystem.hpp"
#include "VertexMath.hpp"
#include "Components/ViewComponent.hpp"
#include "EventsStack.hpp"
#include "Globals.hpp"
#include "EntityManager.hpp"
#include "ComponentManager.hpp"
#include "ISoundEngine.hpp"
#include "Components/SpotLightComponent.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Components/ItemComponent.hpp"
#include "ArchetypeECS/ArchetypeEntityManager.hpp"
#include "Archetypes/PlayerArchetype.hpp"

namespace GLVM::ecs
{
	class CMovementSystem : public ISystem
	{
	public:
		float deltaFrameTime = 0.0f;
        core::CStack& inputStack;

		uint32_t playerArchetypesNumber = 0;
		uint32_t rigidBodyContainedArchetypesNumber = 0;
		struct MovementArchView {
			arch::Archetype* playerCachedArchetype = nullptr;
			arch::Archetype* rigidBodyContainedArchetypesCache[32];
		} archView;

		struct MovementComponentsView {
			ecs::components::move*          playerMoves         = nullptr;
			ecs::components::beholder*      playerViews         = nullptr;
			ecs::components::colliderFlags* playerColliderFlags = nullptr;
			ecs::components::rigidBody*     playerRigidBody     = nullptr;
			ecs::components::animation*     playerAnimation     = nullptr;
			ecs::components::transform*     playerTransform     = nullptr;

			/// Components related to archetypes contains Rigis
			ecs::components::transform* transforms   = nullptr;
			ecs::components::rigidBody* rigidBodies  = nullptr;
			ecs::components::move*      moves        = nullptr;
			ecs::components::item*      items        = nullptr;
		} componentsView;
		
		arch::componentMask playerRequiredMask =
			(1ull << ecs::arch::ComponentsIndices::PLAYER_TAG_COMPONENT);
		arch::componentMask rigidBodyRequiredMask =
			(1ul << arch::ComponentsIndices::TRANSFORM_COMPONENT)  |
			(1ul << arch::ComponentsIndices::RIGID_BODY_COMPONENT) |
			(1ul << arch::ComponentsIndices::MOVE_COMPONENT);
        
        CMovementSystem( core::CStack& inputStack );

		void Update();
        Vector<float, 3> CalculateVectorRL(components::beholder& beholder);
        Vector<float, 3> CalculateVectorFB(components::beholder& beholder,
                                           core::CEvent& event);
	};
}

#endif

