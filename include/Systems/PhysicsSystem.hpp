// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef PHYSICS_SYSTEM
#define PHYSICS_SYSTEM

#include "Components/ColliderComponent.hpp"
#include "../ComponentManager.hpp"
#include "../Event.hpp"
#include "ISystem.hpp"
#include "Components/TransformComponent.hpp"
#include "Vector.hpp"
#include "EventsStack.hpp"
#include "Components/ViewComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs
{
    class CPhysicsSystem : public ISystem
    {
    public:
        float fDelta_Time_ = 0.0f;
        core::CStack& Input_Stack_;

		uint32_t cachedArchetypesNumber = 0;
		struct ArchView {
			arch::Archetype* cachedArchetypes[32];
		} archView;
		
		struct ComponentsView {
			components::transform*     transformsView    = nullptr;
			components::move*          movesView         = nullptr;
			components::rigidBody*     rigidBodiesView   = nullptr;
			components::colliderFlags* colliderFlagsView = nullptr;
		} componentsView;

		arch::componentMask requiredMask =
			(1ul << arch::ComponentsIndices::TRANSFORM_COMPONENT)  |
			(1ul << arch::ComponentsIndices::MOVE_COMPONENT)       |
			(1ul << arch::ComponentsIndices::RIGID_BODY_COMPONENT) |
			(1ul << arch::ComponentsIndices::COLLIDER_COMPONENT);
		
        CPhysicsSystem(core::CStack& _input_Stack) : Input_Stack_(_input_Stack) {}
        
        ///< Set Y-axis of transform component of backtracking entity to upper Y-axis of ground entity.
        
        void Gravity();

        /*! This update searching for refering to colliders entities and check their
         *  transform components for collision, and if collision detected check if
         *  backtracking entity had gravity component for call Gravity function.
         */
         
        void Update() override;
        void Repel(components::transform& _transform_Component,
                   float& _fDelta_Time,
                   components::beholder& _view_Component,
                   core::CEvent& _event);
    };
}

#endif
