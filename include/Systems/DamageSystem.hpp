// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef DAMAGE_SYSTEM
#define DAMAGE_SYSTEM


#include "ISystem.hpp"
#include "EntityManager.hpp"
#include "Components/DamageComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/AttackComponent.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs
{
	class DamageSystem : public ISystem
	{
	public:
		void Update() override;

		float deltaTime = 0.0f;
		bool  isPlayerDeathReported = false;

		uint32_t cachedAttackableArchetypesNumber = 0;
		uint32_t cachedFontArchetypesNumber       = 0;
		struct ArchView {
			arch::Archetype* cachedAttackableArchetypes[32];
			arch::Archetype* cachedFontArchetypes[32];
		} archView;
		
		struct ComponentsView {
			ecs::components::attack* attackableAttacks = nullptr;
			ecs::components::health* attackableHealth  = nullptr;
			ecs::components::font*   attackableFonts   = nullptr;

			ecs::components::font*   fonts = nullptr;
		} componentsView;

		arch::componentMask attackableRequiredMask =
			(1ul << arch::ComponentsIndices::ATTACK_COMPONENT) |
			(1ul << arch::ComponentsIndices::HEALTH_COMPONENT) |
			(1ul << arch::ComponentsIndices::FONT_COMPONENT);

		arch::componentMask fontRequiredMask =
			(1ull << ecs::arch::ComponentsIndices::FONT_COMPONENT);

		arch::componentMask playerRequiredMask =
			(1ull << ecs::arch::ComponentsIndices::PLAYER_TAG_COMPONENT);
	};
} // namespace GLVM::ecs

#endif
