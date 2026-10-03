// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "UnixApi/WindowXCBVulkan.hpp"
#include "Event.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

namespace GLVM::core
{
	namespace
	{
		/// X11 key codes of evdev/libinput servers (Xorg, XWayland) are evdev codes + 8: layout independent, same keys as Wayland
		constexpr xcb_keycode_t XKEYCODE_ESCAPE = 1 + 8;
		constexpr xcb_keycode_t XKEYCODE_W      = 17 + 8;
		constexpr xcb_keycode_t XKEYCODE_I      = 23 + 8;
		constexpr xcb_keycode_t XKEYCODE_O      = 24 + 8;
		constexpr xcb_keycode_t XKEYCODE_A      = 30 + 8;
		constexpr xcb_keycode_t XKEYCODE_S      = 31 + 8;
		constexpr xcb_keycode_t XKEYCODE_D      = 32 + 8;
		constexpr xcb_keycode_t XKEYCODE_SPACE  = 57 + 8;
	}

	WindowXCBVulkan::WindowXCBVulkan() {
		/// Open the connection to the X server
		connection = xcb_connect ( NULL, NULL );
		int error = xcb_connection_has_error(connection);
		if (error) {
			fprintf(stderr, "Error: XCB connection error %d (DISPLAY=%s)\n", error, getenv("DISPLAY") ? getenv("DISPLAY") : "unset");
			exit(EXIT_FAILURE);
		}

		/// Get the first screen
		const xcb_setup_t*    setup    = xcb_get_setup ( connection );
		xcb_screen_iterator_t iterator = xcb_setup_roots_iterator ( setup );
		screen                         = iterator.data;
		if ( screen == nullptr ) {
			fprintf(stderr, "Error: X server has no screens\n");
			exit(EXIT_FAILURE);
		}

		uint32_t event_mask = 0;
		event_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
		uint32_t event_flags[2];
		event_flags[0] = screen->black_pixel;
		event_flags[1] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
			XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE |
			XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
			XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_FOCUS_CHANGE;

		/// Create window
		window = xcb_generate_id ( connection );
		xcb_create_window ( connection,                      ///< Connection
							XCB_COPY_FROM_PARENT,            ///< Depth (same as root)
							window,                          ///< Window id
							screen->root,                    ///< Parent window
							0, 0,                            ///< x, y
							width, height,                   ///< width, height
							10,                              ///< Border width
							XCB_WINDOW_CLASS_INPUT_OUTPUT,   ///< Class
							screen->root_visual,             ///< Visual
							event_mask, event_flags );       ///< Masks

		const char* title = "glvm";
		xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(title), title);

		/// Ask the window manager for a WM_DELETE_WINDOW message instead of killing the connection on close
		const xcb_atom_t wm_protocols = InternAtom("WM_PROTOCOLS");
		wm_delete_window = InternAtom("WM_DELETE_WINDOW");
		if ( wm_protocols != XCB_ATOM_NONE && wm_delete_window != XCB_ATOM_NONE )
			xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, wm_protocols, XCB_ATOM_ATOM, 32, 1, &wm_delete_window);

		/// Map the window on the screen
		xcb_map_window ( connection, window );

		HideCursor();

		/// Make sure commands are sent befour we pause so that the window gets shown
		xcb_flush ( connection );
	}

	xcb_atom_t WindowXCBVulkan::InternAtom(const char* name) {
		xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, strlen(name), name);
		xcb_intern_atom_reply_t* reply  = xcb_intern_atom_reply(connection, cookie, NULL);
		if ( reply == nullptr )
			return XCB_ATOM_NONE;
		const xcb_atom_t atom = reply->atom;
		free(reply);
		return atom;
	}

	void WindowXCBVulkan::configureWindow() {
		uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
		const uint32_t values[] = {
			320,    /* x */
			180,    /* y */
			width,
			height
		};

		xcb_configure_window(connection, window, mask, values);
		xcb_flush(connection);
	}

	/// Fully transparent 8x8 cursor (all-zero mask)
	void WindowXCBVulkan::HideCursor() {
		xcb_pixmap_t pixmap_id = xcb_generate_id (connection);
		xcb_create_pixmap(connection, 1, pixmap_id, window, 8, 8);

		/// The graphical context must be created for the 1-bit pixmap, not for the window
        xcb_gcontext_t graphical_context = xcb_generate_id (connection);
        const uint32_t foreground = 0;
		xcb_create_gc(connection, graphical_context, pixmap_id, XCB_GC_FOREGROUND, &foreground);
		const xcb_rectangle_t rectangle = { 0, 0, 8, 8 };
		xcb_poly_fill_rectangle(connection, pixmap_id, graphical_context, 1, &rectangle);

        xcb_cursor_t cursor = xcb_generate_id (connection);
        xcb_create_cursor (connection,
						   cursor,
						   pixmap_id,
						   pixmap_id,
						   0, 0, 0, 0, 0, 0, 0, 0);

        const uint32_t value_list = cursor;
        xcb_change_window_attributes (connection, window, XCB_CW_CURSOR, &value_list);

        xcb_free_cursor (connection, cursor);
		xcb_free_gc (connection, graphical_context);
		xcb_free_pixmap (connection, pixmap_id);
	}

	xcb_connection_t* WindowXCBVulkan::GetConnection() { return connection; }

	xcb_window_t WindowXCBVulkan::GetWindow() { return window; }

	void WindowXCBVulkan::Disconnect() { Close(); }

	void WindowXCBVulkan::SwapBuffers() {};

	void WindowXCBVulkan::ClearDisplay() {};

	void WindowXCBVulkan::print_modifiers (uint32_t mask)
	{
		const char **mod, *mods[] = {
			"Shift", "Lock", "Ctrl", "Alt",
			"Mod2", "Mod3", "Mod4", "Mod5",
			"Button1", "Button2", "Button3", "Button4", "Button5"
		};
		printf ("Modifier mask: ");
		for (mod = mods ; mask; mask >>= 1, mod++)
			if (mask & 1)
				std::cout << *mod << std::endl;;
		putchar ('\n');
	}

	void WindowXCBVulkan::GrabPointer() {
		if ( isPointerGrabbed || connection == nullptr )
			return;
		xcb_grab_pointer_cookie_t cookie = xcb_grab_pointer(connection, 1, window,
															XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
															XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, window, XCB_NONE, XCB_CURRENT_TIME);
		xcb_grab_pointer_reply_t* grab_pointer_reply = xcb_grab_pointer_reply(connection, cookie, NULL);
		isPointerGrabbed = grab_pointer_reply != nullptr && grab_pointer_reply->status == XCB_GRAB_STATUS_SUCCESS;
		free(grab_pointer_reply);
	}

	void WindowXCBVulkan::UngrabPointer() {
		if ( !isPointerGrabbed )
			return;
		xcb_ungrab_pointer(connection, XCB_CURRENT_TIME);
		xcb_flush(connection);
		isPointerGrabbed = false;
	}

	void WindowXCBVulkan::SendEvent(CEvent& _Event, EEvents _eEvent) {
		_Event.SetEvent(_eEvent);
		Input_Stack_.ControlInput(_Event);
	}

	bool WindowXCBVulkan::HandleEvent([[maybe_unused]] CEvent& _Event) {
		if ( connection == nullptr )
			return false;

		while ( true ) {
			xcb_generic_event_t* generic_event = pending_event;
			pending_event = nullptr;
			if ( generic_event == nullptr )
				generic_event = xcb_poll_for_event(connection);
			if ( generic_event == nullptr )
				break;

			switch (generic_event->response_type & ~0x80) {
			case XCB_CONFIGURE_NOTIFY: {
				xcb_configure_notify_event_t* configure_event = (xcb_configure_notify_event_t*)generic_event;
				if ( configure_event->width > 0 && configure_event->height > 0 ) {
					width  = configure_event->width;
					height = configure_event->height;
				}
				break;
			}
			case XCB_MAP_NOTIFY:
				GrabPointer();
				break;
			case XCB_FOCUS_IN:
				isFocused = true;
				hasLastPointerPosition = false;
				GrabPointer();
				break;
			case XCB_FOCUS_OUT:
				/// Release events of keys held now go to another window: release everything, let the pointer go
				isFocused = false;
				hasLastPointerPosition = false;
				UngrabPointer();
				releaseHeldInput();
				break;
			case XCB_CLIENT_MESSAGE: {
				xcb_client_message_event_t* client_message = (xcb_client_message_event_t*)generic_event;
				if ( client_message->data.data32[0] == wm_delete_window )
					SendEvent(_Event, EEvents::eGAME_LOOP_KILL);
				break;
			}
			case XCB_BUTTON_PRESS: {
				xcb_button_press_event_t* button_event = (xcb_button_press_event_t *)generic_event;
				if ( button_event->detail == 1 ) {
					GrabPointer();
					SendEvent(_Event, EEvents::eMOUSE_LEFT_BUTTON);
				} else if ( button_event->detail == 3 ) {
					SendEvent(_Event, EEvents::eMOUSE_RIGHT_BUTTON);
				}
				break;
			}
			case XCB_BUTTON_RELEASE: {
				xcb_button_release_event_t* button_event = (xcb_button_release_event_t *)generic_event;
				if ( button_event->detail == 1 ) {
					SendEvent(_Event, EEvents::eMOUSE_LEFT_BUTTON_RELEASE);
					_Event.isLeftMouseButtonReleased = true;
				} else if ( button_event->detail == 3 ) {
					SendEvent(_Event, EEvents::eMOUSE_RIGHT_BUTTON_RELEASE);
				}
				break;
			}
			case XCB_MOTION_NOTIFY: {
				xcb_motion_notify_event_t* motion_event = (xcb_motion_notify_event_t *)generic_event;
				const int x = motion_event->event_x;
				const int y = motion_event->event_y;
                _Event.mousePointerPosition.position_X = x;
                _Event.mousePointerPosition.position_Y = y;
				if ( hasLastPointerPosition ) {
					motionX += x - lastPointerX;
					motionY += y - lastPointerY;
				}
				lastPointerX = x;
				lastPointerY = y;
				hasLastPointerPosition = true;
				break;
			}
			case XCB_KEY_PRESS: {
				xcb_key_press_event_t* key_event = (xcb_key_press_event_t *)generic_event;
				switch(key_event->detail)
				{
				case XKEYCODE_ESCAPE: SendEvent(_Event, EEvents::eGAME_LOOP_KILL);          break;
				case XKEYCODE_I:      SendEvent(_Event, EEvents::eINVENTORY);               break;
				case XKEYCODE_O:      SendEvent(_Event, EEvents::eDEBUG_COLLISIONS_ACTIVE); break;
				case XKEYCODE_A:      SendEvent(_Event, EEvents::eMOVE_LEFT);               break;
				case XKEYCODE_D:      SendEvent(_Event, EEvents::eMOVE_RIGHT);              break;
				case XKEYCODE_S:      SendEvent(_Event, EEvents::eMOVE_BACKWARD);           break;
				case XKEYCODE_W:      SendEvent(_Event, EEvents::eMOVE_FORWARD);            break;
				case XKEYCODE_SPACE:  SendEvent(_Event, EEvents::eJUMP);                    break;
				default: break;
				}
				break;
			}
			case XCB_KEY_RELEASE: {
				xcb_key_release_event_t* key_release_event = (xcb_key_release_event_t *)generic_event;

				/// Auto repeat sends release + press with the same time stamp: drop both
				xcb_generic_event_t* next_event = xcb_poll_for_event(connection);
				if ( next_event != nullptr && (next_event->response_type & ~0x80) == XCB_KEY_PRESS ) {
					xcb_key_press_event_t* key_press_event = (xcb_key_press_event_t*)next_event;
					if ( key_press_event->time == key_release_event->time && key_press_event->detail == key_release_event->detail ) {
						free(next_event);
						break;
					}
				}
				pending_event = next_event;                          ///< Not a repeat: handle it on the next iteration

				/// The toggles (I, O) have no release events: the engine consumes them itself
				switch(key_release_event->detail)
				{
                case XKEYCODE_A:     SendEvent(_Event, EEvents::eKEYRELEASE_A);    break;
                case XKEYCODE_D:     SendEvent(_Event, EEvents::eKEYRELEASE_D);    break;
                case XKEYCODE_S:     SendEvent(_Event, EEvents::eKEYRELEASE_S);    break;
                case XKEYCODE_W:     SendEvent(_Event, EEvents::eKEYRELEASE_W);    break;
                case XKEYCODE_SPACE: SendEvent(_Event, EEvents::eKEYRELEASE_JUMP); break;
				default: break;
				}
				break;
			}
			default:
				break;
			}

			free(generic_event);
		}

		if ( xcb_connection_has_error(connection) )                       ///< X server is gone
			SendEvent(_Event, EEvents::eGAME_LOOP_KILL);

		return false;
	};

	void WindowXCBVulkan::Close() {
		if ( connection == nullptr )
			return;
		free(pending_event);
		pending_event = nullptr;
		UngrabPointer();
		xcb_disconnect(connection);
		connection = nullptr;
	};

	/// Per-frame mouse delta (pixels, +x right, +y down); the pointer is re-centered only while the window has focus
	void WindowXCBVulkan::CursorLock([[maybe_unused]] int _x_position, [[maybe_unused]] int _y_position, int* _x_offset, int* _y_offset) {
		*_x_offset = motionX;
		*_y_offset = motionY;
		motionX = 0;
		motionY = 0;

		if ( !isFocused || connection == nullptr )
			return;

		const int centerX = (int)width / 2;
		const int centerY = (int)height / 2;
		if ( hasLastPointerPosition && lastPointerX == centerX && lastPointerY == centerY )
			return;

		xcb_warp_pointer(connection, XCB_NONE, window, 0, 0, 0, 0, (int16_t)centerX, (int16_t)centerY);
		xcb_flush(connection);
		/// Motion after the warp is measured from the center; the warp itself is not counted as movement
		lastPointerX = centerX;
		lastPointerY = centerY;
		hasLastPointerPosition = true;
	};
} // namespace GLVM::core
