#ifndef WINDOW_WAYLAND_VULKAN
#define WINDOW_WAYLAND_VULKAN

#include <cstdint>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include "UnixApi/xdg-shell-client-protocol.h"
#include "UnixApi/pointer-constraints-unstable-v1-client-protocol.h"
#include "UnixApi/relative-pointer-unstable-v1-client-protocol.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "IWindow.hpp"
#include "Globals.hpp"
#include "EventsStack.hpp"

namespace GLVM::core {
	struct WindowWaylandVulkan : IWindow {
		WindowWaylandVulkan();
		void init();
		void Close() override;
		bool HandleEvent(CEvent& _Event) override;
		struct wl_buffer *create_transparent_cursor(struct wl_shm *shm);
		void SwapBuffers() override;
        void ClearDisplay() override;
        void CursorLock(int _x_position, int _y_position, int* _x_offset, int* _y_offset) override;
		void capturePointer(struct wl_pointer* pointer, uint32_t serial);
		void hideCursor(struct wl_pointer* pointer, uint32_t serial);
		void reportDisplayError();

		bool hideAndLockPointer = false;                         ///< Pointer is captured (first click into the window)
		bool configured = false;                                 ///< First xdg_surface.configure was acknowledged
		bool resizePending = false;                              ///< Compositor asked for a new size (see width/height)
		bool connectionLost = false;
		double relativeMotionX = 0.0;                            ///< Mouse motion accumulated since the last HandleEvent (pixels)
		double relativeMotionY = 0.0;
		bool hasLastPointerPosition = false;                     ///< Fallback when relative pointer protocol is missing
		double lastPointerX = 0.0;
		double lastPointerY = 0.0;
		struct xdg_toplevel_listener xdg_toplevel_listener;
		struct xdg_surface_listener xdg_surface_listener;
		struct xdg_wm_base_listener shell_listener;
		struct wl_keyboard_listener keyboard_listener;
		struct zwp_relative_pointer_v1_listener relative_pointer_listener;
		struct wl_pointer_listener pointer_listener;
		struct wl_seat_listener seat_lintener;
		struct wl_registry_listener registry_listener;

		struct wl_surface*    wl_surface = nullptr;
		struct wl_compositor* compositor = nullptr;
		struct xdg_toplevel*  xdg_topLevel = nullptr;
		struct xdg_wm_base*   xdg_shell = nullptr;
		struct wl_buffer*     cursor_buffer = nullptr;
		struct wl_shm*        shared_memory = nullptr;
		struct wl_seat*       seat = nullptr;
		struct wl_keyboard*   keyboard = nullptr;
		struct wl_pointer*    pointer = nullptr;
		struct wl_surface*    pointer_surface = nullptr;
		struct zwp_pointer_constraints_v1 *pointer_constraints = nullptr;
		struct zwp_locked_pointer_v1* locked_pointer = nullptr;
		struct zwp_relative_pointer_manager_v1* relative_pointer_manager = nullptr;
		struct zwp_relative_pointer_v1* relative_pointer = nullptr;
		uint16_t width = 1920;
		uint16_t height = 1080;
		uint8_t  close_xdg_toplevel = 0;                         ///< Compositor asked to close the window
		struct wl_display*  display = nullptr;
		struct wl_registry* registry = nullptr;
		struct xdg_surface *xdg_surface = nullptr;
	};

	void xdg_toplevel_configure( void* data, struct xdg_toplevel* xdg_toplevel, int32_t new_width, int32_t new_height, struct wl_array* atate );
	void xdg_toplevel_close( void* data, struct xdg_toplevel* xdg_toplevel );
	void xdg_toplevel_configure_bounds( void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height );
	void xdg_toplevel_wm_capabilities( void* data, struct xdg_toplevel* xdg_toplevel, struct wl_array* capabilities );
	int32_t alocate_shared_memory( uint64_t size );
	void xdg_surface_configure( void* data, struct xdg_surface* xdg_surface, uint32_t serial );
	void shell_ping( void* data, struct xdg_wm_base* shell, uint32_t serial );
	void keyboard_keymap(void* data, struct wl_keyboard* keyboard, uint32_t format, int32_t keymap_file_descriptor, uint32_t size);
	void keyboard_enter(void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface, struct wl_array* keys);
	void keyboard_leave(void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface);
	void keyboard_key(void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
	void keyboard_modifiers(void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
	void keyboard_repeat_info(void* data, struct wl_keyboard* keyboard, int32_t rate, int32_t delay);
	void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy);
	void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface);
	void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
	void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
	void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
	void pointer_frame(void *data, struct wl_pointer *pointer);
	void pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t axis_source);
	void pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis);
	void pointer_axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete);
	void handle_relative_motion(void *data, struct zwp_relative_pointer_v1 *rel_pointer, uint32_t utime_hi, uint32_t utime_lo,
								wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel);
	void seat_capabilities( void* data, struct wl_seat* seat, uint32_t capabilities );
	void seat_name( void* data, struct wl_seat* seat, const char* name );
	void registry_global( void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version );
	void registry_global_remove( void* data, struct wl_registry* registry, uint32_t name);

	[[nodiscard]] WindowWaylandVulkan* initializeWaylandWindow();
}; ///< namespace GLVM::core

#endif
