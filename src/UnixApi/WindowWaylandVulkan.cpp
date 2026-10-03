#include "UnixApi/WindowWaylandVulkan.hpp"
#include "GraphicAPI/Vulkan.hpp"
#include "UnixApi/pointer-constraints-unstable-v1-client-protocol.h"
#include <algorithm>
#include <cerrno>
#include <vulkan/vulkan_core.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>
#include <poll.h>

namespace GLVM::core {
	static WindowWaylandVulkan windowWaylandVulkan;

	namespace {
		/// Linux evdev key codes (linux/input-event-codes.h). They are layout independent: WASD stays in place on any layout.
		constexpr uint32_t KEY_CODE_ESC   = 1;
		constexpr uint32_t KEY_CODE_W     = 17;
		constexpr uint32_t KEY_CODE_I     = 23;
		constexpr uint32_t KEY_CODE_O     = 24;
		constexpr uint32_t KEY_CODE_A     = 30;
		constexpr uint32_t KEY_CODE_S     = 31;
		constexpr uint32_t KEY_CODE_D     = 32;
		constexpr uint32_t KEY_CODE_SPACE = 57;
		constexpr uint32_t BUTTON_LEFT    = 0x110;                            ///< BTN_LEFT

		constexpr int CURSOR_SIZE = 64;

		void sendInputEvent( EEvents event ) {
			g_eEvent.SetEvent(event);
			Input_Stack_.ControlInput(g_eEvent);
		}
	}

	WindowWaylandVulkan::WindowWaylandVulkan() {}

	void WindowWaylandVulkan::init() {
		// connects your client application to the Wayland display server
		display  = wl_display_connect(nullptr);
		if ( display == nullptr ) {
			const char* waylandDisplay = getenv("WAYLAND_DISPLAY");
			fprintf(stderr, "Error: can't connect to the Wayland display (WAYLAND_DISPLAY=%s).\n"
					"This build uses the Wayland backend (VK_USE_PLATFORM_WAYLAND_KHR in include/GraphicAPI/Vulkan.hpp); "
					"run it inside a Wayland session or rebuild with the X11 backend.\n",
					waylandDisplay ? waylandDisplay : "unset");
			exit(EXIT_FAILURE);
		}
		/* get the global registry object from the Wayland display server (compositor). This registry allows
		   the client to discover available global objects, such as wl_compositor, wl_shm, xdg_wm_base, etc.,
		   which are needed to create surfaces and interact with the window system.
		*/
		registry = wl_display_get_registry(display);
		/* used to attach a listener (callback functions) to the Wayland registry object (wl_registry) so
		   that your client can respond to announcements about global objects provided by the compositor
		*/
		wl_registry_add_listener( registry, &registry_listener, (void*)this );
		/* synchronize the client with the Wayland compositor
		   1. global object announcements (e.g., from wl_registry)
		   2. event responses to previously sent requests
		*/
		wl_display_roundtrip( display );
		if (!compositor || !xdg_shell) {
			fprintf(stderr, "Error: the compositor doesn't provide wl_compositor or xdg_wm_base\n");
			exit(EXIT_FAILURE);
		}
		if ( !seat )
			fprintf(stderr, "Warning: the compositor doesn't provide wl_seat, keyboard and mouse input are unavailable\n");
		if ( !pointer_constraints || !relative_pointer_manager )
			fprintf(stderr, "Warning: the compositor lacks pointer-constraints or relative-pointer protocol, mouse look works only while the cursor is inside the window\n");

		/* create a new surface — which is essentially a drawable area in the Wayland compositor. This
		   surface becomes the foundation for windows, popups, and anything visual in a Wayland client
		*/
		wl_surface = wl_compositor_create_surface( compositor );
		pointer_surface = wl_compositor_create_surface(compositor);

		/* create a top-level window or popup window from a given wl_surface.
		   wraps a wl_surface with an XDG surface, which provides window management features
		*/
		xdg_surface = xdg_wm_base_get_xdg_surface( xdg_shell, wl_surface );
		/* Attach event handlers (callbacks) to an xdg_surface so your application can respond to events
		   from the compositor. When something happens to this surface (like resize, configure, etc.),
		   please call these functions.
		*/
		xdg_surface_add_listener( xdg_surface, &xdg_surface_listener, (void*)this );
		/* Turn a basic xdg_surface into a toplevel window — like a normal app window with borders,
		   title bar, and so on.
		*/
		xdg_topLevel = xdg_surface_get_toplevel( xdg_surface );
		xdg_toplevel_add_listener( xdg_topLevel, &xdg_toplevel_listener, (void*)this );
		xdg_toplevel_set_title( xdg_topLevel, "wayland glvm client" );
		xdg_toplevel_set_app_id( xdg_topLevel, "glvm" );
		/* Commit the changes made to a Wayland surface, notifying the compositor to render those
		   changes to the screen.
		*/
		wl_surface_commit( wl_surface );

		/* xdg-shell forbids attaching a buffer (Vulkan present) before the first configure is acknowledged,
		   so wait for it here.
		*/
		while ( !configured ) {
			if ( wl_display_dispatch( display ) < 0 ) {
				reportDisplayError();
				exit(EXIT_FAILURE);
			}
		}
	}

	void WindowWaylandVulkan::reportDisplayError() {
		if ( connectionLost )
			return;
		connectionLost = true;

		const int error = display ? wl_display_get_error( display ) : 0;
		fprintf(stderr, "Error: Wayland connection failed: %s\n", error ? strerror(error) : "unknown error");
		if ( error == EPROTO ) {
			const struct wl_interface* interface = nullptr;
			uint32_t id = 0;
			const uint32_t code = wl_display_get_protocol_error( display, &interface, &id );
			fprintf(stderr, "Wayland protocol error %u on %s@%u\n", code, interface ? interface->name : "unknown", id);
		}
	}

	bool WindowWaylandVulkan::HandleEvent(CEvent& _Event) {
		if ( display == nullptr || connectionLost ) {
			sendInputEvent(EEvents::eGAME_LOOP_KILL);
			return false;
		}

		bool isDisplayOk = true;
		while( wl_display_prepare_read( display ) != 0 ) {
			if ( wl_display_dispatch_pending( display ) < 0 ) {                 ///< On error the queue isn't drained: don't spin forever
				isDisplayOk = false;
				break;
			}
		}

		if ( isDisplayOk ) {
			wl_display_flush( display );
			struct pollfd pfd = { wl_display_get_fd( display ), POLLIN, 0 };
			if( poll( &pfd, 1, 0 ) > 0 ) {
				if ( wl_display_read_events( display ) < 0 )
					isDisplayOk = false;
			} else {
				wl_display_cancel_read( display );
			}
		}

		if ( isDisplayOk && wl_display_dispatch_pending( display ) < 0 )
			isDisplayOk = false;

		if ( !isDisplayOk ) {
			reportDisplayError();
			sendInputEvent(EEvents::eGAME_LOOP_KILL);
		}

		if ( close_xdg_toplevel ) {                                             ///< Close button / Alt+F4 / "Quit" from the compositor
			sendInputEvent(EEvents::eGAME_LOOP_KILL);
		}

		/* Mouse delta of this frame: sum of all motion events dispatched above. Only whole pixels are
		   handed out, the fractional rest stays for the next frame so slow movements are not lost.
		*/
		const int deltaX = (int)relativeMotionX;
		const int deltaY = (int)relativeMotionY;
		relativeMotionX -= deltaX;
		relativeMotionY -= deltaY;

		_Event.mousePointerPosition.position_X = deltaX;
		_Event.mousePointerPosition.position_Y = deltaY;
		_Event.mousePointerPosition.offset_X   = deltaX;
		_Event.mousePointerPosition.offset_Y   = deltaY;
 		return false;
	}

	/// Transparent cursor image, used to hide the cursor over the window
	struct wl_buffer* WindowWaylandVulkan::create_transparent_cursor(struct wl_shm *shm) {
		if ( shm == nullptr )
			return nullptr;

		const int stride = CURSOR_SIZE * 4;
		const int size   = stride * CURSOR_SIZE;
		const int32_t file_descriptor = alocate_shared_memory( size );
		if ( file_descriptor < 0 )
			return nullptr;

		void* data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
		if ( data == MAP_FAILED ) {
			close(file_descriptor);
			return nullptr;
		}
		memset(data, 0, size);                                                   ///< Fully transparent ARGB pixels
		munmap(data, size);

		struct wl_shm_pool *pool = wl_shm_create_pool(shm, file_descriptor, size);
		struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, CURSOR_SIZE, CURSOR_SIZE, stride, WL_SHM_FORMAT_ARGB8888);
		wl_shm_pool_destroy(pool);
		close(file_descriptor);

		return buffer;
	}

	void WindowWaylandVulkan::hideCursor(struct wl_pointer* _pointer, uint32_t serial) {
		if ( cursor_buffer == nullptr ) {
			cursor_buffer = create_transparent_cursor( shared_memory );
			if ( cursor_buffer == nullptr ) {
				fprintf(stderr, "Warning: can't create a transparent cursor, the cursor stays visible\n");
				return;
			}
			wl_surface_attach( pointer_surface, cursor_buffer, 0, 0 );
			wl_surface_damage( pointer_surface, 0, 0, CURSOR_SIZE, CURSOR_SIZE );
			wl_surface_commit( pointer_surface );
		}
		wl_pointer_set_cursor( _pointer, serial, pointer_surface, 0, 0 );
	}

	/// First click into the window: hide the cursor, lock it and start receiving relative motion
	void WindowWaylandVulkan::capturePointer(struct wl_pointer* _pointer, uint32_t serial) {
		hideAndLockPointer = true;
		hideCursor( _pointer, serial );

		if ( pointer_constraints != nullptr && locked_pointer == nullptr ) {
			// Lock pointer to main window surface, not pointer_surface
			locked_pointer = zwp_pointer_constraints_v1_lock_pointer( pointer_constraints, wl_surface, _pointer, NULL,
																	  ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT );
		}

		if ( relative_pointer_manager != nullptr && relative_pointer == nullptr ) {
			relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer( relative_pointer_manager, _pointer );
			zwp_relative_pointer_v1_add_listener( relative_pointer, &relative_pointer_listener, (void*)this );
		}
	}

	void WindowWaylandVulkan::SwapBuffers() {
	};
	void WindowWaylandVulkan::ClearDisplay() {
	};
	/// The compositor keeps the pointer locked, HandleEvent already put this frame's delta into the position
	void WindowWaylandVulkan::CursorLock(int _x_position, int _y_position, int* _x_offset, int* _y_offset) {
		*_x_offset = _x_position;
		*_y_offset = _y_position;
	};

	void WindowWaylandVulkan::Close() {
		if ( display == nullptr )
			return;

		if ( relative_pointer )
			zwp_relative_pointer_v1_destroy( relative_pointer );
		if ( locked_pointer )
			zwp_locked_pointer_v1_destroy( locked_pointer );
		if ( pointer )
			wl_pointer_destroy( pointer );
		if ( keyboard )
			wl_keyboard_destroy( keyboard );
		/* Release a Wayland seat object, which is responsible for managing input devices like
		   keyboards, mice, or touchscreens. The seat is bound with version 1, wl_seat.release needs version 5.
		*/
		if ( seat )
			wl_seat_destroy( seat );
		if ( cursor_buffer )
			wl_buffer_destroy( cursor_buffer );
		if ( xdg_topLevel )
			xdg_toplevel_destroy( xdg_topLevel );
		if ( xdg_surface )
			xdg_surface_destroy( xdg_surface );
		if ( pointer_surface )
			wl_surface_destroy( pointer_surface );
		if ( wl_surface )
			wl_surface_destroy( wl_surface );
		wl_display_disconnect( display );

		relative_pointer = nullptr;
		locked_pointer   = nullptr;
		pointer          = nullptr;
		keyboard         = nullptr;
		seat             = nullptr;
		cursor_buffer    = nullptr;
		xdg_topLevel     = nullptr;
		xdg_surface      = nullptr;
		pointer_surface  = nullptr;
		wl_surface       = nullptr;
		display          = nullptr;
	}

	/// Anonymous shared memory file for wl_shm pools. Returns -1 on failure.
	int32_t alocate_shared_memory( uint64_t size ) {
		int32_t file_descriptor = memfd_create( "glvm-wayland-shm", MFD_CLOEXEC );
		if ( file_descriptor < 0 ) {
			perror("memfd_create");
			return -1;
		}
		/// File truncation means cutting off a file at a certain size — either shrinking it or expanding it.
		if ( ftruncate( file_descriptor, (off_t)size ) < 0 ) {
			perror("ftruncate");
			close( file_descriptor );
			return -1;
		}

		return file_descriptor;
	}

	void xdg_toplevel_configure( void* data, [[maybe_unused]] struct xdg_toplevel* xdg_toplevel, int32_t new_width, int32_t new_height, [[maybe_unused]] struct wl_array* atate ) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;

		/// Zero means "client decides" and is handled for each dimension separately
		if ( new_width > 0 && new_width <= UINT16_MAX && window->width != new_width ) {
			window->width = (uint16_t)new_width;
			window->resizePending = true;
		}
		if ( new_height > 0 && new_height <= UINT16_MAX && window->height != new_height ) {
			window->height = (uint16_t)new_height;
			window->resizePending = true;
		}
	}

	void xdg_toplevel_close( void* data, [[maybe_unused]] struct xdg_toplevel* xdg_toplevel ) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;

		window->close_xdg_toplevel = 1;
	}

	void xdg_toplevel_configure_bounds( [[maybe_unused]] void* data, [[maybe_unused]] struct xdg_toplevel* xdg_toplevel,
										[[maybe_unused]] int32_t width, [[maybe_unused]] int32_t height ) {
	}

	void xdg_toplevel_wm_capabilities( [[maybe_unused]] void* data, [[maybe_unused]] struct xdg_toplevel* xdg_toplevel,
									   [[maybe_unused]] struct wl_array* capabilities ) {
	}

	void xdg_surface_configure( void* data, struct xdg_surface* xdg_surface, uint32_t serial ) {
		/* Acknowledge a configure event sent by the Wayland compositor to your xdg_surface
		   In Wayland, when the compositor wants to change your window (like resizing it), it sends
		   a configure event to your surface. You must call xdg_surface_ack_configure() to confirm
		   that you received and accepted this change. If you don’t call it, your window won’t be
		   shown or updated properly.
		*/
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;

		xdg_surface_ack_configure( xdg_surface, serial );
		window->configured = true;
	}

	void shell_ping( [[maybe_unused]] void* data, struct xdg_wm_base* shell, uint32_t serial ) {
		xdg_wm_base_pong( shell, serial );
	}

	void keyboard_keymap([[maybe_unused]] void* data, [[maybe_unused]] struct wl_keyboard* keyboard, [[maybe_unused]] uint32_t format,
						 int32_t keymap_file_descriptor, [[maybe_unused]] uint32_t size) {
		close( keymap_file_descriptor );                                         ///< Raw key codes are used, the keymap isn't needed
	}

	void keyboard_enter([[maybe_unused]] void* data, [[maybe_unused]] struct wl_keyboard* keyboard, [[maybe_unused]] uint32_t serial,
						[[maybe_unused]] struct wl_surface* surface, [[maybe_unused]] struct wl_array* keys) {
	}

	/// Focus lost: release events of keys held now will never arrive, release everything
	void keyboard_leave([[maybe_unused]] void* data, [[maybe_unused]] struct wl_keyboard* keyboard, [[maybe_unused]] uint32_t serial,
						[[maybe_unused]] struct wl_surface* surface) {
		releaseHeldInput();
	}

	void keyboard_key([[maybe_unused]] void* data, [[maybe_unused]] struct wl_keyboard* keyboard, [[maybe_unused]] uint32_t serial,
					  [[maybe_unused]] uint32_t time, uint32_t key, uint32_t state) {
		if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			switch ( key ) {
			case KEY_CODE_ESC:   sendInputEvent(EEvents::eGAME_LOOP_KILL);          break;
			case KEY_CODE_W:     sendInputEvent(EEvents::eMOVE_FORWARD);            break;
			case KEY_CODE_S:     sendInputEvent(EEvents::eMOVE_BACKWARD);           break;
			case KEY_CODE_A:     sendInputEvent(EEvents::eMOVE_LEFT);               break;
			case KEY_CODE_D:     sendInputEvent(EEvents::eMOVE_RIGHT);              break;
			case KEY_CODE_SPACE: sendInputEvent(EEvents::eJUMP);                    break;
			case KEY_CODE_I:     sendInputEvent(EEvents::eINVENTORY);               break;      ///< Toggle, the engine removes it
			case KEY_CODE_O:     sendInputEvent(EEvents::eDEBUG_COLLISIONS_ACTIVE); break;      ///< Toggle, the engine removes it
			default: break;
			}
		}

		/* No release events for the toggles (I, O): the engine consumes them itself, and a release in the
		   same frame as the press would cancel the toggle.
		*/
		if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
			switch ( key ) {
			case KEY_CODE_W:     sendInputEvent(EEvents::eKEYRELEASE_W);    break;
			case KEY_CODE_S:     sendInputEvent(EEvents::eKEYRELEASE_S);    break;
			case KEY_CODE_A:     sendInputEvent(EEvents::eKEYRELEASE_A);    break;
			case KEY_CODE_D:     sendInputEvent(EEvents::eKEYRELEASE_D);    break;
			case KEY_CODE_SPACE: sendInputEvent(EEvents::eKEYRELEASE_JUMP); break;
			default: break;
			}
		}
	}

	void keyboard_modifiers([[maybe_unused]] void* data, [[maybe_unused]] struct wl_keyboard* keyboard, [[maybe_unused]] uint32_t serial,
							[[maybe_unused]] uint32_t mods_depressed, [[maybe_unused]] uint32_t mods_latched, [[maybe_unused]] uint32_t mods_locked, [[maybe_unused]] uint32_t group) {
	}

	void keyboard_repeat_info([[maybe_unused]] void* data, [[maybe_unused]] struct wl_keyboard* keyboard, [[maybe_unused]] int32_t rate,
							  [[maybe_unused]] int32_t delay) {
	}

	// Pointer listener callbacks
	void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, [[maybe_unused]] struct wl_surface *surface,
					   wl_fixed_t sx, wl_fixed_t sy) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;

		/// The cursor image is undefined after enter, hide it again once the pointer was captured
		if ( window->hideAndLockPointer )
			window->hideCursor( pointer, serial );

		window->lastPointerX = wl_fixed_to_double(sx);
		window->lastPointerY = wl_fixed_to_double(sy);
		window->hasLastPointerPosition = true;
	}

	void pointer_leave(void *data, [[maybe_unused]] struct wl_pointer *pointer,
					   [[maybe_unused]] uint32_t serial, [[maybe_unused]] struct wl_surface *surface) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;
		window->hasLastPointerPosition = false;

		/// A button held while leaving (Alt+Tab) never gets its release event
		sendInputEvent(EEvents::eMOUSE_LEFT_BUTTON_RELEASE);
		g_eEvent.isLeftMouseButtonReleased = true;
	}

	/// Absolute motion is used only if the compositor has no relative pointer protocol (no motion is sent while locked)
	void pointer_motion(void *data, [[maybe_unused]] struct wl_pointer *pointer,
						[[maybe_unused]] uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;
		const double x = wl_fixed_to_double(sx);
		const double y = wl_fixed_to_double(sy);

		if ( window->relative_pointer == nullptr && window->hideAndLockPointer && window->hasLastPointerPosition ) {
			window->relativeMotionX += x - window->lastPointerX;
			window->relativeMotionY += y - window->lastPointerY;
		}
		window->lastPointerX = x;
		window->lastPointerY = y;
		window->hasLastPointerPosition = true;
	}

	void pointer_axis([[maybe_unused]] void *data, [[maybe_unused]] struct wl_pointer *pointer,
					  [[maybe_unused]] uint32_t time, [[maybe_unused]] uint32_t axis, [[maybe_unused]] wl_fixed_t value) {
	}

	void pointer_frame([[maybe_unused]] void *data, [[maybe_unused]] struct wl_pointer *pointer) {
	}

	void pointer_axis_source([[maybe_unused]] void *data, [[maybe_unused]] struct wl_pointer *pointer, [[maybe_unused]] uint32_t axis_source) {
	}

	void pointer_axis_stop([[maybe_unused]] void *data, [[maybe_unused]] struct wl_pointer *pointer,
						   [[maybe_unused]] uint32_t time, [[maybe_unused]] uint32_t axis) {
	}

	void pointer_axis_discrete([[maybe_unused]] void *data, [[maybe_unused]] struct wl_pointer *pointer,
							   [[maybe_unused]] uint32_t axis, [[maybe_unused]] int32_t discrete) {
	}

	void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, [[maybe_unused]] uint32_t time,
						uint32_t button, uint32_t state) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;

		if ( state == WL_POINTER_BUTTON_STATE_PRESSED && button == BUTTON_LEFT ) {
			sendInputEvent(EEvents::eMOUSE_LEFT_BUTTON);
		}
		if ( state == WL_POINTER_BUTTON_STATE_RELEASED && button == BUTTON_LEFT ) {
			sendInputEvent(EEvents::eMOUSE_LEFT_BUTTON_RELEASE);
			g_eEvent.isLeftMouseButtonReleased = true;
		}

		// Hide and lock the cursor on the first click into the window
		if ( !window->hideAndLockPointer ) {
			window->capturePointer( pointer, serial );
		}
	}

	/// Mouse motion for camera: accumulated (not overwritten) with sub-pixel precision until HandleEvent consumes it
	void handle_relative_motion(void *data, [[maybe_unused]] struct zwp_relative_pointer_v1 *rel_pointer,
								[[maybe_unused]] uint32_t utime_hi, [[maybe_unused]] uint32_t utime_lo,
								wl_fixed_t dx, wl_fixed_t dy, [[maybe_unused]] wl_fixed_t dx_unaccel, [[maybe_unused]] wl_fixed_t dy_unaccel) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;
		window->relativeMotionX += wl_fixed_to_double(dx);
		window->relativeMotionY += wl_fixed_to_double(dy);
	}

	void seat_capabilities( void* data, struct wl_seat* seat, uint32_t capabilities ) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;

		// Handle pointer capabilities
		if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !window->pointer) {
			window->pointer = wl_seat_get_pointer(seat);
			wl_pointer_add_listener(window->pointer, &window->pointer_listener, data);
		} else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && window->pointer) {
			if ( window->relative_pointer ) {
				zwp_relative_pointer_v1_destroy( window->relative_pointer );
				window->relative_pointer = nullptr;
			}
			if ( window->locked_pointer ) {
				zwp_locked_pointer_v1_destroy( window->locked_pointer );
				window->locked_pointer = nullptr;
			}
			wl_pointer_destroy(window->pointer);
			window->pointer = NULL;
			window->hideAndLockPointer = false;
		}

		if ( (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !window->keyboard ) {
			window->keyboard = wl_seat_get_keyboard( seat );
			wl_keyboard_add_listener( window->keyboard, &window->keyboard_listener, data );
		} else if ( !(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && window->keyboard ) {
			wl_keyboard_destroy( window->keyboard );
			window->keyboard = nullptr;
			releaseHeldInput();
		}
	}

	void seat_name( [[maybe_unused]] void* data, [[maybe_unused]] struct wl_seat* seat, [[maybe_unused]] const char* name ) {
	}

	void registry_global( void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version ) {
		WindowWaylandVulkan* window = (WindowWaylandVulkan*)data;

		if (!strcmp( interface, wl_compositor_interface.name )) {
			window->compositor = (wl_compositor*)wl_registry_bind( registry, name, &wl_compositor_interface, std::min<uint32_t>(version, 4) );
		} else if (!strcmp( interface, wl_shm_interface.name )) {
			window->shared_memory = (wl_shm*)wl_registry_bind( registry, name, &wl_shm_interface, 1 );
		} else if (!strcmp( interface, zwp_pointer_constraints_v1_interface.name )) {
			window->pointer_constraints = (zwp_pointer_constraints_v1*)wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1);
		} else if (!strcmp( interface, zwp_relative_pointer_manager_v1_interface.name)) {
			window->relative_pointer_manager = (zwp_relative_pointer_manager_v1*)wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1);
		} else if (!strcmp( interface, xdg_wm_base_interface.name )) {
			window->xdg_shell = (xdg_wm_base*)wl_registry_bind( registry, name, &xdg_wm_base_interface, 1 );
			xdg_wm_base_add_listener( window->xdg_shell, &window->shell_listener, 0 );
		} else if (!strcmp( interface, wl_seat_interface.name ) && window->seat == nullptr) {
			/// Version 1: wl_pointer then sends only enter/leave/motion/button/axis (all handled)
			window->seat = (wl_seat*)wl_registry_bind( registry, name, &wl_seat_interface, 1 );
			wl_seat_add_listener( window->seat, &window->seat_lintener, data );
		}
	}

	void registry_global_remove( [[maybe_unused]] void* data, [[maybe_unused]] struct wl_registry* registry, [[maybe_unused]] uint32_t name) {
	}

	WindowWaylandVulkan*  initializeWaylandWindow() {
		/* Listeners are zero-initialized and filled field by field (no designated initializers): new libwayland
		   versions add fields (e.g. wl_pointer_listener::warp in 1.24), old ones lack them (axis_relative_direction
		   before 1.22). Events above the bound interface versions are never sent, so unset fields are never called.
		*/
		windowWaylandVulkan.xdg_toplevel_listener = {};
		windowWaylandVulkan.xdg_toplevel_listener.configure        = xdg_toplevel_configure;
		windowWaylandVulkan.xdg_toplevel_listener.close            = xdg_toplevel_close;
		windowWaylandVulkan.xdg_toplevel_listener.configure_bounds = xdg_toplevel_configure_bounds;
		windowWaylandVulkan.xdg_toplevel_listener.wm_capabilities  = xdg_toplevel_wm_capabilities;

		windowWaylandVulkan.xdg_surface_listener = {};
		windowWaylandVulkan.xdg_surface_listener.configure = xdg_surface_configure;

		windowWaylandVulkan.shell_listener = {};
		windowWaylandVulkan.shell_listener.ping = shell_ping;

		windowWaylandVulkan.keyboard_listener = {};
		windowWaylandVulkan.keyboard_listener.keymap      = keyboard_keymap;
		windowWaylandVulkan.keyboard_listener.enter       = keyboard_enter;
		windowWaylandVulkan.keyboard_listener.leave       = keyboard_leave;
		windowWaylandVulkan.keyboard_listener.key         = keyboard_key;
		windowWaylandVulkan.keyboard_listener.modifiers   = keyboard_modifiers;
		windowWaylandVulkan.keyboard_listener.repeat_info = keyboard_repeat_info;

		windowWaylandVulkan.relative_pointer_listener = {};
		windowWaylandVulkan.relative_pointer_listener.relative_motion = handle_relative_motion;

		windowWaylandVulkan.pointer_listener = {};
		windowWaylandVulkan.pointer_listener.enter         = pointer_enter;
		windowWaylandVulkan.pointer_listener.leave         = pointer_leave;
		windowWaylandVulkan.pointer_listener.motion        = pointer_motion;
		windowWaylandVulkan.pointer_listener.button        = pointer_button;
		windowWaylandVulkan.pointer_listener.axis          = pointer_axis;
		windowWaylandVulkan.pointer_listener.frame         = pointer_frame;
		windowWaylandVulkan.pointer_listener.axis_source   = pointer_axis_source;
		windowWaylandVulkan.pointer_listener.axis_stop     = pointer_axis_stop;
		windowWaylandVulkan.pointer_listener.axis_discrete = pointer_axis_discrete;

		windowWaylandVulkan.seat_lintener = {};
		windowWaylandVulkan.seat_lintener.capabilities = seat_capabilities;
		windowWaylandVulkan.seat_lintener.name         = seat_name;

		windowWaylandVulkan.registry_listener = {};
		windowWaylandVulkan.registry_listener.global        = registry_global;
		windowWaylandVulkan.registry_listener.global_remove = registry_global_remove;

		windowWaylandVulkan.init();
		return &windowWaylandVulkan;
	}
}; ///< namespace GLVM::core
