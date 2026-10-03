// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "WinApi/WindowWinVulkan.hpp"
#include "Event.hpp"
#include "Globals.hpp"
#include <cstdlib>
#include <iostream>
#include <iterator>

namespace GLVM::core
{
	namespace
	{
		/// PC set 1 scan codes: layout independent (same physical keys as on Linux)
		constexpr UINT SCANCODE_ESCAPE = 0x01;
		constexpr UINT SCANCODE_W      = 0x11;
		constexpr UINT SCANCODE_I      = 0x17;
		constexpr UINT SCANCODE_O      = 0x18;
		constexpr UINT SCANCODE_A      = 0x1E;
		constexpr UINT SCANCODE_S      = 0x1F;
		constexpr UINT SCANCODE_D      = 0x20;
		constexpr UINT SCANCODE_SPACE  = 0x39;

		const char* const kWindowClassName = "GLVM window class";

		UINT scanCodeOf(LPARAM _pLParam) {
			return (UINT)((_pLParam >> 16) & 0xFF);
		}
	}

    WindowWinVulkan::WindowWinVulkan()
    {
        const char* _title = "glvm";
        int _width = width, _height = height;
		HINSTANCE instance = GetModuleHandleA(NULL);

        // Register the window class for the main window.
        window_Class_.style = 0;
        window_Class_.lpfnWndProc = MainWndProc;
        window_Class_.cbClsExtra = 0;
        window_Class_.cbWndExtra = 0;
        window_Class_.hInstance = instance;
        window_Class_.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        window_Class_.hCursor = NULL;                                        ///< No cursor image: the cursor is hidden over the window
        window_Class_.hbrBackground = NULL;
        window_Class_.lpszMenuName = NULL;
        window_Class_.lpszClassName = kWindowClassName;

        RegisterClassA(&window_Class_);

        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

        RECT rect;
        SetRect(&rect, 0, 0, _width, _height);
        AdjustWindowRect(&rect, style, FALSE);

        // Create the main window. The window pointer is passed to WM_NCCREATE and stored in GWLP_USERDATA.
        pModern_Window_ = CreateWindowA(kWindowClassName,
                              _title,
                              style, CW_USEDEFAULT, CW_USEDEFAULT,
                              rect.right - rect.left, rect.bottom - rect.top, (HWND)NULL,
                              (HMENU)NULL, instance, (LPVOID)this);
		if ( pModern_Window_ == NULL ) {
			fprintf(stderr, "Error: CreateWindowA failed (error %lu)\n", (unsigned long)GetLastError());
			exit(EXIT_FAILURE);
		}

        // Show the window and paint its contents.
        ShowWindow(pModern_Window_, SW_SHOWDEFAULT);
        UpdateWindow(pModern_Window_);
		isFocused_ = GetForegroundWindow() == pModern_Window_;
    }

	void WindowWinVulkan::configureWindow() {
		if ( pModern_Window_ == NULL )
			return;
		SetWindowPos(
			pModern_Window_,
			NULL,          // Ignore Z-order placement since we are using SWP_NOZORDER
			0, 0,          // Ignore X and Y positions since we are using SWP_NOMOVE
			width,
			height,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
			);
	}

    void WindowWinVulkan::SwapBuffers() {}

    void WindowWinVulkan::ClearDisplay() {}

	void WindowWinVulkan::SendEvent(EEvents _eEvent) {
		CEvent& event = currentEvent_ ? *currentEvent_ : g_eEvent;
		event.SetEvent(_eEvent);
		Input_Stack_.ControlInput(event);
	}

    bool WindowWinVulkan::HandleEvent(CEvent& _Event)
    {
        ///< Create message struct object.
        MSG msg;

		currentEvent_ = &_Event;
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
			if ( msg.message == WM_QUIT ) {
				SendEvent(EEvents::eGAME_LOOP_KILL);
				continue;
			}
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
		if ( pModern_Window_ == NULL )                                    ///< Window is already destroyed
			SendEvent(EEvents::eGAME_LOOP_KILL);
		return false;
    }

    void WindowWinVulkan::Close()
    {
		if ( pModern_Window_ != NULL ) {
			HWND window = pModern_Window_;
			pModern_Window_ = NULL;
			DestroyWindow(window);
		}
    }

    HWND WindowWinVulkan::GetModernWindowHWND() { return pModern_Window_; }

	/// Per-frame mouse delta (pixels, +x right, +y down). The cursor is centered and hidden only while the window is active.
    void WindowWinVulkan::CursorLock([[maybe_unused]] int _x_position, [[maybe_unused]] int _y_position, int* _x_offset, int* _y_offset)
    {
		*_x_offset = motionX_;
		*_y_offset = motionY_;
		motionX_ = 0;
		motionY_ = 0;

		if ( pModern_Window_ == NULL || !isFocused_ || GetForegroundWindow() != pModern_Window_ )
			return;

		const int centerX = width / 2;
		const int centerY = height / 2;
		SetCursor(NULL);
		if ( hasLastPointerPosition_ && lastPointerX_ == centerX && lastPointerY_ == centerY )
			return;

        POINT point_position{centerX, centerY};
        ClientToScreen(pModern_Window_, &point_position);
        SetCursorPos(point_position.x, point_position.y);
		/// Motion after the warp is measured from the center; the warp itself is not counted as movement
		lastPointerX_ = centerX;
		lastPointerY_ = centerY;
		hasLastPointerPosition_ = true;
    }

	///< Callback method for events handling.
    LRESULT CALLBACK WindowWinVulkan::MainWndProc(HWND _pHwnd, UINT _pMsg, WPARAM _pWParam, LPARAM _pLParam)
    {
		if ( _pMsg == WM_NCCREATE ) {
			const CREATESTRUCTA* createStruct = (const CREATESTRUCTA*)_pLParam;
			SetWindowLongPtrA(_pHwnd, GWLP_USERDATA, (LONG_PTR)createStruct->lpCreateParams);
		}

		WindowWinVulkan* window = (WindowWinVulkan*)GetWindowLongPtrA(_pHwnd, GWLP_USERDATA);
		if ( window == nullptr )
			return DefWindowProcA(_pHwnd, _pMsg, _pWParam, _pLParam);

		return window->HandleMessage(_pHwnd, _pMsg, _pWParam, _pLParam);
	}

	LRESULT WindowWinVulkan::HandleMessage(HWND _pHwnd, UINT _pMsg, WPARAM _pWParam, LPARAM _pLParam)
	{
        switch (_pMsg)
        {
        case WM_CREATE:
            return 0;

        case WM_SIZE:
            return 0;

		case WM_SETCURSOR:
			if ( LOWORD(_pLParam) == HTCLIENT ) {                          ///< Hide the cursor over the client area
				SetCursor(NULL);
				return TRUE;
			}
			break;

		case WM_SETFOCUS:
			isFocused_ = true;
			hasLastPointerPosition_ = false;
			return 0;

		case WM_KILLFOCUS:
			/// Release events of keys held now go to another window: release everything
			isFocused_ = false;
			hasLastPointerPosition_ = false;
			releaseHeldInput();
			return 0;

        case WM_LBUTTONDOWN:
			SendEvent(EEvents::eMOUSE_LEFT_BUTTON);
            return 0;

        case WM_LBUTTONUP:
			SendEvent(EEvents::eMOUSE_LEFT_BUTTON_RELEASE);
			(currentEvent_ ? *currentEvent_ : g_eEvent).isLeftMouseButtonReleased = true;
			return 0;

        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(_pLParam);
            const int y = GET_Y_LPARAM(_pLParam);
			CEvent& event = currentEvent_ ? *currentEvent_ : g_eEvent;
            event.mousePointerPosition.position_X = x;
            event.mousePointerPosition.position_Y = y;
			if ( hasLastPointerPosition_ ) {
				motionX_ += x - lastPointerX_;
				motionY_ += y - lastPointerY_;
			}
			lastPointerX_ = x;
			lastPointerY_ = y;
			hasLastPointerPosition_ = true;
            return 0;
		}

        case WM_KEYDOWN:
			if ( _pLParam & (1 << 30) )                                     ///< Auto repeat: the key was already down
				return 0;
            switch (scanCodeOf(_pLParam))
            {
            case SCANCODE_ESCAPE: SendEvent(EEvents::eGAME_LOOP_KILL);          break;
            case SCANCODE_W:      SendEvent(EEvents::eMOVE_FORWARD);            break;
			case SCANCODE_S:      SendEvent(EEvents::eMOVE_BACKWARD);           break;
			case SCANCODE_A:      SendEvent(EEvents::eMOVE_LEFT);               break;
			case SCANCODE_D:      SendEvent(EEvents::eMOVE_RIGHT);              break;
            case SCANCODE_SPACE:  SendEvent(EEvents::eJUMP);                    break;
            case SCANCODE_I:      SendEvent(EEvents::eINVENTORY);               break;
            case SCANCODE_O:      SendEvent(EEvents::eDEBUG_COLLISIONS_ACTIVE); break;
            default:
                break;
            }
            return 0;

        case WM_KEYUP:
			/// The toggles (I, O) have no release events: the engine consumes them itself
            switch (scanCodeOf(_pLParam))
            {
            case SCANCODE_W:     SendEvent(EEvents::eKEYRELEASE_W);    break;
			case SCANCODE_S:     SendEvent(EEvents::eKEYRELEASE_S);    break;
			case SCANCODE_A:     SendEvent(EEvents::eKEYRELEASE_A);    break;
			case SCANCODE_D:     SendEvent(EEvents::eKEYRELEASE_D);    break;
            case SCANCODE_SPACE: SendEvent(EEvents::eKEYRELEASE_JUMP); break;
            default:
                break;
            }
            return 0;

		case WM_CLOSE:
			/// Close button / Alt+F4: stop the game loop, the renderer destroys the window during cleanup
			SendEvent(EEvents::eGAME_LOOP_KILL);
			return 0;

        case WM_DESTROY:
			pModern_Window_ = NULL;
            PostQuitMessage(0);
            return 0;

        default:
			break;
        }
        return DefWindowProcA(_pHwnd, _pMsg, _pWParam, _pLParam);
    }
}
