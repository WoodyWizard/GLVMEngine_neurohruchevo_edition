#include "Systems/InventorySystem.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "Archetypes/ItemArchetype.hpp"
#include "Components/CrosshairComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "Components/ItemComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "VertexMath.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "Archetypes/EnemyArchetype.hpp"
#include "TagComponents/CrosshairTagComponent.hpp"
#include "Archetypes/CrosshairArchetype.hpp"
#include "Archetypes/InventoryArchetype.hpp"
#include <cstdint>
                                
namespace GLVM::ecs
{
	void InventorySystem::Update() {
		if ( isInventoryOpened ) {
			namespace cm = GLVM::ecs::components;

			/// Only one archetype of every kind is cached (capacity of each cache is 1)
			crosshairArchetypesNumber = 0;
			arch::world.searchCacheArchetypes( crosshairRequiredMask, &archView.crosshairCachedArchetype, crosshairArchetypesNumber, 1 );
			inventoryArchetypesNumber = 0;
			arch::world.searchCacheArchetypes( inventoryRequiredMask, &archView.inventoryCachedArchetype, inventoryArchetypesNumber, 1 );
			if ( crosshairArchetypesNumber == 0 || inventoryArchetypesNumber == 0 ||
				 archView.crosshairCachedArchetype->entityCount == 0 || archView.inventoryCachedArchetype->entityCount == 0 ||
				 isItemDraged == nullptr || isLeftMouseButtonReleased == nullptr )
				return;

			componentsView.crosshairTransformsView = (ecs::components::transform*)archView.crosshairCachedArchetype->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];

			componentsView.inventoryTransformsView = (ecs::components::transform*)archView.inventoryCachedArchetype->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			componentsView.inventoryView           = (ecs::components::inventory*)archView.inventoryCachedArchetype->
				components[arch::ComponentsIndices::INVENTORY_COMPONENT];
			componentsView.inventoryMeshesView     = (ecs::components::mesh*)archView.inventoryCachedArchetype->
				components[arch::ComponentsIndices::MESH_COMPONENT];
						
			if( componentsView.crosshairTransformsView && componentsView.inventoryTransformsView &&
				componentsView.inventoryView && componentsView.inventoryMeshesView ) {
				cm::transform* crosshairTransformComponent = &componentsView.crosshairTransformsView[0];
			
				cm::transform* inventoryTransformComponent = &componentsView.inventoryTransformsView[0];
				cm::inventory* inventoryComponent          = &componentsView.inventoryView[0];
				cm::mesh*      inventoryMeshComponent      = &componentsView.inventoryMeshesView[0];
			
				const float inventorySlotScale = inventoryMeshComponent->gltf ? inventoryComponent->slotScale * 2.0f : inventoryComponent->slotScale;
				const float inventorySlotHalfScale = inventoryMeshComponent->gltf ? inventoryComponent->slotScale : inventoryComponent->slotScale * 0.5f;

				if ( *isItemDraged < 0 && isLeftMouseButtonPressed && *isLeftMouseButtonReleased ) {           ///< Take an item from inventory
					if ( checkCrosshairInventoryIntersection( crosshairTransformComponent, inventoryTransformComponent, inventoryComponent,
															  inventorySlotScale, inventorySlotHalfScale) ) {
						point2D<int> intersectionSlot = determineActualIntersectionSlot( crosshairTransformComponent, inventoryTransformComponent, inventorySlotScale, inventorySlotHalfScale );
						const unsigned int row    = intersectionSlot.y;
						const unsigned int column = intersectionSlot.x;
						const unsigned int entity = (row < inventoryComponent->row && column < inventoryComponent->col) ?
							inventoryComponent->slots[row][column] : UINT_MAX;
						if( entity != UINT_MAX && entity < arch::world.entityLocations.GetSize() ) {  ///< Check slot is not empty and hold an item
							const arch::EntityLocation& itemLocation = arch::world.entityLocations[arch::getId( entity )];
							arch::ItemArchetype* itemArch = static_cast<arch::ItemArchetype*>(itemLocation.arch);
							const uint32_t itemIndex = itemLocation.index;
							cm::item* itemComponent = itemArch ? &itemArch->items[itemIndex] : nullptr;

							if( itemComponent != nullptr ) {
								for( unsigned int i = 0; i < itemComponent->occupiedSlots.GetSize(); ++i ) {
									unsigned int row_index = itemComponent->occupiedSlots[i] / inventoryComponent->col;     ///< Slot index is row * col + column
									unsigned int col_index = itemComponent->occupiedSlots[i] % inventoryComponent->col;

									inventoryComponent->slots[row_index][col_index] = UINT_MAX;   ///< Need to free all slots that hold an item
								}
							}
							*isItemDraged = entity;                                               ///< Set curretly draged item entity
						}
					}
					*isLeftMouseButtonReleased = false;
				}

				if ( *isItemDraged >= 0 ) {
					if ( checkCrosshairInventoryIntersection( crosshairTransformComponent, inventoryTransformComponent, inventoryComponent,
															  inventorySlotScale, inventorySlotHalfScale) ) {
						[[maybe_unused]] point2D<int> intersectionSlot = determineActualIntersectionSlot( crosshairTransformComponent, inventoryTransformComponent, inventorySlotScale, inventorySlotHalfScale );

						const arch::EntityLocation& itemLocation = arch::world.entityLocations[arch::getId( *isItemDraged )];
						arch::ItemArchetype* itemArch = static_cast<arch::ItemArchetype*>(itemLocation.arch);
						const uint32_t itemIndex = itemLocation.index;
						cm::item* itemComponent = &itemArch->items[itemIndex];

						core::vector<unsigned int> potentialOccupiedSlots;
						int isSwapable = 0;
						isSwapable = determineSwappableStatusAndSlots( itemComponent, inventoryTransformComponent, potentialOccupiedSlots, crosshairTransformComponent,
																	   intersectionSlot, inventoryComponent, inventorySlotScale );
					
						inventoryComponent->highlightedSlots = potentialOccupiedSlots;
						if( isSwapable == -1 || isSwapable >= 0 ) 
							inventoryComponent->isAvailableHighlightedSlots = true;
						else
							inventoryComponent->isAvailableHighlightedSlots = false;
					} else {
						inventoryComponent->highlightedSlots.clear();
					}
				}

				if ( *isItemDraged >= 0 && isLeftMouseButtonPressed && *isLeftMouseButtonReleased ) {    ///< Item drop to inventory, swaped or we just cant place
					int isSwapable = 0;
                    bool isCrosshairInventoryIntersects = checkCrosshairInventoryIntersection( crosshairTransformComponent, inventoryTransformComponent, inventoryComponent, inventorySlotScale, inventorySlotHalfScale);
					std::cout << "intersects: " << isCrosshairInventoryIntersects << std::endl;
                    if ( isCrosshairInventoryIntersects ) {
						[[maybe_unused]] point2D<int> intersectionSlot = determineActualIntersectionSlot( crosshairTransformComponent, inventoryTransformComponent, inventorySlotScale, inventorySlotHalfScale );

						const arch::EntityLocation& itemLocation = arch::world.entityLocations[arch::getId( *isItemDraged )];
						arch::ItemArchetype* itemArch = static_cast<arch::ItemArchetype*>(itemLocation.arch);
						const uint32_t itemIndex = itemLocation.index;
						cm::item* itemComponent = &itemArch->items[itemIndex];

						core::vector<unsigned int> potentialOccupiedSlots;
						isSwapable = determineSwappableStatusAndSlots( itemComponent, inventoryTransformComponent, potentialOccupiedSlots, crosshairTransformComponent,
																	   intersectionSlot, inventoryComponent, inventorySlotScale );

						const int itemWidth  = itemComponent->itemSlotType.width;
						const int itemHeight = itemComponent->itemSlotType.height;

						if( isSwapable == -1 ) {               ///< Default value. Just drop item to all empty slots
							itemComponent->occupiedSlots = potentialOccupiedSlots;
							fillInventorySlots( itemComponent, itemWidth, itemHeight, inventoryComponent, *isItemDraged );
						} else if ( isSwapable > 0 ) {         ///< Swap one item that we draging to another one in inventory
							const arch::EntityLocation& itemLocation = arch::world.entityLocations[arch::getId( isSwapable )];
							arch::ItemArchetype* itemArch = static_cast<arch::ItemArchetype*>(itemLocation.arch);
							const uint32_t itemIndex = itemLocation.index;
							cm::item* swapedItemComponent = &itemArch->items[itemIndex];

							itemComponent->occupiedSlots = potentialOccupiedSlots;
							fillInventorySlots( swapedItemComponent, swapedItemComponent->itemSlotType.width, swapedItemComponent->itemSlotType.height,
												inventoryComponent, UINT_MAX );
							
							swapedItemComponent->occupiedSlots.clear();
							fillInventorySlots( itemComponent, itemWidth, itemHeight, inventoryComponent, *isItemDraged );
						}

						if( isSwapable == -1 ) {
							*isLeftMouseButtonReleased = false;
							*isItemDraged = -1;
						} else if( isSwapable == -2) {         ///< Already have 2 or more items in potential inventory slots
							*isLeftMouseButtonReleased = false;
						} else {
							*isLeftMouseButtonReleased = false;
							*isItemDraged = isSwapable;
						}
                    } else { ///< Item drop to the ground
						const arch::EntityLocation& itemLocation = arch::world.entityLocations[arch::getId( *isItemDraged )];
						arch::ItemArchetype* itemArch = static_cast<arch::ItemArchetype*>(itemLocation.arch);
						const uint32_t itemIndex = itemLocation.index;
						itemArch->rigidBodies[itemIndex] = { .fMass_ = 2.0f };
						cm::transform* itemTransform = &itemArch->transforms[itemIndex];
						cm::item*      item          = &itemArch->items[itemIndex];
						item->isActor = true;

						/// Item is dropped in front of the inventory owner (player)
						const arch::componentMask playerTagMask = (1ull << arch::ComponentsIndices::PLAYER_TAG_COMPONENT);
						const uint32_t ownerId = inventoryComponent->entityOwner;
						const arch::Archetype* ownerArch = ownerId < arch::world.entityLocations.GetSize() ?
							arch::world.entityLocations[ownerId].arch : nullptr;
						if ( ownerArch != nullptr && arch::matchesRequiredMask( ownerArch->mask, playerTagMask ) ) {
							const arch::PlayerArchetype* playerArch = static_cast<const arch::PlayerArchetype*>(ownerArch);
							const uint32_t playerIndex = arch::world.entityLocations[ownerId].index;
							const cm::transform* playerTransform = &playerArch->transforms[playerIndex];
							itemTransform->position = playerTransform->position;
							vec3 normalizedForward = Normalize(playerTransform->forward);
							itemTransform->position[0] += normalizedForward[0] * 1.5f;
							itemTransform->position[1] += normalizedForward[1] * 1.5f;
							itemTransform->position[2] += normalizedForward[2] * 1.5f;
						}
						itemTransform->scale = 0.05f;
					
						*isItemDraged = -1;
						*isLeftMouseButtonReleased = false;
					}
				}
			}
		}
	}

	int InventorySystem::determineSwappableStatusAndSlots( components::item* itemComponent, components::transform* inventoryTransformComponent,
														   core::vector<unsigned int>& potentialOccupiedSlots, components::transform* crosshairTransformComponent,
														   point2D<int> intersectionSlot, components::inventory* inventoryComponent, const float inventorySlotScale ) {
		if( itemComponent != nullptr ) {
			const int itemWidth  = itemComponent->itemSlotType.width;
			const int itemHeight = itemComponent->itemSlotType.height;

			const int row    = intersectionSlot.y;
			const int column = intersectionSlot.x;

			/// Find left-upper pivot slot inventory
			int rowBasicOffset    = 0;
			int columnBasicOffset = 0;

			/*
			  ===============================================
			  Set as pivot point slot in left upper corner.
			  Need to calculate offset for row and column
			  to change it from center. And need to It is
			  necessary to take into account the offset
			  relative to the center for additional correction
			  ===============================================
			*/
			columnBasicOffset = calculateBasicOffset( itemWidth, inventoryTransformComponent->position[0],
													  crosshairTransformComponent->position[0], column, inventorySlotScale );
			rowBasicOffset    = calculateBasicOffset( itemHeight, inventoryTransformComponent->position[1],
													  crosshairTransformComponent->position[1], row, inventorySlotScale * aspectRate );

			int pivotRow    = row - rowBasicOffset;
			int pivotColumn = column - columnBasicOffset;

			pivotRow    = clamp<int>( 0, pivotRow, static_cast<int>(inventoryComponent->row) - itemHeight );
			pivotColumn = clamp<int>( 0, pivotColumn, static_cast<int>(inventoryComponent->col) - itemWidth );

			return determineSwappableField( itemComponent, itemWidth, itemHeight, pivotRow, pivotColumn, inventoryComponent, potentialOccupiedSlots );
		} else {
			return -3; ///< Return -3 as error code means itemComponent is nullptr
		}
	}
	
	void InventorySystem::fillInventorySlots( components::item* itemComponent, const int itemWidth, const int itemHeight,
											  components::inventory* inventoryComponent, const int fillValue ) {
		for( int i = 0; i < itemHeight; ++i ) {
			for( int j = 0; j < itemWidth; ++j ) {
				const unsigned int slotsRow = itemComponent->occupiedSlots[i * itemWidth + j] / inventoryComponent->col;
				const unsigned int slotsColumn = itemComponent->occupiedSlots[i * itemWidth + j] % inventoryComponent->col;

				inventoryComponent->slots[slotsRow][slotsColumn] = fillValue;
			}
		}
	}
	
	int InventorySystem::determineSwappableField( components::item* itemComponent, const int itemWidth, const int itemHeight,
												  int pivotRow, int pivotColumn, components::inventory* inventoryComponent,
												  core::vector<unsigned int>& potentialOccupiedSlots) {
		itemComponent->occupiedSlots.clear();
		int isSwapable = -1;                   ///< -1: default value. -2: found two entities in potential slots. Any other value: swapable.
		for( int i = 0; i < itemHeight; ++i ) {
			for( int j = 0; j < itemWidth; ++j ) {
				const unsigned int finalRow    = pivotRow + i;
				const unsigned int finalColumn = pivotColumn + j;
				if ( isSwapable == -1 && inventoryComponent->slots[finalRow][finalColumn] != UINT_MAX ) {
					isSwapable = inventoryComponent->slots[finalRow][finalColumn];
				} else if ( isSwapable > 0 && inventoryComponent->slots[finalRow][finalColumn] != UINT_MAX
							&& (int)inventoryComponent->slots[finalRow][finalColumn] != isSwapable ) {
					isSwapable = -2;
				}
								
				potentialOccupiedSlots.Push(finalRow * inventoryComponent->col + finalColumn);
			}
		}
		
		return isSwapable;
	}
	
	int InventorySystem::calculateBasicOffset( const int itemAxisSize, const float axisValue,
											   const float crosshairAxisPosition, const int axisSlotIndex,
											   const float inventorySlotScale) {
		if( itemAxisSize % 2 == 0 ) {
			const float slotCenterX = axisValue + static_cast<float>(axisSlotIndex) * inventorySlotScale;
			if( slotCenterX > crosshairAxisPosition ) {
				return itemAxisSize / 2;
			}
			return itemAxisSize / 2 - 1;
		}
		return itemAxisSize / 2;
	}
	
	bool InventorySystem::checkCrosshairInventoryIntersection( components::transform* crosshairTransformComponent, components::transform* inventoryTransformComponent,
															   components::inventory* inventoryComponent, const float inventorySlotScale, const float inventorySlotHalfScale ) {
		return crosshairTransformComponent->position[0] > inventoryTransformComponent->position[0] - inventorySlotHalfScale &&
			crosshairTransformComponent->position[0] < inventoryTransformComponent->position[0] - inventorySlotHalfScale + inventorySlotScale * inventoryComponent->col &&
													   crosshairTransformComponent->position[1] > inventoryTransformComponent->position[1] - inventorySlotHalfScale * aspectRate &&
			crosshairTransformComponent->position[1] < inventoryTransformComponent->position[1] - inventorySlotHalfScale * aspectRate + inventorySlotScale * inventoryComponent->row * aspectRate;
	}

	point2D<int> InventorySystem::determineActualIntersectionSlot( components::transform* crosshairTransformComponent, components::transform* inventoryTransformComponent,
															  const float inventorySlotScale, const float inventorySlotHalfScale ) {
		float x_delta = crosshairTransformComponent->position[0] - inventoryTransformComponent->position[0] + inventorySlotHalfScale;
		float y_delta = crosshairTransformComponent->position[1] - inventoryTransformComponent->position[1] + inventorySlotHalfScale * aspectRate;
		
		return point2D<int>{ (int)(x_delta / inventorySlotScale), (int)(y_delta / (inventorySlotScale * aspectRate)) };
	}
} ///< namespace GLVM::ecs
