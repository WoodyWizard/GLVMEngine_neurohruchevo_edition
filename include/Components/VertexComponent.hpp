// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef VERTEX_COMPONENT_HPP
#define VERTEX_COMPONENT_HPP

// #include <vector>
// #include <iostream>
// #include "GraphicAPI/Vulkan.hpp"

#include <cstdint>
#include "VertexMath.hpp"

namespace GLVM::ecs::components
{
	struct MeshHandle {
		uint32_t id = 0;
	};
	
	struct mesh
	{
        MeshHandle handle;
		bool gltf       = true;
		AABB aabb{};
		bool randarable = true;
	};
}

#endif
