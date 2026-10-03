#ifndef COLLIDER_FLAGS_COMPONENT_HPP
#define COLLIDER_FLAGS_COMPONENT_HPP

namespace GLVM::ecs::components {
	struct colliderFlags {
		int flags : 4 = 0; ///< 0001 = wallCollision; 0010 = groundCollision; 0100 = roofCollision; 1000 = itemDrag
	};
}; // namespace GLVM::ecs::components

#endif
