// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef WINDOW_XCB_VULKAN
#define WINDOW_XCB_VULKAN

#include "IWindow.hpp"
#include <cstdint>
#include <unistd.h>
#include <cstdio>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <cassert>
#include <iostream>
#include "EventsStack.hpp"
#include "Globals.hpp"

namespace GLVM::core
{
    typedef uint32_t xcb_window_t;

	class WindowXCBVulkan : public IWindow
	{
		xcb_connection_t*  connection = nullptr;
		xcb_screen_t*      screen = nullptr;
		xcb_window_t       window = 0;
		xcb_generic_event_t* pending_event = nullptr;            ///< Event read ahead by the auto repeat check
		xcb_atom_t         wm_delete_window = XCB_ATOM_NONE;
		bool               isFocused = false;
		bool               isPointerGrabbed = false;
		bool               hasLastPointerPosition = false;
		int                lastPointerX = 0;
		int                lastPointerY = 0;
		int                motionX = 0;                          ///< Pointer motion accumulated since the last CursorLock
		int                motionY = 0;

		static void print_modifiers (uint32_t mask);
		xcb_atom_t InternAtom(const char* name);
		void GrabPointer();
		void UngrabPointer();
		void SendEvent(CEvent& _Event, EEvents _eEvent);
	public:
		uint32_t           width = 1920;
		uint32_t           height = 1080;
		bool               isWindowResizeRead = false;

		WindowXCBVulkan ();

		void configureWindow();
		void HideCursor();
		xcb_connection_t* GetConnection();
		xcb_window_t GetWindow();
		void Disconnect();

		void SwapBuffers() override;
        void ClearDisplay() override;
        bool HandleEvent(CEvent& _Event) override;
        void Close() override;
        void CursorLock(int _x_position, int _y_position, int* _x_offset, int* _y_offset) override;
	};
} // namespace GLVM::core

#endif
