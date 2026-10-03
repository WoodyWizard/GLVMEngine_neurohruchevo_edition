// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "UnixApi/WindowXVulkan.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace GLVM::core
{
	namespace
	{
		/// X11 key codes of evdev/libinput servers (Xorg, XWayland) are evdev codes + 8: layout independent, same keys as Wayland
		constexpr unsigned int XKEYCODE_ESCAPE = 1 + 8;
		constexpr unsigned int XKEYCODE_W      = 17 + 8;
		constexpr unsigned int XKEYCODE_I      = 23 + 8;
		constexpr unsigned int XKEYCODE_O      = 24 + 8;
		constexpr unsigned int XKEYCODE_A      = 30 + 8;
		constexpr unsigned int XKEYCODE_S      = 31 + 8;
		constexpr unsigned int XKEYCODE_D      = 32 + 8;
		constexpr unsigned int XKEYCODE_SPACE  = 57 + 8;
	}

    WindowXVulkan::WindowXVulkan()
    {
        pDisp_ = XOpenDisplay(NULL);
		if ( pDisp_ == nullptr ) {
			fprintf(stderr, "Error: can't open X display (DISPLAY=%s)\n", getenv("DISPLAY") ? getenv("DISPLAY") : "unset");
			exit(EXIT_FAILURE);
		}

        Root_Window_ = DefaultRootWindow(pDisp_);
        Set_Window_Attributes_.event_mask = KeyPressMask | KeyReleaseMask |
            PointerMotionMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | FocusChangeMask;

        Win_ = XCreateWindow(pDisp_, Root_Window_, 0, 0, width, height, 0, CopyFromParent, InputOutput,
                            CopyFromParent, CWEventMask, &Set_Window_Attributes_);
		XStoreName(pDisp_, Win_, "glvm");

		/// Ask the window manager for a WM_DELETE_WINDOW message instead of killing the connection on close
		wmDeleteWindow_ = XInternAtom(pDisp_, "WM_DELETE_WINDOW", False);
		XSetWMProtocols(pDisp_, Win_, &wmDeleteWindow_, 1);

        ///< Show_the_window
        XMapWindow(pDisp_, Win_);

        Cursor invisibleCursor;
        Pixmap bitmapNoData;
        XColor black;
        static char noData[] = { 0,0,0,0,0,0,0,0 };
        black.red = black.green = black.blue = 0;

        bitmapNoData = XCreateBitmapFromData(pDisp_, Win_, noData, 8, 8);
        invisibleCursor = XCreatePixmapCursor(pDisp_, bitmapNoData, bitmapNoData,
                                              &black, &black, 0, 0);
        XDefineCursor(pDisp_,Win_, invisibleCursor);

        XFreeCursor(pDisp_, invisibleCursor);
        XFreePixmap(pDisp_, bitmapNoData);
		XFlush(pDisp_);
    }

    WindowXVulkan::~WindowXVulkan() = default;

    Window WindowXVulkan::GetWindow() { return Win_; }
    Display* WindowXVulkan::GetDisplay() { return pDisp_; }

	void WindowXVulkan::GrabPointer() {
		if ( isPointerGrabbed_ || pDisp_ == nullptr )
			return;
		///< Link mouse cursor to specified window.
		const int result = XGrabPointer(pDisp_, Win_, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
										GrabModeAsync, GrabModeAsync, Win_, None, CurrentTime);
		isPointerGrabbed_ = result == GrabSuccess;
	}

	void WindowXVulkan::UngrabPointer() {
		if ( !isPointerGrabbed_ )
			return;
		XUngrabPointer(pDisp_, CurrentTime);
		isPointerGrabbed_ = false;
	}

	void WindowXVulkan::SendEvent(CEvent& _Event, EEvents _eEvent) {
		_Event.SetEvent(_eEvent);
		Input_Stack_.ControlInput(_Event);
	}

	/// Per-frame mouse delta (pixels, +x right, +y down); the pointer is re-centered only while the window has focus
    void WindowXVulkan::CursorLock([[maybe_unused]] int _x_position, [[maybe_unused]] int _y_position, int* _x_offset, int* _y_offset)
    {
		*_x_offset = motionX_;
		*_y_offset = motionY_;
		motionX_ = 0;
		motionY_ = 0;

		if ( !isFocused_ )
			return;

		const int centerX = (int)width / 2;
		const int centerY = (int)height / 2;
		if ( hasLastPointerPosition_ && lastPointerX_ == centerX && lastPointerY_ == centerY )
			return;

        XWarpPointer(pDisp_, None, Win_, 0, 0, 0, 0, centerX, centerY);
        XFlush(pDisp_);
		/// Motion after the warp is measured from the center; the warp itself is not counted as movement
		lastPointerX_ = centerX;
		lastPointerY_ = centerY;
		hasLastPointerPosition_ = true;
    }

    void WindowXVulkan::SwapBuffers()
    {
    }

    void WindowXVulkan::ClearDisplay()
    {
    }

    bool WindowXVulkan::HandleEvent(CEvent& _Event)
    {
        XEvent uXEvent;

        while(pDisp_ != nullptr && XPending(pDisp_))
        {
            XNextEvent(pDisp_, &uXEvent);

			switch(uXEvent.type)
			{
            case MotionNotify: {
				const int x = uXEvent.xmotion.x;
				const int y = uXEvent.xmotion.y;
                _Event.mousePointerPosition.position_X = x;
                _Event.mousePointerPosition.position_Y = y;
				if ( hasLastPointerPosition_ ) {
					motionX_ += x - lastPointerX_;
					motionY_ += y - lastPointerY_;
				}
				lastPointerX_ = x;
				lastPointerY_ = y;
				hasLastPointerPosition_ = true;
                break;
			}
            case MapNotify:
				GrabPointer();
                break;
			case ConfigureNotify:
				if ( uXEvent.xconfigure.width > 0 && uXEvent.xconfigure.height > 0 ) {
					width  = (uint32_t)uXEvent.xconfigure.width;
					height = (uint32_t)uXEvent.xconfigure.height;
				}
				break;
			case FocusIn:
				isFocused_ = true;
				hasLastPointerPosition_ = false;
				GrabPointer();
				break;
			case FocusOut:
				/// Release events of keys held now go to another window: release everything, let the pointer go
				isFocused_ = false;
				hasLastPointerPosition_ = false;
				UngrabPointer();
				releaseHeldInput();
				break;
			case ClientMessage:
				if ( (Atom)uXEvent.xclient.data.l[0] == wmDeleteWindow_ )
					SendEvent(_Event, EEvents::eGAME_LOOP_KILL);
				break;
            case ButtonPress:
                if ( uXEvent.xbutton.button == Button1 ) {
					GrabPointer();
                    SendEvent(_Event, EEvents::eMOUSE_LEFT_BUTTON);
                }
                break;
            case ButtonRelease:
                if ( uXEvent.xbutton.button == Button1 ) {
                    SendEvent(_Event, EEvents::eMOUSE_LEFT_BUTTON_RELEASE);
					_Event.isLeftMouseButtonReleased = true;
                }
                break;
			case KeyPress:
				switch(uXEvent.xkey.keycode)
				{
				case XKEYCODE_I:      SendEvent(_Event, EEvents::eINVENTORY);               break;
				case XKEYCODE_O:      SendEvent(_Event, EEvents::eDEBUG_COLLISIONS_ACTIVE); break;
				case XKEYCODE_ESCAPE: SendEvent(_Event, EEvents::eGAME_LOOP_KILL);          break;
				case XKEYCODE_A:      SendEvent(_Event, EEvents::eMOVE_LEFT);               break;
				case XKEYCODE_D:      SendEvent(_Event, EEvents::eMOVE_RIGHT);              break;
				case XKEYCODE_S:      SendEvent(_Event, EEvents::eMOVE_BACKWARD);           break;
				case XKEYCODE_W:      SendEvent(_Event, EEvents::eMOVE_FORWARD);            break;
				case XKEYCODE_SPACE:  SendEvent(_Event, EEvents::eJUMP);                    break;
				default: break;
				}
				break;

			case KeyRelease:
				if(XEventsQueued(pDisp_, QueuedAfterReading))
				{
					XEvent uXNext_Event;
					XPeekEvent(pDisp_, &uXNext_Event);

					if (uXNext_Event.type == KeyPress && uXNext_Event.xkey.time == uXEvent.xkey.time &&
						uXNext_Event.xkey.keycode == uXEvent.xkey.keycode)
					{
						///< Key wasn’t actually released (auto repeat): drop both events
                        XNextEvent(pDisp_, &uXNext_Event);
						continue;
					}
				}
				/// The toggles (I, O) have no release events: the engine consumes them itself
                switch(uXEvent.xkey.keycode)
                {
                case XKEYCODE_A:     SendEvent(_Event, EEvents::eKEYRELEASE_A);    break;
                case XKEYCODE_D:     SendEvent(_Event, EEvents::eKEYRELEASE_D);    break;
                case XKEYCODE_S:     SendEvent(_Event, EEvents::eKEYRELEASE_S);    break;
                case XKEYCODE_W:     SendEvent(_Event, EEvents::eKEYRELEASE_W);    break;
                case XKEYCODE_SPACE: SendEvent(_Event, EEvents::eKEYRELEASE_JUMP); break;
				default: break;
                }
				break;
			default:
				break;
			}
        }
		return false;
    }

    void WindowXVulkan::Close()
    {
		if ( pDisp_ == nullptr )
			return;
		UngrabPointer();
        XDestroyWindow(pDisp_, Win_);
        XCloseDisplay(pDisp_);
		pDisp_ = nullptr;
    }
}
