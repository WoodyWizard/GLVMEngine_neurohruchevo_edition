#ifndef ITEM_SYSTEM
#define ITEM_SYSTEM

#include "Archetypes/ItemArchetype.hpp"
#include "ComponentManager.hpp"
#include "EntityManager.hpp"
#include "EventsStack.hpp"
#include "ISystem.hpp"
#include "Components/ItemComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ActorComponent.hpp"
#include "Components/CrosshairComponent.hpp"
#include <climits>
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Archetypes/CrosshairArchetype.hpp"
#include "Archetypes/InventoryArchetype.hpp"
#include "ArchetypeECS/ArchECS_Utils.hpp"

namespace GLVM::ecs
{
	class ItemSystem : public ISystem
	{
	public:
		uint32_t inventoryArchetypesNumber = 0;
		uint32_t itemArchetypesNumber      = 0;
		uint32_t crosshairArchetypesNumber = 0;
		struct ArchView {
			arch::Archetype* inventoryCachedArchetype = nullptr;
			arch::Archetype* itemArchetype            = nullptr;
			arch::Archetype* crosshairArchetype       = nullptr;
		} archView;
		
		struct ComponentsView {
			ecs::components::inventory* inventoriesView = nullptr;
			
			ecs::components::item*      itemsView          = nullptr;
			ecs::components::collider*  itemCollidersView  = nullptr;
			ecs::components::transform* itemTransformsView = nullptr;

			ecs::components::transform* crosshairTransforms = nullptr;
//			
//			arch::ProjectileBundle*             projectileBundles       = nullptr;
		} componentsView;

		arch::componentMask inventoryRequiredMask =
			(1ull << ecs::arch::ComponentsIndices::INVENTORY_COMPONENT);

		arch::componentMask itemRequiredMask =
			(1ul << arch::ComponentsIndices::TRANSFORM_COMPONENT) |
			(1ul << arch::ComponentsIndices::ITEM_COMPONENT)      |
			(1ul << arch::ComponentsIndices::MESH_COMPONENT)      |
			(1ul << arch::ComponentsIndices::MATERIAL_COMPONENT)  |
			(1ul << arch::ComponentsIndices::COLLIDER_COMPONENT)  |
			(1ul << arch::ComponentsIndices::COLLIDER_FLAGS_COMPONENT);

		arch::componentMask crosshairRequiredMask =
			(1ul << arch::ComponentsIndices::TRANSFORM_COMPONENT) |
			(1ul << arch::ComponentsIndices::CROSSHAIR_TAG_COMPONENT);
		
		void Update();
		bool putItem2x2(components::inventory* inventoryComponent, unsigned int itemEntity);

		core::CStack* inputStack                = nullptr;
		bool          isInventoryOpened         = false;
		int*          dragedItemEntity          = nullptr;
		bool*         isLeftMouseButtonReleased = nullptr;
		bool          isLeftMouseButtonPressed  = false;
		float         mouseOffsetX = 0;
		float         mouseOffsetY = 0;
	};
} // namespace GLVM::ecs


#endif
