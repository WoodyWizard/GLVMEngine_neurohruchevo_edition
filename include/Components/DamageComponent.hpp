// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef DAMAGE_COMPONENT_HPP
#define DAMAGE_COMPONENT_HPP

namespace GLVM::ecs::components
{
	struct damage
	{
		float maximumDamage    = 0.0f;
		float minimumDamage    = 0.0f;
		float criticalHitRate  = 0.0f;
		float criticalModifier = 0.0f;
	};
}

#endif
