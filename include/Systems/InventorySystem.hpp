#ifndef INVENTORY_SYSTEM_HPP
#define INVENTORY_SYSTEM_HPP

#include "ArchetypeECS/ArchECS_Types.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "ISystem.hpp"
#include "Components/ItemComponent.hpp"
#include "Components/CrosshairComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "VertexMath.hpp"
#include "Components/TransformComponent.hpp"
#include "ArchetypeECS/ArchECS_Utils.hpp"

namespace GLVM::ecs
{
	class InventorySystem : public ecs::ISystem {
	public:
		uint32_t crosshairArchetypesNumber   = 0;
		uint32_t inventoryArchetypesNumber  = 0;
		struct ArchView {
			arch::Archetype* crosshairCachedArchetype = nullptr;
			arch::Archetype* inventoryCachedArchetype = nullptr;
		} archView;
		
		struct ComponentsView {
			ecs::components::transform* crosshairTransformsView    = nullptr;
			
			ecs::components::transform* inventoryTransformsView = nullptr;
			ecs::components::inventory* inventoryView           = nullptr;
			ecs::components::mesh*      inventoryMeshesView     = nullptr;
		} componentsView;

		arch::componentMask crosshairRequiredMask =
			(1ull << ecs::arch::ComponentsIndices::TRANSFORM_COMPONENT) |
			(1ull << ecs::arch::ComponentsIndices::CROSSHAIR_TAG_COMPONENT);

		arch::componentMask inventoryRequiredMask =
			(1ull << ecs::arch::ComponentsIndices::TRANSFORM_COMPONENT) |
			(1ull << ecs::arch::ComponentsIndices::INVENTORY_COMPONENT) |
			(1ull << ecs::arch::ComponentsIndices::MESH_COMPONENT);
		
		void Update() override;
		int determineSwappableStatusAndSlots( components::item* itemComponent, components::transform* inventoryTransformComponent,
											  core::vector<unsigned int>& potentialOccupiedSlots, components::transform* crosshairTransformComponent,
											  point2D<int> intersectionSlot, components::inventory* inventoryComponent, const float inventorySlotScale );
		void fillInventorySlots( components::item* itemComponent, const int itemWidth, const int itemHeight,
								 components::inventory* inventoryComponent, const int fillValue );
		int determineSwappableField( components::item* itemComponent, const int itemWidth, const int itemHeight,
									 int pivotRow, int pivotColumn, components::inventory* inventoryComponent,
									 core::vector<unsigned int>& potentialOccupiedSlots );
		int calculateBasicOffset( const int itemAxisSize, const float axisValue,
								  const float crosshairAxisPosition, const int axisSlotIndex,
								  const float inventorySlotScale );
		bool checkCrosshairInventoryIntersection( components::transform* crosshairTransformComponent, components::transform* inventoryTransformComponent,
												  components::inventory* inventoryComponent, const float inventorySlotScale, const float inventorySlotHalfScale );
		point2D<int> determineActualIntersectionSlot( components::transform* crosshairTransformComponent, components::transform* inventoryTransformComponent,
												 const float inventorySlotScale, const float inventorySlotHalfScale );

		bool          isInventoryOpened         = false;
		int*          isItemDraged              = nullptr;
		bool*         isLeftMouseButtonReleased = nullptr;
		bool          isLeftMouseButtonPressed  = false;
		float         mouseOffsetX = 0;
		float         mouseOffsetY = 0;
		float         aspectRate   = 1.778;                   ///< Multiplier of current aspect rate. For full hd this must be 1920 / 1080
		arch::Archetype* cachedCrosshairArchetype;
		arch::Archetype* cachedInventoryArchetype;
	};
} ///< namespace GLVM::ecs


#endif
