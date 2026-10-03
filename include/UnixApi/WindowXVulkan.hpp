// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef WINDOW_X_VULKAN
#define WINDOW_X_VULKAN

#include <X11/Xlib.h>
#include <cstdint>
#include "IWindow.hpp"
#include "EventsStack.hpp"
#include "Globals.hpp"

namespace GLVM::core
{
    class WindowXVulkan : public IWindow
    {
        Window Root_Window_;
        XSetWindowAttributes Set_Window_Attributes_;
		Atom wmDeleteWindow_ = None;
		bool isFocused_ = false;
		bool isPointerGrabbed_ = false;
		bool hasLastPointerPosition_ = false;
		int lastPointerX_ = 0;
		int lastPointerY_ = 0;
		int motionX_ = 0;                                       ///< Pointer motion accumulated since the last CursorLock
		int motionY_ = 0;

		void GrabPointer();
		void UngrabPointer();
		void SendEvent(CEvent& _Event, EEvents _eEvent);

    public:
        Display* pDisp_ = nullptr;
        Window Win_ = 0;
		uint32_t           width = 1920;
		uint32_t           height = 1080;

        WindowXVulkan();
        ~WindowXVulkan();

        Window GetWindow();
        Display* GetDisplay();
        void CursorLock(int _x_position, int _y_position, int* _x_offset, int* _y_offset) override;
        void SwapBuffers() override;
        void ClearDisplay() override;
        bool HandleEvent(CEvent& _Event) override;
        void Close() override;
    };
}

#endif
