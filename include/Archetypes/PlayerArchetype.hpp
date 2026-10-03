#ifndef PLAYER_ARCHETYPE_HPP
#define PLAYER_ARCHETYPE_HPP

#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Components/AnimationComponent.hpp"
#include "Components/AttackComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/FontComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Components/ViewComponent.hpp"
#include "Components/MoveComponent.hpp"
#include "TagComponents/PlayerTagComponent.hpp"
#include "Globals.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Components/RotationComponent.hpp"

namespace GLVM::ecs::arch {
	constexpr uint32_t PLAYER_ARCH_CHUNK_SIZE =
		ARCHETYPE_CHUNK_SIZE /
		(sizeof(components::transform) +
		 sizeof(components::beholder) +
		 sizeof(components::collider) +
		 sizeof(components::colliderFlags) +
		 sizeof(components::mesh) +
		 sizeof(components::rigidBody) +
		 sizeof(components::health) +
		 sizeof(components::material) +
		 sizeof(components::move) +
		 sizeof(components::attack) +
		 sizeof(components::animation) +
		 sizeof(components::font) +
		 sizeof(components::rotation) +
		 sizeof(tagComponents::playerTagComponent));
	
	struct PlayerArchetype : Archetype {
		components::transform             transforms[PLAYER_ARCH_CHUNK_SIZE];
		components::beholder              beholders[PLAYER_ARCH_CHUNK_SIZE];
		components::collider              colliders[PLAYER_ARCH_CHUNK_SIZE];
		components::colliderFlags         colliderFlags[PLAYER_ARCH_CHUNK_SIZE];
		components::mesh                  meshes[PLAYER_ARCH_CHUNK_SIZE];
		components::rigidBody             rigidBodies[PLAYER_ARCH_CHUNK_SIZE];
		components::health                health[PLAYER_ARCH_CHUNK_SIZE];
		components::material              materials[PLAYER_ARCH_CHUNK_SIZE];
		components::move                  moves[PLAYER_ARCH_CHUNK_SIZE];
		components::attack                attacks[PLAYER_ARCH_CHUNK_SIZE];
		components::animation             animations[PLAYER_ARCH_CHUNK_SIZE];
		components::font                  fonts[PLAYER_ARCH_CHUNK_SIZE];
		components::rotation              rotations[PLAYER_ARCH_CHUNK_SIZE];
		tagComponents::playerTagComponent playerTagComponents[PLAYER_ARCH_CHUNK_SIZE];

		PlayerArchetype() {
			components[ComponentsIndices::TRANSFORM_COMPONENT]       = transforms;
			components[ComponentsIndices::VIEW_COMPONENT]            = beholders;
			components[ComponentsIndices::COLLIDER_COMPONENT]        = colliders;
			components[ComponentsIndices::COLLIDER_FLAGS_COMPONENT]  = colliderFlags;
			components[ComponentsIndices::MESH_COMPONENT]            = meshes;
			components[ComponentsIndices::RIGID_BODY_COMPONENT]      = rigidBodies;
			components[ComponentsIndices::HEALTH_COMPONENT]          = health;
			components[ComponentsIndices::MATERIAL_COMPONENT]        = materials;
			components[ComponentsIndices::MOVE_COMPONENT]            = moves;
			components[ComponentsIndices::ATTACK_COMPONENT]          = attacks;
			components[ComponentsIndices::ANIMATION_COMPONENT]       = animations;
			components[ComponentsIndices::FONT_COMPONENT]            = fonts;
			components[ComponentsIndices::ROTATION_COMPONENT]        = rotations;
			components[ComponentsIndices::PLAYER_TAG_COMPONENT]      = playerTagComponents;

			mask =
				(1ull << ComponentsIndices::TRANSFORM_COMPONENT) |
				(1ull << ComponentsIndices::VIEW_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_COMPONENT) |
				(1ull << ComponentsIndices::COLLIDER_FLAGS_COMPONENT) |
				(1ull << ComponentsIndices::MESH_COMPONENT) |
				(1ull << ComponentsIndices::RIGID_BODY_COMPONENT) |
				(1ull << ComponentsIndices::HEALTH_COMPONENT) |
				(1ull << ComponentsIndices::MATERIAL_COMPONENT) |
				(1ull << ComponentsIndices::MOVE_COMPONENT) |
				(1ull << ComponentsIndices::ATTACK_COMPONENT) |
				(1ull << ComponentsIndices::ANIMATION_COMPONENT) |
				(1ull << ComponentsIndices::FONT_COMPONENT) |
				(1ull << ComponentsIndices::ROTATION_COMPONENT) |
				(1ull << ComponentsIndices::PLAYER_TAG_COMPONENT);

			componentIds[0] = ComponentsIndices::TRANSFORM_COMPONENT;
			componentIds[1] = ComponentsIndices::VIEW_COMPONENT;
			componentIds[2] = ComponentsIndices::COLLIDER_COMPONENT;
			componentIds[3] = ComponentsIndices::COLLIDER_FLAGS_COMPONENT;
			componentIds[4] = ComponentsIndices::MESH_COMPONENT;
			componentIds[5] = ComponentsIndices::RIGID_BODY_COMPONENT;
			componentIds[6] = ComponentsIndices::HEALTH_COMPONENT;
			componentIds[7] = ComponentsIndices::MATERIAL_COMPONENT;
			componentIds[8] = ComponentsIndices::MOVE_COMPONENT;
			componentIds[9] = ComponentsIndices::ATTACK_COMPONENT;
			componentIds[10] = ComponentsIndices::ANIMATION_COMPONENT;
			componentIds[11] = ComponentsIndices::FONT_COMPONENT;
			componentIds[12] = ComponentsIndices::ROTATION_COMPONENT;
			componentIds[13] = ComponentsIndices::PLAYER_TAG_COMPONENT;
			componentCount = 14;
			static_assert( PLAYER_ARCH_CHUNK_SIZE <= CAPACITY, "Archetype component arrays are larger than entities array" );
		}
	};
}; // namespace GLVM::ecs::arch

#endif
