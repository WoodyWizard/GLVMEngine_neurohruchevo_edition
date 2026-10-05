// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/// Window, Vulkan surface and input of the fluid demo, on top of the engine window classes
/// (Wayland, X11 or Win32, selected like in the engine renderer).

#ifndef GLVM_FLUID_DEMO_PLATFORM_HPP
#define GLVM_FLUID_DEMO_PLATFORM_HPP

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace GLVM::fluid::demo
{
	struct DemoInput {
		bool  quit = false;
		bool  forward = false, backward = false, left = false, right = false;   ///< W S A D held
		bool  nextScenario = false;                                         ///< Space pressed this frame
		bool  nextMode = false;                                             ///< I pressed this frame
		bool  togglePause = false;                                          ///< O pressed this frame
		bool  click = false;                                                ///< Left button pressed this frame
		bool  buttonHeld = false;
		int   mouseDeltaX = 0, mouseDeltaY = 0;
	};

	class DemoWindow {
	public:
		DemoWindow();
		~DemoWindow();
		DemoWindow( const DemoWindow& ) = delete;
		DemoWindow& operator=( const DemoWindow& ) = delete;

		static std::vector<const char*> requiredInstanceExtensions();
		VkSurfaceKHR createSurface( VkInstance instance ) const;
		uint32_t width() const;
		uint32_t height() const;
		/// Processes the window events of this frame.
		DemoInput poll();

	private:
		struct Implementation;
		std::unique_ptr<Implementation> implementation_;
		bool previousJump_ = false;
		bool previousButton_ = false;
	};
}

#endif
