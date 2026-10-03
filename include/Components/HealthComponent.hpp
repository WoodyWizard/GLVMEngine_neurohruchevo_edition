// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef HEALTH_COMPONENT_HPP
#define HEALTH_COMPONENT_HPP

namespace GLVM::ecs::components
{
	struct health
	{
		float maxHealth     = 100.0f;
        float currentHealth = 100.0f;
		bool randarable = true;
	};
}

#endif
