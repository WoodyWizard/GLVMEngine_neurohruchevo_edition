#include "Systems/ItemSystem.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Archetypes/ItemArchetype.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/ColliderFlagsComponent.hpp"
#include "Components/CrosshairComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "Components/InventorySlotComponent.hpp"
#include "Components/ItemComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/TransformComponent.hpp"
#include <cstdint>
#ifdef __linux__
#include <unistd.h>
#endif

namespace GLVM::ecs
{
	/*
	  ===========================================
	  This method is trying to search for suitable
	  slots for the given specific type item. It returns
	  true if it finds them and false otherwise.
	  ==========================================
	*/
	
	bool ItemSystem::putItem2x2(components::inventory* inventoryComponent, unsigned int itemEntity) {
		namespace cm = GLVM::ecs::components;
		
		bool isSlotFound = false;
		unsigned int row = inventoryComponent->row;
		unsigned int col = inventoryComponent->col;

		const arch::EntityLocation& itemLocation = arch::world.entityLocations[arch::getId( itemEntity )];
		arch::ItemArchetype* itemArch = static_cast<arch::ItemArchetype*>(itemLocation.arch);
		if ( itemArch == nullptr )
			return false;

		const uint32_t itemIndex = itemLocation.index;
		cm::item* itemComponent = &itemArch->items[itemIndex];

		unsigned int item_width = itemComponent->itemSlotType.width;
		unsigned int item_height = itemComponent->itemSlotType.height;

		std::cout << "item width: " << item_width << std::endl;
		std::cout << "item height: " << item_height << std::endl;
		if ( item_width == 0 || item_height == 0 || item_width > col || item_height > row )
			return false;
		for ( unsigned int i = 0; i < row - item_height + 1; ++i )
			for ( unsigned int j = 0; j < col - item_width + 1; ++j ) {
				core::vector<unsigned int> maybeAvailabeSlots;
				core::vector<unsigned int> indicesOfMaybeAvailableSlots;
				for ( unsigned int m = i; m < i + item_height; ++m )
					for ( unsigned int n = j; n < j + item_width; ++n ) {
						maybeAvailabeSlots.Push(inventoryComponent->slots[m][n]);
						indicesOfMaybeAvailableSlots.Push(m * col + n);
					}

				unsigned int isAllSlotsAvailable = 0;
				for ( unsigned int v = 0; v < maybeAvailabeSlots.GetSize(); ++v ) {
					if ( maybeAvailabeSlots[v] == UINT_MAX ) {
//						std::cout << "inc" << std::endl;
						++isAllSlotsAvailable;
					} else {
//						std::cout << "dec" << std::endl;
						--isAllSlotsAvailable;
					}
				}
//				std::cout << "all slots: " << isAllSlotsAvailable << std::endl;
				if ( maybeAvailabeSlots.GetSize() == isAllSlotsAvailable ) {
					for ( unsigned int w = 0; w < maybeAvailabeSlots.GetSize(); ++w ) {
						unsigned int row_index = indicesOfMaybeAvailableSlots[w] / col;     ///< Slot index is row * col + column
						unsigned int col_index = indicesOfMaybeAvailableSlots[w] % col;
						inventoryComponent->slots[row_index][col_index] = itemEntity;
						itemComponent->occupiedSlots.Push(indicesOfMaybeAvailableSlots[w]);
					}
					
					isSlotFound = true;
					return isSlotFound;
				}
			}

		return isSlotFound;
	}
	
	void ItemSystem::Update() {
		namespace cm = GLVM::ecs::components;

		if (!isInventoryOpened) {
			/// Only one archetype of every kind is cached (capacity of each cache is 1)
			inventoryArchetypesNumber = 0;
			arch::world.searchCacheArchetypes( inventoryRequiredMask, &archView.inventoryCachedArchetype, inventoryArchetypesNumber, 1 );
			itemArchetypesNumber = 0;
			arch::world.searchCacheArchetypes( itemRequiredMask, &archView.itemArchetype, itemArchetypesNumber, 1 );
			if ( inventoryArchetypesNumber == 0 || itemArchetypesNumber == 0 )
				return;

			componentsView.inventoriesView = (ecs::components::inventory*)archView.inventoryCachedArchetype->
				components[arch::ComponentsIndices::INVENTORY_COMPONENT];
			componentsView.itemsView         = (ecs::components::item*)archView.itemArchetype->
				components[arch::ComponentsIndices::ITEM_COMPONENT];
			componentsView.itemCollidersView = (ecs::components::collider*)archView.itemArchetype->
				components[arch::ComponentsIndices::COLLIDER_COMPONENT];
			
			
			for ( unsigned int m = 0; m < archView.inventoryCachedArchetype->entityCount; ++m ) {
				cm::inventory* inventoryComponent = &componentsView.inventoriesView[m];

				for ( unsigned int i = 0; i < archView.itemArchetype->entityCount; ++i ) {
					unsigned int itemEntity = archView.itemArchetype->entities[i];
					cm::collider* itemColliderComponent = &componentsView.itemCollidersView[i];

					for ( unsigned int j = 0; j < itemColliderComponent->colliders.GetSize(); ++j ) {
						if ( itemColliderComponent->colliders[j] == inventoryComponent->entityOwner &&
							componentsView.itemsView[i].isActor ) {
							if ( putItem2x2(inventoryComponent, itemEntity) ) {
								componentsView.itemsView[i].isActor = false;
//								componentManager->RemoveComponent<cm::actor>(itemEntity);
//								componentManager->RemoveComponent<cm::rigidBody>(itemEntity);
							} else {
								std::cout << "No suitable slots for that item in inventory" << std::endl;
							}
						}
					}
				}
			}
		}

		if(isInventoryOpened) {
			crosshairArchetypesNumber = 0;
			arch::world.searchCacheArchetypes( crosshairRequiredMask, &archView.crosshairArchetype, crosshairArchetypesNumber, 1 );
			itemArchetypesNumber = 0;
			arch::world.searchCacheArchetypes( itemRequiredMask, &archView.itemArchetype, itemArchetypesNumber, 1 );
			if ( crosshairArchetypesNumber == 0 || itemArchetypesNumber == 0 || archView.crosshairArchetype->entityCount == 0 )
				return;

			componentsView.crosshairTransforms = (ecs::components::transform*)archView.crosshairArchetype->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			componentsView.itemTransformsView = (ecs::components::transform*)archView.itemArchetype->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];

			components::transform* crosshairTransformComponent = &componentsView.crosshairTransforms[0];
			for ( unsigned int i = 0; i < archView.itemArchetype->entityCount; ++i ) {
				uint32_t entityItemContaining = archView.itemArchetype->entities[i];
				cm::transform* itemTransformComponent = &componentsView.itemTransformsView[i];

				// bool isCrosshairCollided = false;
				// for ( unsigned int n = 0; n < itemColliderComponent->colliders.GetSize(); ++n ) {
				// 	if ( itemColliderComponent->colliders[n] == linkedCrosshairEntities[0] ) {                ///< Is there a crosshair among the colliders
				// 		isCrosshairCollided = true;
				// 		break;
				// 	}
				// }
				
				// if ( itemColliderComponent->wallCollision && isCrosshairCollided ) {
				// 	itemTransformComponent->position = crosshairTransformComponent->position;                 ///< Set crosshair position to draged item
				// 	itemColliderComponent->itemDrag = true;
				// 	isItemDraged = true;
				// }
				
				if ( *dragedItemEntity >= 0 && *dragedItemEntity == (int)entityItemContaining ) {
					itemTransformComponent->position = crosshairTransformComponent->position;                 ///< Set crosshair position to draged itemx
				}
			}
		}

		// if ( mouseOffsetX > 1.0f )
		// 	mouseOffsetX = 1.0f;
		// else if ( mouseOffsetX < -1.0f )
		// 	mouseOffsetX = -1.0f;
		
		// if ( mouseOffsetY > 1.0f )
		// 	mouseOffsetY = 1.0f;
		// else if ( mouseOffsetY < -1.0f )
		// 	mouseOffsetY = -1.0f;

		
// 		if ( isInventoryOpened ) {
// 			if ( !isItemDraged && isLeftMouseButtonPressed && *isLeftMouseButtonReleased ) {
// 				std::cout << "Take an item" << std::endl;
// 				std::cout << "x: " << mouseOffsetX << std::endl;
// 				std::cout << "y: " << mouseOffsetY << std::endl;
// 				*isLeftMouseButtonReleased = false;
// 				isItemDraged = true;
// 			}

// 			if ( isItemDraged ) {
// //			std::cout << "highlight item slots" << std::endl;
// 			}

// 			if ( isItemDraged && isLeftMouseButtonPressed && *isLeftMouseButtonReleased ) {
// 				std::cout << "item droped" << std::endl;
// 				*isLeftMouseButtonReleased = false;
// 				isItemDraged = false;
// 			}
// 		}
	}
} // namespace GLVM::ecs
