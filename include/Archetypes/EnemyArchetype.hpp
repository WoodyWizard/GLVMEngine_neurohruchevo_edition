#ifndef ENEMY_ARCHETYPE_HPP
#define ENEMY_ARCHETYPE_HPP

#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Components/AnimationComponent.hpp"
#include "Components/AttackComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/EnemyComponent.hpp"
#include "Components/FontComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/RotationComponent.hpp"
#include "Components/StateComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Components/MoveComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t ENEMY_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::enemy) +
		 sizeof(components::state) +
		 sizeof(components::font) +
		 sizeof(components::animation) +
		 sizeof(components::material) +
		 sizeof(components::mesh) +
		 sizeof(components::collider) +
		 sizeof(components::colliderFlags) +
		 sizeof(components::health) +
		 sizeof(components::rigidBody) +
		 sizeof(components::attack) +
		 sizeof(components::rotation) +
		 sizeof(components::move));
	
	struct EnemyArchetype : Archetype {
		components::transform     transforms[ENEMY_ARCH_CHUNK_SIZE];
		components::enemy         enemies[ENEMY_ARCH_CHUNK_SIZE];
		components::state         states[ENEMY_ARCH_CHUNK_SIZE];
		components::font          fonts[ENEMY_ARCH_CHUNK_SIZE];
		components::animation     animations[ENEMY_ARCH_CHUNK_SIZE];
		components::material      materials[ENEMY_ARCH_CHUNK_SIZE];
		components::mesh          meshes[ENEMY_ARCH_CHUNK_SIZE];
		components::collider      colliders[ENEMY_ARCH_CHUNK_SIZE];
		components::colliderFlags colliderFlags[ENEMY_ARCH_CHUNK_SIZE];
		components::health        health[ENEMY_ARCH_CHUNK_SIZE];
		components::rigidBody     rigidBodies[ENEMY_ARCH_CHUNK_SIZE];
		components::attack        attacks[ENEMY_ARCH_CHUNK_SIZE];
		components::rotation      rotations[ENEMY_ARCH_CHUNK_SIZE];
		components::move          moves[ENEMY_ARCH_CHUNK_SIZE];

		EnemyArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::ENEMY_COMPONENT]           = enemies;
			components[ComponentsIndices::STATE_COMPONENT]           = states;
			components[ComponentsIndices::FONT_COMPONENT]            = fonts;
			components[ComponentsIndices::ANIMATION_COMPONENT]       = animations;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::COLLIDER_COMPONENT]        = colliders;
			components[ComponentsIndices::COLLIDER_FLAGS_COMPONENT]  = colliderFlags;
			components[ComponentsIndices::HEALTH_COMPONENT]          = health;
			components[ComponentsIndices::RIGID_BODY_COMPONENT]      = rigidBodies;
			components[ComponentsIndices::ATTACK_COMPONENT]          = attacks;
			components[ComponentsIndices::ROTATION_COMPONENT]        = rotations;
			components[ComponentsIndices::MOVE_COMPONENT]            = moves;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::ENEMY_COMPONENT) |
				(1ull << ComponentsIndices::STATE_COMPONENT) |
				(1ull << ComponentsIndices::FONT_COMPONENT) |
				(1ull << ComponentsIndices::ANIMATION_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_FLAGS_COMPONENT) |
				(1ull << ComponentsIndices::HEALTH_COMPONENT) |
				(1ull << ComponentsIndices::RIGID_BODY_COMPONENT) |
				(1ull << ComponentsIndices::ATTACK_COMPONENT) |
				(1ull << ComponentsIndices::ROTATION_COMPONENT) |
				(1ull << ComponentsIndices::MOVE_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::ENEMY_COMPONENT;
			componentIds[2] = ComponentsIndices::STATE_COMPONENT;
			componentIds[3] = ComponentsIndices::FONT_COMPONENT;
			componentIds[4] = ComponentsIndices::ANIMATION_COMPONENT;
			componentIds[5] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[6] = ComponentsIndices::MESH_COMPONENT;
			componentIds[7] = ComponentsIndices::COLLIDER_COMPONENT;
			componentIds[8] = ComponentsIndices::COLLIDER_FLAGS_COMPONENT;
			componentIds[9] = ComponentsIndices::HEALTH_COMPONENT;
			componentIds[10] = ComponentsIndices::RIGID_BODY_COMPONENT;
			componentIds[11] = ComponentsIndices::ATTACK_COMPONENT;
			componentIds[12] = ComponentsIndices::ROTATION_COMPONENT;
			componentIds[13] = ComponentsIndices::MOVE_COMPONENT;
			componentCount = 14;
			static_assert( ENEMY_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch

#endif
