// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef PROJECTILE_COMPONENT
#define PROJECTILE_COMPONENT

#include <climits>

namespace GLVM::ecs::components
{
    class projectile
    {
    public:
		unsigned int owner = UINT_MAX;     ///< Id of entity that fired the projectile. Projectile does not collide with its owner.
        bool bCollision_Status_ = false;
        float fDamage_ = 0.0f;
        float fSpeed_ = 0.0f;
        float fFlying_Range_ = 0.0f;
		float damage = 0.0f;
		float lifeTime = 0.0f;             ///< Seconds left before the projectile is destroyed
    };
}

#endif
