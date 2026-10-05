// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__) && !defined(VK_USE_PLATFORM_XLIB_KHR) && !defined(VK_USE_PLATFORM_XCB_KHR)
#define VK_USE_PLATFORM_WAYLAND_KHR                                         ///< Same default as include/GraphicAPI/Vulkan.hpp
#endif

#include "DemoPlatform.hpp"
#include "Globals.hpp"
#include "EventsStack.hpp"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#include "WinApi/WindowWinVulkan.hpp"
#include <vulkan/vulkan_win32.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include "UnixApi/WindowWaylandVulkan.hpp"
#include <vulkan/vulkan_wayland.h>
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#include "UnixApi/WindowXCBVulkan.hpp"
#include <vulkan/vulkan_xcb.h>
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#include "UnixApi/WindowXVulkan.hpp"
#include <vulkan/vulkan_xlib.h>
#endif

#include <stdexcept>

/// The engine window classes report input through these globals (defined by Engine.cpp in the game).
GLVM::core::CEvent g_eEvent{};

namespace GLVM::fluid::demo
{
	using GLVM::core::EEvents;

	struct DemoWindow::Implementation {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		GLVM::core::WindowWinVulkan* window = nullptr;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
		GLVM::core::WindowWaylandVulkan* window = nullptr;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		GLVM::core::WindowXCBVulkan* window = nullptr;
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		GLVM::core::WindowXVulkan* window = nullptr;
#endif
	};

	DemoWindow::DemoWindow() : implementation_( std::make_unique<Implementation>() ) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		implementation_->window = new GLVM::core::WindowWinVulkan();
		SetWindowTextA( implementation_->window->GetModernWindowHWND(), "GLVM fluid demo" );
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
		implementation_->window = GLVM::core::initializeWaylandWindow();
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		implementation_->window = new GLVM::core::WindowXCBVulkan();
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		implementation_->window = new GLVM::core::WindowXVulkan();
#endif
		if ( implementation_->window == nullptr )
			throw std::runtime_error( "can't create the window" );
	}

	DemoWindow::~DemoWindow() {
		implementation_->window->Close();
		delete implementation_->window;
	}

	std::vector<const char*> DemoWindow::requiredInstanceExtensions() {
		std::vector<const char*> extensions = { VK_KHR_SURFACE_EXTENSION_NAME };
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		extensions.push_back( VK_KHR_WIN32_SURFACE_EXTENSION_NAME );
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
		extensions.push_back( VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME );
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		extensions.push_back( VK_KHR_XCB_SURFACE_EXTENSION_NAME );
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		extensions.push_back( VK_KHR_XLIB_SURFACE_EXTENSION_NAME );
#endif
		return extensions;
	}

	VkSurfaceKHR DemoWindow::createSurface( VkInstance instance ) const {
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkResult result = VK_ERROR_INITIALIZATION_FAILED;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VkWin32SurfaceCreateInfoKHR surfaceInfo{};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.hinstance = GetModuleHandleA( nullptr );
		surfaceInfo.hwnd      = implementation_->window->GetModernWindowHWND();
		result = vkCreateWin32SurfaceKHR( instance, &surfaceInfo, nullptr, &surface );
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
		VkWaylandSurfaceCreateInfoKHR surfaceInfo{};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.display = implementation_->window->display;
		surfaceInfo.surface = implementation_->window->wl_surface;
		result = vkCreateWaylandSurfaceKHR( instance, &surfaceInfo, nullptr, &surface );
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		VkXcbSurfaceCreateInfoKHR surfaceInfo{};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.connection = implementation_->window->GetConnection();
		surfaceInfo.window     = implementation_->window->GetWindow();
		result = vkCreateXcbSurfaceKHR( instance, &surfaceInfo, nullptr, &surface );
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		VkXlibSurfaceCreateInfoKHR surfaceInfo{};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.dpy    = implementation_->window->GetDisplay();
		surfaceInfo.window = implementation_->window->GetWindow();
		result = vkCreateXlibSurfaceKHR( instance, &surfaceInfo, nullptr, &surface );
#endif
		if ( result != VK_SUCCESS )
			throw std::runtime_error( "can't create the Vulkan surface" );
		return surface;
	}

	uint32_t DemoWindow::width() const { return (uint32_t)implementation_->window->width; }
	uint32_t DemoWindow::height() const { return (uint32_t)implementation_->window->height; }

	DemoInput DemoWindow::poll() {
		DemoInput input;
		g_eEvent.SetEvent( EEvents::eDEFAULT );
		implementation_->window->HandleEvent( g_eEvent );

		auto isActive = []( EEvents event ) { return Input_Stack_.SearchElement( event ) == event; };
		input.quit     = isActive( EEvents::eGAME_LOOP_KILL );
		input.forward  = isActive( EEvents::eMOVE_FORWARD );
		input.backward = isActive( EEvents::eMOVE_BACKWARD );
		input.left     = isActive( EEvents::eMOVE_LEFT );
		input.right    = isActive( EEvents::eMOVE_RIGHT );

		/// Space and the left button are held states: presses are their rising edges.
		const bool jump = isActive( EEvents::eJUMP );
		input.nextScenario = jump && !previousJump_;
		previousJump_ = jump;
		input.buttonHeld = isActive( EEvents::eMOUSE_LEFT_BUTTON );
		input.click = input.buttonHeld && !previousButton_;
		previousButton_ = input.buttonHeld;

		/// I and O are toggles that the consumer removes.
		if ( isActive( EEvents::eINVENTORY ) ) {
			input.nextMode = true;
			Input_Stack_.Remove( EEvents::eINVENTORY );
		}
		if ( isActive( EEvents::eDEBUG_COLLISIONS_ACTIVE ) ) {
			input.togglePause = true;
			Input_Stack_.Remove( EEvents::eDEBUG_COLLISIONS_ACTIVE );
		}

		implementation_->window->CursorLock( g_eEvent.mousePointerPosition.position_X, g_eEvent.mousePointerPosition.position_Y,
											 &input.mouseDeltaX, &input.mouseDeltaY );
		return input;
	}
}
