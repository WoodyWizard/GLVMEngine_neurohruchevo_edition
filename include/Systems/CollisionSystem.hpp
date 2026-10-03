// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef COLLISION_SYSTEM
#define COLLISION_SYSTEM

#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "ISystem.hpp"
#include "Event.hpp"
#include "Components/MoveComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "VertexMath.hpp"
#include <cstdint>
#include "Common/CommonFunctions.hpp"
#include "Globals.hpp"

namespace GLVM::ecs
{
	class CCollisionSystem : public ISystem
	{   
	public:
		float fDelta_Time_              = 0.0f;
		bool isInventoryOpened          = false;
		bool* isItemDraged              = nullptr;
		bool isLeftMouseButtonPressed   = false;
		bool* isLeftMouseButtonReleased = nullptr;
        core::CStack& Input_Stack_;
		arch::Archetype* cachedArchetypes[32];
		uint32_t cachedArchetypesNumber = 0;
		core::vector<u32> collectedEntities;          ///< Reused buffer with entities collected from grid cells

		struct CollisionComponentsView {
			components::transform* backtrackingTransforms        = nullptr;
			components::collider*  backtrackingColliders         = nullptr;
			components::colliderFlags* backtrackingColliderFlags = nullptr;
			components::mesh* backtrackingMeshes                 = nullptr;
			components::move* backtrackingMove = nullptr;
		} view;
		
		arch::componentMask	requiredMask = (1ul << arch::ComponentsIndices::COLLIDER_COMPONENT) |
			(1ul << arch::ComponentsIndices::COLLIDER_FLAGS_COMPONENT) |
			(1ul << arch::ComponentsIndices::TRANSFORM_COMPONENT) |
			(1ul << arch::ComponentsIndices::MESH_COMPONENT);

        CCollisionSystem(core::CStack& _input_Stack) : Input_Stack_(_input_Stack) {}
		void Update() override;
        bool UpperActorCheck(vec3 backtrackingPosition,
							 vec3 comparedPosition,
							 float backtrackingScale,
							 float comparedScale,
							 components::MeshHandle backtrackingMeshHandle,
							 components::MeshHandle comparedMeshHandle);
    };
}
	
#endif
