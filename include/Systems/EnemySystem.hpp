// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef ENEMY_SYSTEM
#define ENEMY_SYSTEM

#include "ArchetypeECS/ArchECS_Types.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Components/AnimationComponent.hpp"
#include "ISystem.hpp"
#include "EntityManager.hpp"
#include "Vector.hpp"
#include "Components/EnemyComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/EnemyComponent.hpp"
#include "Components/DamageComponent.hpp"
#include "Components/StateComponent.hpp"
#include "ISoundEngine.hpp"
#include "Common/CommonFunctions.hpp"

namespace GLVM::ecs
{
	class EnemySystem : public ISystem
	{
	public:
		uint32_t playerArchetypesNumber      = 0;
		uint32_t enemyArchetypesNumber       = 0;
		uint32_t projectileArchetypesNumber  = 0;
		struct ArchView {
			arch::Archetype* playerCachedArchetype = nullptr;
			arch::Archetype* enemyCachedArchetype  = nullptr;
			arch::Archetype* projectileArchetype   = nullptr;
		} archView;
		
		struct ComponentsView {
			ecs::components::transform* playerTransforms = nullptr;

			ecs::components::transform* enemyTransforms  = nullptr;
			ecs::components::state*     enemyStates      = nullptr;
			ecs::components::animation* enemyAnimation   = nullptr;
			ecs::components::enemy*     enemies          = nullptr;
		} componentsView;

		arch::componentMask playerRequiredMask =
			(1ull << ecs::arch::ComponentsIndices::PLAYER_TAG_COMPONENT);

		arch::componentMask enemyRequiredMask  =
			(1ul << arch::ComponentsIndices::TRANSFORM_COMPONENT)  |
			(1ul << arch::ComponentsIndices::STATE_COMPONENT) |
			(1ul << arch::ComponentsIndices::ANIMATION_COMPONENT)  |
			(1ul << arch::ComponentsIndices::ENEMY_COMPONENT);

		arch::componentMask projectileRequiredMask =
			(1ull << ecs::arch::ComponentsIndices::PROJECTILE_TAG_COMPONENT);
		
		void Update() override;
		// void CalculateProjectile(const vec3& projectilePosition,
		// 						 const vec3& projectileForward,
		// 						 const ecs::components::MeshHandle& meshHandle,
		// 						 const components::material& material,
		// 						 const components::damage& damage);

		core::Sound::ISoundEngine* soundEngine = nullptr;
		core::vector<ecs::TextureHandle> textureHandlers;
		core::vector<ecs::components::MeshHandle> meshHandlers;
		float deltaFrameTime = 0.0f;                     ///< Attack cooldown is stored per enemy (components::enemy::attackCooldown)
	};
} // namespace GLVM::ecs

#endif
