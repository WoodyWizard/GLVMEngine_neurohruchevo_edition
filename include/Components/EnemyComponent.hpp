// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef ENEMY_COMPONENT_HPP
#define ENEMY_COMPONENT_HPP

namespace GLVM::ecs::components
{
	struct enemy
	{
		float detectRadius   = 15.0f;
		float attackCooldown = 5.0f;       ///< Own cooldown of every enemy. Decreases on every frame, enemy can shoot when it is <= 0
	};
}

#endif
