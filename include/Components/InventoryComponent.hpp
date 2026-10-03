#ifndef INVENTORY_COMPONENT_HPP
#define INVENTORY_COMPONENT_HPP

#include "Components/InventorySlotComponent.hpp"
#include <climits>
#include "Components/VertexComponent.hpp"
#include "Vector.hpp"

namespace GLVM::ecs::components
{
	/*
	  Slots are stored inline, so implicitly generated copy/move constructors, assignments and destructor
	  are correct (rule of zero). It is required by swap-remove of archetype components.
	*/
	class inventory {
	public:
		static constexpr unsigned int MAX_ROWS    = 8;
		static constexpr unsigned int MAX_COLUMNS = 8;

		inventory() {
			for ( unsigned int i = 0; i < MAX_ROWS; ++i )
				for ( unsigned int j = 0; j < MAX_COLUMNS; ++j )
					slots[i][j] = UINT_MAX;
		}

		unsigned int row = MAX_ROWS;
		unsigned int col = MAX_COLUMNS;
		// int slots[row][col] = {
		// 	{ -1, -1, -1, -1, -1, -1, -1, -1 },
		// 	{ -1, -1, -1, -1, -1, -1, -1, -1 },
		// 	{ -1, -1, -1, -1, -1, -1, -1, -1 },
		// 	{ -1, -1, -1, -1, -1, -1, -1, -1 },
		// 	{ -1, -1, -1, -1, -1, -1, -1, -1 },
		// 	{ -1, -1, -1, -1, -1, -1, -1, -1 },
		// 	{ -1, -1, -1, -1, -1, -1, -1, -1 },
		// 	{ -1, -1, -1, -1, -1, -1, -1, -1 },
		// };

		unsigned int slots[MAX_ROWS][MAX_COLUMNS];           ///< Array with entities contained inventorySlotComponents. UINT_MAX is an empty slot
		unsigned int entityOwner = UINT_MAX;
		core::vector<unsigned int> highlightedSlots;
		bool isAvailableHighlightedSlots = false;
		MeshHandle slotMeshID;
		float slotScale = 0.05f;
	};
}; // namespace GLVM::ecs::components


#endif
