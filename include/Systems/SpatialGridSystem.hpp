// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef SPATIAL_GRID_SYSTEM_HPP
#define SPATIAL_GRID_SYSTEM_HPP

#include "ISystem.hpp"
#include "VertexMath.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Globals.hpp"
#include "Common/CommonFunctions.hpp"
#include "Vector.hpp"

namespace GLVM::ecs {

	class SpatialGridSystem : public ISystem {
		arch::Archetype* cachedArchetypes[32];
		uint32_t cachedArchetypesNumber = 0;
		bool isInitialized = false;
		core::vector<u32> occupiedCells;              ///< Flat indices of cells filled on the previous update. Only they are cleared
		bool isOutOfGridReported = false;
		
		struct SpatialGridComponentsView {
			components::transform* transforms = nullptr;
			components::mesh* meshes          = nullptr;
		} view;
		
		arch::componentMask requiredMask =
			(1ul << arch::ComponentsIndices::COLLIDER_COMPONENT) |
			(1ul << arch::ComponentsIndices::COLLIDER_FLAGS_COMPONENT) |
			(1ul << arch::ComponentsIndices::TRANSFORM_COMPONENT) |
			(1ul << arch::ComponentsIndices::MESH_COMPONENT);

		void Update() override;
	};
	
}; ///< namespace GLVM::core

#endif
