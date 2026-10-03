// Input globals shared by every window backend (Wayland, X11, XCB, Win32); this file is compiled on all platforms.

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "EventsStack.hpp"
#include "Globals.hpp"

GLVM::core::CStack Input_Stack_{};

void releaseHeldInput() {
	using GLVM::core::EEvents;

	static const EEvents releaseEvents[] = {
		EEvents::eKEYRELEASE_W,
		EEvents::eKEYRELEASE_S,
		EEvents::eKEYRELEASE_A,
		EEvents::eKEYRELEASE_D,
		EEvents::eKEYRELEASE_JUMP,
		EEvents::eMOUSE_LEFT_BUTTON_RELEASE,
	};

	for ( const EEvents releaseEvent : releaseEvents ) {
		g_eEvent.SetEvent(releaseEvent);
		Input_Stack_.ControlInput(g_eEvent);
	}
	g_eEvent.isLeftMouseButtonReleased = true;
	g_eEvent.SetEvent(EEvents::eDEFAULT);
}
