// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef ATTACK_COMPONENT_HPP
#define ATTACK_COMPONENT_HPP

namespace GLVM::ecs::components
{
	struct attack
	{
		float damage = 0.0f;               ///< Damage received on the current frame. DamageSystem applies and resets it.
	};
}

#endif
