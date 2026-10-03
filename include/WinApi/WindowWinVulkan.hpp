// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef WINDOW_WIN_VULKAN
#define WINDOW_WIN_VULKAN

#include "IWindow.hpp"
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <windowsx.h>
#include <cstdint>

#include "EventsStack.hpp"

namespace GLVM::core
{
    class WindowWinVulkan : public IWindow
    {
        WNDCLASSA window_Class_ = {};                         ///< ANSI class explicitly: builds with and without UNICODE
        HWND pModern_Window_ = NULL;
		CEvent* currentEvent_ = nullptr;                      ///< Event object of the current HandleEvent call
		bool isFocused_ = false;
		bool hasLastPointerPosition_ = false;
		int lastPointerX_ = 0;                                ///< Client coordinates
		int lastPointerY_ = 0;
		int motionX_ = 0;                                     ///< Pointer motion accumulated since the last CursorLock
		int motionY_ = 0;

		void SendEvent(EEvents _eEvent);
		LRESULT HandleMessage(HWND _pHwnd, UINT _pMsg, WPARAM _pWParam, LPARAM _pLParam);

    public:
		uint16_t width = 1920;
		uint16_t height = 1080;

        WindowWinVulkan();

		void configureWindow();
        void SwapBuffers() override;
        void ClearDisplay() override;
        bool HandleEvent(CEvent& _Event) override;
        HWND GetModernWindowHWND();
        void Close() override;
        virtual void CursorLock(int _x_position, int _y_position, int* _x_offset, int* _y_offset) override;
        ///< Callback method for events handling.
        static LRESULT CALLBACK MainWndProc(HWND _pHwnd, UINT _pMsg, WPARAM _pWParam, LPARAM _pLParam);
    }; // namespace GLVM::core
}

#endif
