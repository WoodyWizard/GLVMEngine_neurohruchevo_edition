// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef VIEW_COMPONENT_HPP
#define VIEW_COMPONENT_HPP

#include "VertexMath.hpp"

namespace GLVM::ecs::components
{
	/// Orbit camera: rotation angle (radians) per one pixel of mouse movement.
	inline constexpr float kOrbitCameraSensitivity = 0.05f * 3.14159265f / 180.0f;
	/// Orbit camera: pitch limit (radians). Keeps camera away from the poles where look-at basis degenerates.
	inline constexpr float kOrbitCameraMaxPitch    = 85.0f * 3.14159265f / 180.0f;

    class beholder
    {
    public:
		vec3 Position{0.0f, 0.0f, 0.0f};                ///< Camera position relative to the player
        vec3 forward{0.0f, 0.0, 0.0f};

		/*
		  Orbit camera parameters:
		  Position = orbitRadius * ( cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw) ).
		  Start values reproduce the previous start view of the camera.
		*/
		float yaw                = -0.9542f;
		float pitch              = 0.5035f;
		float orbitRadius        = 0.0f;                ///< 0 means: take length of the initial Position
		bool  isOrbitInitialized = false;
    };
}


#endif
