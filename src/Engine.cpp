// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "Engine.hpp"
#include "ArchetypeECS/ArchECS_Types.hpp"
#include "ArchetypeECS/ArchECS_Utils.hpp"
#include "ArchetypeECS/ArchetypeInterface.hpp"
#include "Archetypes/CrosshairArchetype.hpp"
#include "Archetypes/DirectionalLightArchetype.hpp"
#include "Archetypes/EnemyArchetype.hpp"
#include "Archetypes/InventoryArchetype.hpp"
#include "Archetypes/ItemArchetype.hpp"
#include "Archetypes/LevelChunkArchetype.hpp"
#include "Archetypes/PlayerArchetype.hpp"
#include "Archetypes/ProjectileArchetype.hpp"
#include "Archetypes/StaticMeshArchetype.hpp"
#include "Common/CommonFunctions.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/MeshGenerationComponent.hpp"
#include "Components/ProjectileBundle.hpp"
#include "Components/AnimationComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/DirectionalLightComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/PointLightComponent.hpp"
#include "Components/RotationComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Components/ViewComponent.hpp"
#include "Constants.hpp"
#include "Event.hpp"
#include "ISoundEngine.hpp"
#include "GraphicAPI/Vulkan.hpp"
#include "ISoundEngine.hpp"
#include "ProceduralLevelGeneratingSystem.hpp"
#include "ShaderStructs.hpp"
#include "SoundEngineFactory.hpp"
#include "SystemManager.hpp"
#include "Systems/CollisionSystem.hpp"
#include "Systems/DamageSystem.hpp"
#include "Systems/EnemySystem.hpp"
#include "Systems/InventorySystem.hpp"
#include "Systems/ItemSystem.hpp"
#include "Systems/MovementSystem.hpp"
#include "Systems/PhysicsSystem.hpp"
#include "Systems/ProjectileSystem.hpp"
#include "TagComponents/LevelChunkTagComponent.hpp"
#include "Texture.hpp"
#include "VertexMath.hpp"
#include "VkStructs.hpp"
#include <cstdint>
#include <limits>
#include <mutex>
#include <sys/types.h>
#include <thread>
//#include <wayland-client-core.h>
#include <fstream>
#include <filesystem>


/*******************************************************************
 * Legends never die...
 * You are about to face most terrifying data structures of all time.
 *    "Abandon hope all ye who enter here..." (c) Dante Alighieri.
 *******************************************************************
 *****************  👑  !!!  DESTRUCTOR_3000  !!!  👑  *************/

/*******************************************************************
*                                                                  *
*                             \_/                                  *
*                            (* *)                                 *
*                           __)#(__                                *
*                          ( )...( )(_)                            *
*                          || |_| ||//                             *
*                       >==() | | ()/                              *
*                           _(___)_                                *
*                          [-]   [-]                               *
*                                                                  *
********************************************************************/

#define DESTRUCTOR_3000													\
    std::cout << "You have been destructurized. [=]___[=]" << std::endl; \
    exit(1)

GLVM::core::CEvent g_eEvent{};
GLVM::core::vector<GLVM::core::MeshAxisMaxAbsoluteValues> allMeshMaxAbsoluteValues;      /// contain all maximum absolute axis values
// struct wl_surface*    wl_surface;
// struct wl_compositor* compositor;
// struct xdg_toplevel*  xdg_topLevel;
// struct xdg_wm_base*   xdg_shell;
// struct wl_buffer*     buffer;
// struct wl_shm*        shared_memory;
// struct wl_seat*       seat;
// struct wl_keyboard*   keyboard;
// void* pixels;
// uint16_t width = 480;
// uint16_t height = 320;
// uint8_t  constant_byte = 0;
// uint8_t  close_xdg_toplevel;
// struct wl_display*  display;
// struct wl_registry* registry;
// struct wl_callback* frame_callback;
// struct xdg_surface* xdg_surface;

namespace GLVM::core
{
    Engine* Engine::pInstance_ = nullptr;
    std::mutex Engine::Mutex_;

	/// Sound thread. SoundStream() sleeps until there is a sound to play or StopStream() is called.
    void PlaybackSound(Sound::ISoundEngine* _sound_Engine, std::atomic<bool>& runningSound) {
        while( runningSound ) {
			_sound_Engine->SoundStream();
		}
    }

    Engine::Engine() {
		chrono                          = Time::CTimerCreator().Create();
		soundEngine                     = Sound::CSoundEngineFactory().CreateSoundEngine();

		spatialGridSystem               = new ecs::SpatialGridSystem();
		collisionSystem                 = new ecs::CCollisionSystem(Input_Stack_);
		movementSystem                  = new ecs::CMovementSystem(Input_Stack_);
		physicsSystem                   = new ecs::CPhysicsSystem(Input_Stack_);
		projectileSystem                = new ecs::CProjectileSystem(Input_Stack_);
		damageSystem                    = new ecs::DamageSystem();
		enemySytem                      = new ecs::EnemySystem();
		itemSystem                      = new ecs::ItemSystem();
		procuduralLevelGeneratingSystem = new ProceduralLevelGeneratingSystem();
		inventorySystem                 = new ecs::InventorySystem();
        
		deltaFrameTime             = 0.0;
		g_eEvent.SetEvent(eDEFAULT);

		ecs::CSystemManager* pSystem_Manager = ecs::CSystemManager::GetInstance();

		///< Call of ActivateSystem function must be in this order.
		pSystem_Manager->ActivateSystem(procuduralLevelGeneratingSystem);
		pSystem_Manager->ActivateSystem(movementSystem);
		pSystem_Manager->ActivateSystem(enemySytem);
		pSystem_Manager->ActivateSystem(projectileSystem);
		pSystem_Manager->ActivateSystem(spatialGridSystem);
		pSystem_Manager->ActivateSystem(collisionSystem);
		pSystem_Manager->ActivateSystem(damageSystem);
		pSystem_Manager->ActivateSystem(physicsSystem);
		pSystem_Manager->ActivateSystem(inventorySystem);
		pSystem_Manager->ActivateSystem(itemSystem);

		/// Device is opened before the sound thread starts, running flag is set before the thread starts too
		soundEngine->OpenDevice( "default" );
		runningSound = true;
		sound_thread = std::thread(PlaybackSound, soundEngine, std::ref(runningSound));
    }

    Engine::~Engine() {
		/// GameKill is idempotent: it stops the sound thread (a joinable std::thread must not be destroyed) and frees systems
		GameKill();
		if ( pInstance_ == this )
			pInstance_ = nullptr;
	}
            
    Engine* Engine::GetInstance() {
		std::lock_guard<std::mutex> lock(Mutex_);
		if(pInstance_ == nullptr) {
			pInstance_ = new Engine();
		}
		return pInstance_;
    }

    void Engine::GameLoop() {
		RenderVulkan();
    }

	void Engine::EventQueueFlush() {
	}
	
	void Engine::RenderVulkan() {
		namespace arch = GLVM::ecs::arch;
		std::cout << "INNER CALL ARCHETYPES NUMBER: " << arch::world.archetypes.GetSize() << std::endl;
		ecs::CSystemManager* pSystem_Manager = ecs::CSystemManager::GetInstance();
		bool bGame_Loop_Active = true;

		projectileSystem->textureHandlers = textureHandlers;
		projectileSystem->meshHandlers    = meshHandlers;

		enemySytem->textureHandlers       = textureHandlers;
		enemySytem->meshHandlers          = meshHandlers;

		procuduralLevelGeneratingSystem->meshHandlers    = meshHandlers;
		procuduralLevelGeneratingSystem->textureHandlers = textureHandlers;

		inventorySystem->isItemDraged     = &dragedItemEntity;
		itemSystem->dragedItemEntity      = &dragedItemEntity;

		/// Joint matrices of all not animated actors. Built once instead of every frame.
		identityJointMatrices.Resize(MAX_JOINTS_NUMBER);
		for ( unsigned int i = 0; i < MAX_JOINTS_NUMBER; ++i )
			identityJointMatrices[i] = mat4(1.0f);

		vulkanRenderer = new CVulkanRenderer();
		vulkanRenderer->initializeTextureData_ = textureVector;
		vulkanRenderer->pathsArray_            = pathsArray_;
		vulkanRenderer->pathsGLTF_             = pathsGLTF_;
		GLVM::core::MeshManager*   meshManager = GLVM::core::MeshManager::GetInstance();
		vulkanRenderer->SetMeshData(meshManager->pathsArray_, meshManager->pathsGLTF_);
		namespace cm = GLVM::ecs::components;

		directionalLightArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( directionalLightRequiredMask, cachedDirectionalLigthArchetypes, directionalLightArchetypesNumber );
		vulkanRenderer->directionalLightNumber = directionalLightArchetypesNumber;

		spotLightArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( spotLightRequiredMask, cachedSpotLigthArchetypes, spotLightArchetypesNumber );
		vulkanRenderer->spotLightNumber = spotLightArchetypesNumber;

		pointLightArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( pointLightRequiredMask, cachedPointLigthArchetypes, pointLightArchetypesNumber );
		vulkanRenderer->pointLightNumber = pointLightArchetypesNumber;

		animationActorsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( animatedActorsRequiredMask, cachedAnimationActorsArchetypes, animationActorsArchetypesNumber );

		staticActorsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( staticActorsRequiredMask, cachedStaticActorsArchetypes, staticActorsArchetypesNumber );

		// vulkanRenderer->projectionMatrix = SetProjectionMatrix( 90.0f, 1920.0f, 1080.0f, 0.1f, 100.0f );
		// SetViewMatrix();
		// const mat4 vp = vulkanRenderer->viewMatrix * vulkanRenderer->projectionMatrix;
		// vulkanRenderer->extractFrustum( vp );
		
		loadWavefrontObj();
		initializeGLTF();
		initializeFontData();
		initializeMathObjectsData();
		vulkanRenderer->run();
//		vulkanRenderer->Window->Input_Stack_    = &Input_Stack_;		

#ifdef __linux__
		// XEvent uXEvent;
		// while (XPending(vulkanRenderer->Window.GetDisplay())) {
		// 	XNextEvent(vulkanRenderer->Window.GetDisplay(), &uXEvent);
		// }

		// xcb_generic_event_t* event;
		// while (( event = xcb_poll_for_event ( vulkanRenderer->Window.GetConnection() ))) {
		// }
#endif

#ifdef _WIN32
/*		MSG msg;

		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage( &msg );
				DispatchMessage( &msg );
				}*/

		// while(GetMessageA(&msg, vulkanRenderer->Window.GetModernWindowHWND(), WM_KEYFIRST, WM_KEYLAST)) {
		// 		// TranslateMessage( &msg );
		// 		// DispatchMessage( &msg );
		// }
#endif

		/*
		  Frame time is limited: after a stall (loading, hidden window, debugger) a huge step would make
		  objects pass through walls. First frame does not include loading time because the timer is reset here.
		*/
		constexpr float maxFrameTime = 0.1f;
		chrono->Reset();

		while(bGame_Loop_Active) {
			deltaFrameTime = std::min( static_cast<float>(chrono->GetElapsed()), maxFrameTime );
			chrono->Reset();

			vulkanRenderer->Window->ClearDisplay();
			g_eEvent.SetEvent(EEvents::eDEFAULT);
			vulkanRenderer->Window->HandleEvent(g_eEvent);
//			std::cout << "left mouse released flag" << g_eEvent.isItemDraged << std::endl;			
			// 	Input_Stack_.ControlInput(g_eEvent);
			if((Input_Stack_.SearchElement(EEvents::eGAME_LOOP_KILL)) == EEvents::eGAME_LOOP_KILL) {
				bGame_Loop_Active = false;
			}

			if((Input_Stack_.SearchElement(EEvents::eMOUSE_LEFT_BUTTON)) == EEvents::eMOUSE_LEFT_BUTTON) {
				isLeftMouseButtonPressed = true;
			} else {
				isLeftMouseButtonPressed = false;
			}

			if((Input_Stack_.SearchElement(EEvents::eINVENTORY)) == EEvents::eINVENTORY) {
				vulkanRenderer->isInventoryOpened = !vulkanRenderer->isInventoryOpened;
				Input_Stack_.Remove(EEvents::eINVENTORY);
				if ( vulkanRenderer->isInventoryOpened )
					pSystem_Manager->DeactivateSystem(ecs::DeactivatedSystems::DEACTIVATED_MOVEMENT_SYSTEM);
				else
					pSystem_Manager->ReturnSystemToActivatedState(ecs::DeactivatedSystems::DEACTIVATED_MOVEMENT_SYSTEM);
//				bGame_Loop_Active = false;
			}

			if((Input_Stack_.SearchElement(EEvents::eDEBUG_COLLISIONS_ACTIVE)) == EEvents::eDEBUG_COLLISIONS_ACTIVE) {
				vulkanRenderer->isDebugCollisitionsActive = !vulkanRenderer->isDebugCollisitionsActive;
				Input_Stack_.Remove(EEvents::eDEBUG_COLLISIONS_ACTIVE);
			}

			// }
			g_eEvent.SetLastEvent(Input_Stack_);

			vulkanRenderer->Window->CursorLock(g_eEvent.mousePointerPosition.position_X,
								  g_eEvent.mousePointerPosition.position_Y,
								  &g_eEvent.mousePointerPosition.offset_X,
											  &g_eEvent.mousePointerPosition.offset_Y);

			computeHudScreeenCoordinates();
			// std::cout << "lmb released " << g_eEvent.isLeftMouseButtonReleased << std::endl;
			// std::cout << "lmb pressed " << isLeftMouseButtonPressed << std::endl;
			// std::cout << "item draged " << itemSystem->isItemDraged << std::endl;
			FPScounter();
			damageSystem->deltaTime                   = deltaFrameTime;
			movementSystem->deltaFrameTime            = deltaFrameTime;
			collisionSystem->fDelta_Time_             = deltaFrameTime;
			collisionSystem->isInventoryOpened        = vulkanRenderer->isInventoryOpened;
//			collisionSystem->isItemDraged             = &itemSystem->isItemDraged;
			collisionSystem->isLeftMouseButtonPressed = isLeftMouseButtonPressed;
			collisionSystem->isLeftMouseButtonReleased = &g_eEvent.isLeftMouseButtonReleased;
			enemySytem->deltaFrameTime                = deltaFrameTime;
			enemySytem->soundEngine                   = soundEngine;
			projectileSystem->deltaFrameTime          = deltaFrameTime;
			projectileSystem->soundEngine             = soundEngine;
			projectileSystem->isInventoryOpened       = vulkanRenderer->isInventoryOpened;
			physicsSystem->fDelta_Time_               = deltaFrameTime;
			inventorySystem->isInventoryOpened         = vulkanRenderer->isInventoryOpened;
			inventorySystem->aspectRate                = getViewportAspectRate();
			inventorySystem->isLeftMouseButtonReleased = &g_eEvent.isLeftMouseButtonReleased;
			inventorySystem->isLeftMouseButtonPressed  = isLeftMouseButtonPressed;
			inventorySystem->mouseOffsetX              = hud_screen_x;
			inventorySystem->mouseOffsetY              = hud_screen_y;
			itemSystem->inputStack                    = &Input_Stack_;
			itemSystem->isInventoryOpened             = vulkanRenderer->isInventoryOpened;
			itemSystem->isLeftMouseButtonReleased     = &g_eEvent.isLeftMouseButtonReleased;
			itemSystem->isLeftMouseButtonPressed      = isLeftMouseButtonPressed;
			itemSystem->mouseOffsetX                  = hud_screen_x;
			itemSystem->mouseOffsetY                  = hud_screen_y;
			pSystem_Manager->Update();
			vulkanRenderer->levelGeneratedVertices    = procuduralLevelGeneratingSystem->levelGeneratedVertices;
			vulkanRenderer->levelGeneratedIndices     = procuduralLevelGeneratingSystem->levelGeneratedIndices;
			procuduralLevelGeneratingSystem->levelGeneratedVertices.clear();
			procuduralLevelGeneratingSystem->levelGeneratedIndices.clear();
			vulkanRenderer->dragedItemEntity          = dragedItemEntity;
			vulkanRenderer->hud_screen_x              = hud_screen_x;
			vulkanRenderer->hud_screen_y              = hud_screen_y;
			vulkanRenderer->initializeGameLevelVertices();
//			Input_Stack_.PrintStack();
			/// Wireframe buffers are created per mesh, so they are (re)initialized only when new meshes appear
			if( !vulkanRenderer->isCollisionsWireframeBuffersInitialized ||
				collisionWireframeMeshesNumber != allMeshMaxAbsoluteValues.GetSize() ) {
				vulkanRenderer->initializeCollisionWireframesBuffers();
				collisionWireframeMeshesNumber = allMeshMaxAbsoluteValues.GetSize();
			}
			
			if( !vulkanRenderer->isInventoryOpened ) {
				SetViewMatrix();
			}

			/// Projection follows aspect rate of the swapchain (renderer updates it when the window is resized)
			vulkanRenderer->projectionMatrix = SetProjectionMatrix( 90.0f, getViewportAspectRate(), 1.0f, 0.1f, 100.0f );
			const mat4 vp = vulkanRenderer->viewMatrix * vulkanRenderer->projectionMatrix;
			vulkanRenderer->mainCameraFrustum = extractFrustum( vp );
			initializeAABB();
			setFrameData();
			vulkanRenderer->draw();
			vulkanRenderer->Window->SwapBuffers();
		}

//		vulkanRenderer->Window->Close();
		delete vulkanRenderer;
	}

	namespace {
		/*
		  Angle of a direction on XZ plane. Direction (-sin(angle), 0, -cos(angle)) has angle "angle". It is the same
		  parametrization as the orbit camera yaw: forward of the camera that looks at the player has angle equal to camera yaw.
		*/
		float directionAngleXZ( const vec3& direction ) {
			return std::atan2( -direction[0], -direction[2] );
		}
	}

	/// Player model rotation (transform pitch) = playerModelYawOffset + angle of model forward direction on XZ plane.
	/// Offset is taken from the initial state of the player, so the start orientation of the model is kept.
	void Engine::initializePlayerModelYawOffset( const ecs::components::transform& playerTransform ) {
		if ( isPlayerModelYawOffsetInitialized )
			return;

		playerModelYawOffset = playerTransform.pitch - directionAngleXZ( playerTransform.previousFrameForward );
		isPlayerModelYawOffsetInitialized = true;
	}

	/// Aspect rate of the swapchain. Renderer updates it when the swapchain is recreated (window resize).
	float Engine::getViewportAspectRate() const {
		constexpr float defaultAspectRate = 1920.0f / 1080.0f;
		if ( vulkanRenderer == nullptr )
			return defaultAspectRate;

		const float aspectRate = vulkanRenderer->aspectRate;
		return ( std::isfinite( aspectRate ) && aspectRate > 0.0f ) ? aspectRate : defaultAspectRate;
	}

    void Engine::SetViewMatrix()
    {
		namespace cm = GLVM::ecs::components;
		namespace arch = GLVM::ecs::arch;

		playerArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( playerRequiredMask, cachedPlayerArchetypes, playerArchetypesNumber );

		for( uint32_t n = 0; n < playerArchetypesNumber; ++n ) {
			arch::Archetype* arch = cachedPlayerArchetypes[n];
			cm::beholder*  views      = (ecs::components::beholder*)arch->
				components[arch::ComponentsIndices::VIEW_COMPONENT];
			cm::transform* transfroms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];

			for( uint32_t x = 0; x < arch->entityCount; ++x ) {
				cm::beholder* cameraComponent   = &views[x];
				cm::transform* _Player          = &transfroms[x];

				Matrix<float, 4> viewMatrix_(1.0f);
				const float kSensitivity = 0.1f;

				/// Mouse offsets are the mouse movement (pixels) of the current frame
				const float mouseOffsetX = static_cast<float>(g_eEvent.mousePointerPosition.offset_X);
				const float mouseOffsetY = static_cast<float>(g_eEvent.mousePointerPosition.offset_Y);

				fYaw   = mouseOffsetX * kSensitivity;
				fPitch = mouseOffsetY * kSensitivity;
				g_eEvent.mousePointerPosition.pitch = fPitch;
				g_eEvent.mousePointerPosition.yaw   = fYaw;

				vulkanRenderer->current_X = mouseOffsetX;
				vulkanRenderer->current_Y = mouseOffsetY;

				/*
				  Orbit camera around the player. Yaw rotates the camera around the world vertical axis, pitch is
				  limited by kOrbitCameraMaxPitch, so the camera never reaches the poles where look-at basis with
				  up vector (0, -1, 0) degenerates. Position is computed from the angles, so its length does not drift.
				*/
				const bool isFirstCameraUpdate = !cameraComponent->isOrbitInitialized;
				if ( isFirstCameraUpdate ) {
					if ( cameraComponent->orbitRadius <= 0.0f )
						cameraComponent->orbitRadius = VecLength( cameraComponent->Position );
					if ( !(cameraComponent->orbitRadius > 0.0f) )
						cameraComponent->orbitRadius = 1.0f;
					cameraComponent->isOrbitInitialized = true;
				}

				const bool isMouseMoved = mouseOffsetX != 0.0f || mouseOffsetY != 0.0f;
				constexpr float fullTurn = 2.0f * 3.14159265f;
				cameraComponent->yaw   = std::remainder( cameraComponent->yaw + mouseOffsetX * cm::kOrbitCameraSensitivity, fullTurn );
				cameraComponent->pitch = std::clamp( cameraComponent->pitch + mouseOffsetY * cm::kOrbitCameraSensitivity,
													 -cm::kOrbitCameraMaxPitch, cm::kOrbitCameraMaxPitch );

				const float cosPitch = std::cos( cameraComponent->pitch );
				cameraComponent->Position = vec3( cosPitch * std::sin( cameraComponent->yaw ),
												  std::sin( cameraComponent->pitch ),
												  cosPitch * std::cos( cameraComponent->yaw ) ) * cameraComponent->orbitRadius;
				cameraComponent->forward  = Normalize( -cameraComponent->Position );

				/// While the player stands still, his model turns together with the camera (by camera yaw)
				if ( (isMouseMoved || isFirstCameraUpdate) && _Player->frameMovement[0] == 0.0f && _Player->frameMovement[2] == 0.0f ) {
					initializePlayerModelYawOffset( *_Player );
					_Player->forward              = cameraComponent->forward;
					_Player->pitch                = playerModelYawOffset + cameraComponent->yaw;
					_Player->previousFrameForward = Normalize( vec3( cameraComponent->forward[0], 0.0f, cameraComponent->forward[2] ) );
				}
				mat4 view = LookAtMain( cameraComponent->Position + _Player->position,
										cameraComponent->Position + _Player->position + cameraComponent->forward,
//										_Player->position,
										vec3( 0.0f, -1.0f, 0.0) );
				for ( unsigned int i = 0; i < 4; ++i )
					for ( unsigned int j = 0; j < 4; ++j )
						viewMatrix_[i][j] = view[i][j];
		
				vulkanRenderer->viewMatrix = viewMatrix_;

				vulkanRenderer->prev_Y = (float)g_eEvent.mousePointerPosition.offset_Y;
				vulkanRenderer->prev_X = (float)g_eEvent.mousePointerPosition.offset_X;
			}
		}
    }
	
	mat4 Engine::SetProjectionMatrix( const float fov, const float viewPortWidth, const float viewPortHeight, const float nearPlane, const float farPlane )
	{
//		mat4 tProjection_Matrix = Perspective(Radians(90.0f), (float)1920 / (float)1080, 0.1f, 100.0f);
		mat4 projectionMatrix = Perspective(Radians(fov), viewPortWidth / viewPortHeight, nearPlane, farPlane);
		return projectionMatrix;
//		vulkanRenderer->projectionMatrix = tProjection_Matrix;
//		vulkanRenderer->projectionMatrix[1][1] *= 1.0f;
	}
	
	/*
	  Animation time (frameAccumulator) grows only while the animation is played (this function is called).
	  currentAnimationFrame is the number of key frames whose time is passed, so several key frames can be skipped
	  on a long frame and animation speed does not depend on FPS. On the end of the clip time wraps with its remainder.
	*/
	void Engine::updateAnimationFrames(ecs::components::animation* animationComponent, unsigned int meshID) {
		const bool hasAnimation = meshID < vulkanRenderer->jointMatricesPerMesh.GetSize() &&
			vulkanRenderer->jointMatricesPerMesh[meshID].GetSize() > 0 &&
			meshID < vulkanRenderer->frames.GetSize() &&
			vulkanRenderer->frames[meshID].GetSize() > 0;

		if ( hasAnimation ) {
			const core::vector<float>& keyFrameTimes = vulkanRenderer->frames[meshID];
			const unsigned int keyFramesNumber = keyFrameTimes.GetSize();
			const float clipDuration = keyFrameTimes[keyFramesNumber - 1];

			animationComponent->frameAccumulator += deltaFrameTime;
			if ( !(clipDuration > 0.0f) ) {
				animationComponent->currentAnimationFrame = 0;
				animationComponent->frameAccumulator      = 0.0f;
			} else {
				while ( animationComponent->currentAnimationFrame < keyFramesNumber &&
						animationComponent->frameAccumulator >= keyFrameTimes[animationComponent->currentAnimationFrame] ) {
					++animationComponent->currentAnimationFrame;
				}

				if ( animationComponent->currentAnimationFrame >= keyFramesNumber ) {
					animationComponent->currentAnimationFrame = 0;
					animationComponent->frameAccumulator      = std::fmod( animationComponent->frameAccumulator, clipDuration );
				}
			}
		}

		core::vector<mat4>& jointMatrices = animationComponent->jointMatrices;
		if ( jointMatrices.GetSize() != MAX_JOINTS_NUMBER )
			jointMatrices.Resize(MAX_JOINTS_NUMBER);

		unsigned int jointsNumber = 0;
		if ( hasAnimation ) {
			jointsNumber = vulkanRenderer->jointMatricesPerMesh[meshID].GetSize();
			if ( jointsNumber > MAX_JOINTS_NUMBER ) {
				std::cerr << "Mesh " << meshID << " has more joints than MAX_JOINTS_NUMBER, extra joints are ignored" << std::endl;
				jointsNumber = MAX_JOINTS_NUMBER;
			}
		}

		for ( unsigned int i = 0; i < jointsNumber; ++i ) {
			const core::vector<mat4>& jointFrames = vulkanRenderer->jointMatricesPerMesh[meshID][i];
			if ( jointFrames.GetSize() == 0 ) {
				jointMatrices[i] = mat4(1.0f);
				continue;
			}

			/// Channels of a joint can have less key frames than the clip, then the last one is used
			const unsigned int frameIndex = std::min( animationComponent->currentAnimationFrame, jointFrames.GetSize() - 1 );
			jointMatrices[i] = jointFrames[frameIndex];
		}

		for ( unsigned int j = jointsNumber; j < MAX_JOINTS_NUMBER; ++j )
			jointMatrices[j] = mat4(1.0f);
	}

	mat4 Engine::updateDirectionalLightSpaceMatrixShadowMapUBO( ecs::components::directionalLight* directionalLightComponent ) {
		float nearPlaneFlatShadowMap = 5.5f;
		float farPlaneFlatShadowMap = 100.0f;
		mat4 directionalProjectionMatrixLight = ortho(-20.0f, 20.0f, -20.0f, 20.0f,
													  nearPlaneFlatShadowMap, farPlaneFlatShadowMap);

		vec3 positionVectorLight = directionalLightComponent->position;
		vec3 directionVectorLight = directionalLightComponent->direction;

		/// LookAt takes a target point, the light looks along its direction.
		mat4 viewMatrixLight = LookAtMain(positionVectorLight,
										  positionVectorLight + directionVectorLight,
										  { 0.0f, -1.0f, 0.0f });

//		directionalProjectionMatrixLight[1][1] *= -1;
		return viewMatrixLight * directionalProjectionMatrixLight;
	}

	mat4 Engine::updateSpotLightSpaceMatrixShadowMapUBO( ecs::components::spotLight* spotLightComponent ) {
		float nearPlaneFlatShadowMap = 5.5f;
		float farPlaneFlatShadowMap = 100.0f;
		/// Spot light shadow maps are square (SHADOW_MAP_SIZE x SHADOW_MAP_SIZE).
		mat4 spotProjectionMatrixLight = Perspective(Radians(90.0f), 1.0f,
														 nearPlaneFlatShadowMap, farPlaneFlatShadowMap);

		vec3 positionVectorLight  = spotLightComponent->position;
		vec3 directionVectorLight = spotLightComponent->direction;
		/// LookAt takes a target point, the light looks along its direction.
		mat4 viewMatrixLight = LookAtMain(positionVectorLight,
										  positionVectorLight + directionVectorLight,
										  { 0.0f, -1.0f, 0.0f });

//		spotProjectionMatrixLight[1][1] *= -1;
		return viewMatrixLight * spotProjectionMatrixLight;
	}

	mat4 Engine::updatePointLightSpaceMatrixShadowMapUBO( ecs::components::pointLight* pointLightComponent, uint32_t layer ) {
		vec3 positionVectorLight  = pointLightComponent->position;
		vec3 directionalVectorLight = vec3(0.0f, 0.0f, 0.0f);
		vec3 upVector = { 0.0, 0.0, 0.0 };

		switch(layer) {
		case 0:
			/// Positive X
			directionalVectorLight = positionVectorLight + vec3( 1.0f,  0.0f, 0.0f);
			upVector = vec3(0.0f, -1.0f,  0.0f);
			break;
		case 1:
			/// Negative X
			directionalVectorLight = positionVectorLight + vec3( -1.0f,  0.0f,  0.0f);
			upVector = vec3(0.0f, -1.0f,  0.0f);
			break;
		case 2:
			/// Positive Y
			directionalVectorLight = positionVectorLight + vec3( 0.0f,  1.0f,  0.0f);
			upVector = vec3(0.0f, 0.0f,  1.0f);
			break;
		case 3:
			/// Negative Y
			directionalVectorLight = positionVectorLight + vec3( 0.0f,  -1.0f,  0.0f);
			upVector = vec3(0.0f, 0.0f,  -1.0f);
			break;
		case 4:
			/// Positive Z
			directionalVectorLight = positionVectorLight + vec3( 0.0f,  0.0f,  1.0f);
			upVector = vec3(0.0f, -1.0f,  0.0f);
			break;
			/// Negative Z
		case 5:
			directionalVectorLight = positionVectorLight + vec3( 0.0f,  0.0f,  -1.0f);
			upVector = vec3(0.0f, -1.0f,  0.0f);
			break;
		default:
			break;
		}
		
		mat4 projectionMatrixCubeShadowMap = Perspective(Radians(90.0f), (float)SHADOW_MAP_SIZE / (float)SHADOW_MAP_SIZE, 5.5f, 100.0f);

		mat4 viewMatrixLight = LookAtMain(positionVectorLight,
										  directionalVectorLight,
										  upVector);

		return viewMatrixLight * projectionMatrixCubeShadowMap;
	}

	[[nodiscard]] SlotData Engine::updateDataUBO_UI(const unsigned int currentInventoryRow, const unsigned int currentInventoryColumn,
													ecs::components::inventory* inventoryComponent,
													ecs::components::transform* slotTransfromComponent,
													ecs::components::mesh*      meshComponent) {
		SlotData hudUBO{};
		mat4 model(1.0);
		const float fullSlotScale     = meshComponent->gltf ? inventoryComponent->slotScale * 2.0f : inventoryComponent->slotScale;
		const float x = slotTransfromComponent->position[0] + currentInventoryColumn * fullSlotScale;
		const float y_scaleMultilayer = vulkanRenderer->aspectRate * fullSlotScale;
		const float y = slotTransfromComponent->position[1] + currentInventoryRow * y_scaleMultilayer;
		const float inventorySlotScale = inventoryComponent->slotScale;
		model[0][0] = inventorySlotScale;
		model[1][1] = inventorySlotScale;
		model[2][2] = inventorySlotScale;
		model[3][0] = x;
		model[3][1] = y;
		model[3][2] = 0.1f;
		
		hudUBO.model = model;

		bool highLightedSlot = false;
		for ( unsigned int i = 0; i < inventoryComponent->highlightedSlots.GetSize(); ++i ) {
			if ( inventoryComponent->highlightedSlots[i] == currentInventoryRow * inventoryComponent->col + currentInventoryColumn ) {
				highLightedSlot = true;
				break;
			} else
				continue;
		}

		if ( inventoryComponent->highlightedSlots.GetSize() > 0 ) {
			if ( highLightedSlot ) {
				if ( inventoryComponent->isAvailableHighlightedSlots )
					hudUBO.color = { 0.0, 0.3, 0.0 };
				else
					hudUBO.color = { 0.3, 0.0, 0.0 };
			}
		} else {
			hudUBO.color = { 0.0, 0.0, 0.0 };
		}

		return hudUBO;
	}

	mat4 Engine::updateDataUBO_IconsUI(ecs::components::transform* itemTransfromComponent,
									   [[maybe_unused]] ecs::components::collider* itemColliderComponent,
									   ecs::components::item* itemComponent,
									   [[maybe_unused]] const unsigned int rowInventory,
									   const unsigned int columnInventory,
									   ecs::components::transform* inventoryTransformComponent,
									   ecs::components::mesh* itemMesh,
									   int itemEntity) {
		float x_result_offset = 0.0f;
		float y_result_offset = 0.0f;
		if ( itemComponent->occupiedSlots.GetSize() == 0 ) {
		} else {
			const unsigned int inventorySlotEntity_0 = itemComponent->occupiedSlots[0];
			const unsigned int inventorySlotEntity_3 = itemComponent->occupiedSlots.GetHead();
			/// Slot index is row * columnInventory + column
			const unsigned int rowIndexFirstSlot = inventorySlotEntity_0 / columnInventory;
			const unsigned int colIndexFirstSlot = inventorySlotEntity_0 % columnInventory;
			const unsigned int rowIndexSecondSlot = inventorySlotEntity_3 / columnInventory;
			const unsigned int colIndexSecondSlot = inventorySlotEntity_3 % columnInventory;

			const float itemScale             = itemTransfromComponent->scale;
			const float fullSlotScale         = itemMesh->gltf ? itemScale * 2.0f : itemScale;
			constexpr float centreMultiplayer = 0.5f;                                                                   ///< Eather division by 2.0f using multiply on 0.5f
			x_result_offset = inventoryTransformComponent->position[0] + (colIndexFirstSlot * fullSlotScale + colIndexSecondSlot * fullSlotScale) * centreMultiplayer;
			y_result_offset = inventoryTransformComponent->position[1] + (rowIndexFirstSlot * fullSlotScale + rowIndexSecondSlot * fullSlotScale) * centreMultiplayer * vulkanRenderer->aspectRate;
		}
		float itemScale = itemTransfromComponent->scale;

		if ( dragedItemEntity != itemEntity ) {
			itemTransfromComponent->position = vec3(x_result_offset, y_result_offset, 0.1f);
		} else {
//			std::cout << "item entity: " << itemEntity << std::endl;
			itemScale *= 1.1f;
//			itemColliderComponent->itemDrag = false;
			itemTransfromComponent->position[2] = 0.0f;
		}

//		std::cout << itemTransfromComponent->position << std::endl;
		
		mat4 model(1.0);
		model[0][0] = itemScale * itemComponent->itemSlotType.width;
		model[1][1] = itemScale * itemComponent->itemSlotType.height;
		model[2][2] = 0.0f;
		model[3][0] = itemTransfromComponent->position[0];
		model[3][1] = itemTransfromComponent->position[1];
		model[3][2] = itemTransfromComponent->position[2];

		return model;
	}

	mat4 Engine::updateDataHudScreenUBO( ecs::components::transform* cursorTransform ) {
		mat4 model;
		vec3 defaultPosition = vec3(0.0, 0.0, 0.0);

#ifndef VK_USE_PLATFORM_WAYLAND_KHR
//		hud_screen_x = -hud_screen_x;
#endif
		
		cursorTransform->position[0] = hud_screen_x;
		cursorTransform->position[1] = -hud_screen_y;
//		std::cout << "cursor scale: " << cursorTransform->fScale << std::endl;

//		std::cout << "x: " << cursorTransform->tPosition[0] << " y: " << cursorTransform->tPosition << std::endl;
		
		if ( !vulkanRenderer->isInventoryOpened ) {
			model[3][0] = defaultPosition[0];
			model[3][1] = defaultPosition[1];
			model[3][2] = defaultPosition[2];
			model[0][0] = cursorTransform->scale;
			model[1][1] = cursorTransform->scale;
			model[2][2] = cursorTransform->scale;
			model[3][3] = 1.0f;
		} else {
			defaultPosition[0] = hud_screen_x;
#ifdef VK_USE_PLATFORM_WIN32_KHR
//			defaultPosition[0] = -defaultPosition[0];
#endif
			defaultPosition[1] = -hud_screen_y;
			
			model[3][0] = defaultPosition[0];
			model[3][1] = defaultPosition[1];
			model[3][2] = defaultPosition[2];
			model[0][0] = cursorTransform->scale;
			model[1][1] = cursorTransform->scale;
			model[2][2] = cursorTransform->scale;
			model[3][3] = 1.0f;
		}

		return model;
	}
	
	void Engine::setFrameData() {
		namespace cm = GLVM::ecs::components;
		namespace arch = GLVM::ecs::arch;
		
		vulkanRenderer->directionalLights.clear();
		directionalLightArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( directionalLightRequiredMask, cachedDirectionalLigthArchetypes, directionalLightArchetypesNumber );
		
		uint32_t directionalLightCounter = 0;
		for( uint32_t x = 0; x < directionalLightArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedDirectionalLigthArchetypes[x];
			cm::directionalLight*  directionalLights = (ecs::components::directionalLight*)arch->
				components[arch::ComponentsIndices::DIRECTIONAL_LIGHT_COMPONENT];

			for( uint32_t x1 = 0; x1 < arch->entityCount; ++x1 ) {
				if( directionalLights ) {
					vulkanRenderer->directionalLights.Push({});
					cm::directionalLight* directionalLightComponent = &directionalLights[x1];
					vulkanRenderer->directionalLights[directionalLightCounter].DirectionalLightSpaceMatrix =
						updateDirectionalLightSpaceMatrixShadowMapUBO( directionalLightComponent );
					vulkanRenderer->directionalLights[directionalLightCounter].position = vec4(directionalLightComponent->position[0],
																							   directionalLightComponent->position[1],
																							   directionalLightComponent->position[2], 0.0);
					vulkanRenderer->directionalLights[directionalLightCounter].direction = vec4(directionalLightComponent->direction[0],
																								directionalLightComponent->direction[1],
																								directionalLightComponent->direction[2], 0.0);
					vulkanRenderer->directionalLights[directionalLightCounter].ambient = vec4(directionalLightComponent->ambient[0],
																							  directionalLightComponent->ambient[1],
																							  directionalLightComponent->ambient[2], 0.0);
					vulkanRenderer->directionalLights[directionalLightCounter].diffuse = vec4(directionalLightComponent->diffuse[0],
																							  directionalLightComponent->diffuse[1],
																							  directionalLightComponent->diffuse[2], 0.0);
					vulkanRenderer->directionalLights[directionalLightCounter].specular = vec4(directionalLightComponent->specular[0],
																							   directionalLightComponent->specular[1],
																							   directionalLightComponent->specular[2], 0.0);
					++directionalLightCounter;
				}
			}
		}

		vulkanRenderer->spotLights.clear();
		spotLightArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( spotLightRequiredMask, cachedSpotLigthArchetypes, spotLightArchetypesNumber );
		
		uint32_t spotLightCounter = 0;
		for( uint32_t x = 0; x < spotLightArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedSpotLigthArchetypes[x];
			cm::spotLight*  spotLights = (ecs::components::spotLight*)arch->
				components[arch::ComponentsIndices::SPOT_LIGHT_COMPONENT];
			
			for( uint32_t x1 = 0; x1 < arch->entityCount; ++x1 ) {
				if( spotLights ) {
					vulkanRenderer->spotLights.Push({});
					cm::spotLight* spotLightComponent = &spotLights[x1];
					vulkanRenderer->spotLights[spotLightCounter].SpotLigthSpaceMatrix =
						updateSpotLightSpaceMatrixShadowMapUBO( spotLightComponent );
					vulkanRenderer->spotLights[spotLightCounter].position    = spotLightComponent->position;
					vulkanRenderer->spotLights[spotLightCounter].direction   = spotLightComponent->direction;
					vulkanRenderer->spotLights[spotLightCounter].cutOff      = spotLightComponent->cutOff;
					vulkanRenderer->spotLights[spotLightCounter].outerCutOff = spotLightComponent->outerCutOff;
					vulkanRenderer->spotLights[spotLightCounter].ambient     = spotLightComponent->ambient;
					vulkanRenderer->spotLights[spotLightCounter].diffuse     = spotLightComponent->diffuse;
					vulkanRenderer->spotLights[spotLightCounter].specular    = spotLightComponent->specular;
					vulkanRenderer->spotLights[spotLightCounter].constant    = spotLightComponent->constant;
					vulkanRenderer->spotLights[spotLightCounter].linear      = spotLightComponent->linear;
					vulkanRenderer->spotLights[spotLightCounter].quadratic   = spotLightComponent->quadratic;
					++spotLightCounter;
				}
			}
		}
		
		vulkanRenderer->pointLights.clear();
		pointLightArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( pointLightRequiredMask, cachedPointLigthArchetypes, pointLightArchetypesNumber );
		
		uint32_t pointLightCounter = 0;
		for( uint32_t x = 0; x < pointLightArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedPointLigthArchetypes[x];
			cm::pointLight* pointLights = (ecs::components::pointLight*)arch->
				components[arch::ComponentsIndices::POINT_LIGHT_COMPONENT];
			
			for( uint32_t x1 = 0; x1 < arch->entityCount; ++x1 ) {
				if( pointLights ) {
					vulkanRenderer->pointLights.Push({});
					cm::pointLight* pointLightComponent = &pointLights[x1];
					uint32_t maxCubeMapLayers = 6;
					for ( uint32_t cubeMapLayerCounter = 0; cubeMapLayerCounter < maxCubeMapLayers; ++cubeMapLayerCounter ) {                      ///< 6 is a number of cube map layers.
						vulkanRenderer->pointLights[pointLightCounter].pointLightSpaceMatrix[cubeMapLayerCounter] =
							updatePointLightSpaceMatrixShadowMapUBO( pointLightComponent, cubeMapLayerCounter );
					}
					vulkanRenderer->pointLights[pointLightCounter].position  = pointLightComponent->position;
					vulkanRenderer->pointLights[pointLightCounter].ambient   = pointLightComponent->ambient;
					vulkanRenderer->pointLights[pointLightCounter].diffuse   = pointLightComponent->diffuse;
					vulkanRenderer->pointLights[pointLightCounter].specular  = pointLightComponent->specular;
					vulkanRenderer->pointLights[pointLightCounter].constant  = pointLightComponent->constant;
					vulkanRenderer->pointLights[pointLightCounter].linear    = pointLightComponent->linear;
					vulkanRenderer->pointLights[pointLightCounter].quadratic = pointLightComponent->quadratic;
					++pointLightCounter;
				}
			}
		}
		
		vulkanRenderer->healthBars.clear();
		healthBarsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( healthBarsRequiredMask, cachedHealthBarsArchetypes, healthBarsArchetypesNumber );
		
		uint32_t healthBarCounter = 0;
		for( uint32_t x = 0; x < healthBarsArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedHealthBarsArchetypes[x];
			cm::transform* healthBarTransforms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
		 	cm::mesh*      healthBarMeshes     = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			cm::health*    healthBars          = (ecs::components::health*)arch->
				components[arch::ComponentsIndices::HEALTH_COMPONENT];

			unsigned int uiVertexId = 0;
			if( arch->entityCount > 0 && ecs::arch::matchesRequiredMask( arch->mask, arch::playerComponentMask ) ) {
				uiVertexId = healthBarMeshes[0].handle.id;
			}

			uiVertexId = 0;               ///< TODO: Need to consider another solution

			for ( unsigned int i = 0; i < arch->entityCount; ++i ) {
				if( !healthBars[i].randarable ) {
					continue;
				}
				
				vulkanRenderer->healthBars.Push({});
//				unsigned int uiVertexId           = healthBarMeshes[i].handle.id;
				cm::transform* transformComponent = &healthBarTransforms[i];
				cm::health* healthComponent       = &healthBars[i];
				vulkanRenderer->healthBars[healthBarCounter].meshID        = uiVertexId;
				vulkanRenderer->healthBars[healthBarCounter].position      = transformComponent->position;
				vulkanRenderer->healthBars[healthBarCounter].maxHealth     = healthComponent->maxHealth;
				vulkanRenderer->healthBars[healthBarCounter].currentHealth = healthComponent->currentHealth;
				++healthBarCounter;
			}
		}
		
		vulkanRenderer->fonts.clear();
		fontsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( fontRequiredMask, cachedFontsArchetypes, fontsArchetypesNumber );
		
		uint32_t fontCounter = 0;
		for( uint32_t x = 0; x < fontsArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedFontsArchetypes[x];
			cm::transform* fontTransforms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
		 	cm::font*      fonts          = (ecs::components::font*)arch->
				components[arch::ComponentsIndices::FONT_COMPONENT];

			for ( unsigned int i = 0; i < arch->entityCount; ++i ) {
				vulkanRenderer->fonts.Push({});
				cm::font* fontComponent           = &fonts[i];
				cm::transform* transformComponent = &fontTransforms[i];
				vulkanRenderer->fonts[fontCounter].position    = transformComponent->position;
				vulkanRenderer->fonts[fontCounter].font_string = fontComponent->font_string;
				vulkanRenderer->fonts[fontCounter].lifeTime    = fontComponent->lifeTime;
				++fontCounter;
			}
		}
		
		if ( vulkanRenderer->isInventoryOpened ) {
			vulkanRenderer->inventories.clear();
			uint32_t inventoryCounter = 0;
			vulkanRenderer->items.clear();                  ///< Items of all inventories are collected, so the list is cleared once
			uint32_t itemCounter = 0;
			inventoryArchetypesNumber = 0;
			arch::world.searchCacheArchetypes( inventoryRequiredMask, cachedInventoryArchetypes, inventoryArchetypesNumber );
			
			for( uint32_t x = 0; x < inventoryArchetypesNumber; ++x ) {
				arch::Archetype* arch = cachedInventoryArchetypes[x];
				cm::transform* inventoryTransforms = (ecs::components::transform*)arch->
					components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
				cm::inventory* inventory          = (ecs::components::inventory*)arch->
					components[arch::ComponentsIndices::INVENTORY_COMPONENT];
				cm::material*  inventoryMaterials = (ecs::components::material*)arch->
					components[arch::ComponentsIndices::MATERIAL_COMPONENT];
				cm::mesh*      inventoryMeshes    = (ecs::components::mesh*)arch->
					components[arch::ComponentsIndices::MESH_COMPONENT];
				
				if( inventoryTransforms && inventoryMaterials && inventory && inventoryMeshes ) {
				
					for ( unsigned int i = 0; i < arch->entityCount; ++i ) {
						vulkanRenderer->inventories.Push({});
						cm::inventory* inventoryComponent = &inventory[i];
						unsigned int inventoryTextureID   = inventoryMaterials[i].diffuseTextureID_.id;
						unsigned int meshID           = inventoryComponent->slotMeshID.id;
						vulkanRenderer->inventories[inventoryCounter].inventoryTextureID = inventoryTextureID;
						vulkanRenderer->inventories[inventoryCounter].meshID             = meshID;
						vulkanRenderer->inventories[inventoryCounter].row                = inventoryComponent->row;
						vulkanRenderer->inventories[inventoryCounter].col                = inventoryComponent->col;
						vulkanRenderer->inventories[inventoryCounter].slotData.clear();
						for ( unsigned int j = 0; j < inventoryComponent->row; ++j ) {
							for ( unsigned int m = 0; m < inventoryComponent->col; ++m ) {
								cm::transform* slotTransformComponent     = &inventoryTransforms[i];
								vulkanRenderer->inventories[inventoryCounter].slotData.Push({});
								vulkanRenderer->inventories[inventoryCounter].slotData[j * inventoryComponent->col + m] =
									updateDataUBO_UI( j, m, inventoryComponent, slotTransformComponent, &inventoryMeshes[i] );
							}
						}
						++inventoryCounter;
					}

					for ( unsigned int i = 0; i < arch->entityCount; ++i ) {
						cm::inventory* inventoryComponent          = &inventory[i];
						cm::transform* inventoryTransformComponent = &inventoryTransforms[i];
		
						itemArchetypesNumber = 0;
						arch::world.searchCacheArchetypes( itemRequiredMask, cachedItemArchetypes, itemArchetypesNumber );
						
						for( uint32_t c = 0; c < itemArchetypesNumber; ++c ) {
							arch::Archetype* arch = cachedItemArchetypes[c];
							cm::transform* itemTransforms = (ecs::components::transform*)arch->
								components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
							cm::item*      items          = (ecs::components::item*)arch->
								components[arch::ComponentsIndices::ITEM_COMPONENT];
							cm::material*  itemMaterials  = (ecs::components::material*)arch->
								components[arch::ComponentsIndices::MATERIAL_COMPONENT];
							cm::mesh*      itemMeshes     = (ecs::components::mesh*)arch->
								components[arch::ComponentsIndices::MESH_COMPONENT];
							cm::collider*  itemColliders  = (ecs::components::collider*)arch->
								components[arch::ComponentsIndices::COLLIDER_COMPONENT];

							if( itemTransforms && itemMaterials && itemMeshes &&
								itemColliders && items) {
								for ( unsigned int a = 0; a < arch->entityCount; ++a ) {
									cm::item* itemComponent = &items[a];
									if( !itemComponent->isActor ) {
										vulkanRenderer->items.Push({});
										unsigned int meshID = itemMeshes[a].handle.id;
										unsigned int diffuseTexureID = itemMaterials[a].diffuseTextureID_.id;
										vulkanRenderer->items[itemCounter].meshID          = meshID;
										vulkanRenderer->items[itemCounter].diffuseTexureID = diffuseTexureID;
										cm::transform* itemTransformComponent    = &itemTransforms[a];
										cm::collider* itemColliderComponent      = &itemColliders[a];

										if ( itemTransformComponent == nullptr )
											std::cout << "NULL POINTER" << std::endl;

										uint32_t itemEntity = arch->entities[a];
										vulkanRenderer->items[itemCounter].model = updateDataUBO_IconsUI(itemTransformComponent,
																							   itemColliderComponent,
																							   itemComponent,
																							   inventoryComponent->row,
																							   inventoryComponent->col,
																							   inventoryTransformComponent,
																							   &itemMeshes[a],
																							   itemEntity);
										++itemCounter;
									}
								}
							}
						}
					}
				}
			}
		}

		vulkanRenderer->crosshairs.clear();
		crosshairActorsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( crosshairRequiredMask, cachedCrosshairActorsArchetypes, crosshairActorsArchetypesNumber );

		uint32_t crosshairCounter = 0;
		for( uint32_t x = 0; x < crosshairActorsArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedCrosshairActorsArchetypes[x];
			cm::transform* crosshairTransforms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			cm::mesh*      crosshairMeshes     = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];

			for ( unsigned int i = 0; i < arch->entityCount; ++i ) {
				vulkanRenderer->crosshairs.Push({});
				cm::transform* cursorTransform = &crosshairTransforms[i];
				unsigned int meshID            = crosshairMeshes[i].handle.id;
				vulkanRenderer->crosshairs[crosshairCounter].meshID = meshID;
				vulkanRenderer->crosshairs[crosshairCounter].model  = updateDataHudScreenUBO( cursorTransform );
				++crosshairCounter;
			}
		}


		
		vulkanRenderer->actors.clear();
		vulkanRenderer->collisionsWireframes.clear();
		uint32_t collisionsWireframesCounter = 0;
		levelChunkActorsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( levelChunkRequiredMask, cachedLevelChunkActorsArchetypes, levelChunkActorsArchetypesNumber );
		
		uint32_t levelChunkActorsCounter = 0;
		for( uint32_t x = 0; x < levelChunkActorsArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedLevelChunkActorsArchetypes[x];
			cm::transform* levelChunkTransforms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			cm::mesh*      levelChunkMeshes     = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			cm::material*  levelChunkMaterials  = (ecs::components::material*)arch->
				components[arch::ComponentsIndices::MATERIAL_COMPONENT];
			cm::rotation*  levelChunkRotations  = (ecs::components::rotation*)arch->
				components[arch::ComponentsIndices::ROTATION_COMPONENT];
			ecs::tagComponents::levelChunkTagComponent* levelChunks = (ecs::tagComponents::levelChunkTagComponent*)arch->
				components[arch::ComponentsIndices::LEVEL_CHUNK_TAG_COMPONENT];
			
			const core::vector<mat4>& jointMatrices = identityJointMatrices;     ///< Not animated actors
			
			for( uint32_t n = 0; n < arch->entityCount; ++n ) {
				cm::transform* transformComponent = &levelChunkTransforms[n];
				cm::material*  materialComponent  = &levelChunkMaterials[n];
				cm::rotation*  rotationComponent  = &levelChunkRotations[n];
				if( levelChunkTransforms && levelChunkMaterials && levelChunks &&
					levelChunkRotations && levelChunkMeshes ) {
					const unsigned int meshID = levelChunkMeshes[n].handle.id;
//					cm::transform playerTransform = *transformComponent;
//					playerTransform.position += vec3(0.0f, 2.0f, -3.0f);
//					cm::rotation  playerRotation  = *rotationComponent;
					const AABB worldAABB = computeWorldAABB( levelChunkMeshes[n].aabb, transformComponent->position );
					const bool isFrustumIntersectFlag = isFrustumIntersect( vulkanRenderer->mainCameraFrustum, worldAABB );

					if( isFrustumIntersectFlag ) {
						vulkanRenderer->actors.Push({});
						vulkanRenderer->collisionsWireframes.Push({});

						const mat4 model = computeModelMatrix(transformComponent, rotationComponent);
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].model    = model;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].position = transformComponent->position;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].scale    = transformComponent->scale;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshID   = meshID;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[meshID];
						++collisionsWireframesCounter;
					
						vulkanRenderer->actors[levelChunkActorsCounter].modelMatrix   = model;
						vulkanRenderer->actors[levelChunkActorsCounter].jointMatrices = jointMatrices;
						vulkanRenderer->actors[levelChunkActorsCounter].meshID        = meshID;
						vulkanRenderer->actors[levelChunkActorsCounter].diffuseTextureIndex  = materialComponent->diffuseTextureID_.id;
						vulkanRenderer->actors[levelChunkActorsCounter].specularTextureIndex = materialComponent->specularTextureID_.id;
						vulkanRenderer->actors[levelChunkActorsCounter].ambient   = materialComponent->ambient;
						vulkanRenderer->actors[levelChunkActorsCounter].shininess = materialComponent->shininess;
						++levelChunkActorsCounter;
					}
				}
			}
		}

		animationActorsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( animatedActorsRequiredMask, cachedAnimationActorsArchetypes, animationActorsArchetypesNumber );

		const arch::componentMask playerTagMask = (1ull << arch::ComponentsIndices::PLAYER_TAG_COMPONENT);
		uint32_t animationActorsCounter = levelChunkActorsCounter;
		for( uint32_t x = 0; x < animationActorsArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedAnimationActorsArchetypes[x];
			const bool isPlayerArchetype = arch::matchesRequiredMask( arch->mask, playerTagMask );
			cm::transform* actorTransforms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			cm::mesh*      actorMeshes     = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			cm::material*  actorMaterials  = (ecs::components::material*)arch->
				components[arch::ComponentsIndices::MATERIAL_COMPONENT];
			cm::rotation*  actorRotations  = (ecs::components::rotation*)arch->
				components[arch::ComponentsIndices::ROTATION_COMPONENT];
			cm::animation* actorAnimations = (ecs::components::animation*)arch->
				components[arch::ComponentsIndices::ANIMATION_COMPONENT];

			for( uint32_t n = 0; n < arch->entityCount; ++n ) {
				[[maybe_unused]] const u32 entity = arch->entities[n];

				cm::transform* transformComponent = &actorTransforms[n];
				cm::material*  materialComponent  = &actorMaterials[n];
				cm::mesh*      meshComponent      = &actorMeshes[n];
				[[maybe_unused]] cm::animation* animationComponent = &actorAnimations[n];
				cm::rotation*  rotationComponent  = &actorRotations[n];
				if( actorTransforms && actorMaterials &&
					actorAnimations && actorRotations ) {

					/// Moving player model faces the direction of its movement (only player has frameMovement)
					if( isPlayerArchetype && ( transformComponent->frameMovement[0] != 0.0f || transformComponent->frameMovement[2] != 0.0f ) ) {
						initializePlayerModelYawOffset( *transformComponent );
						const vec3 movementDirection = Normalize( vec3( transformComponent->frameMovement[0], 0.0f, transformComponent->frameMovement[2] ) );
						transformComponent->pitch                = playerModelYawOffset + directionAngleXZ( movementDirection );
						transformComponent->forward              = transformComponent->frameMovement;
						transformComponent->previousFrameForward = transformComponent->forward;
					}

					const AABB worldAABB = computeWorldAABB( meshComponent->aabb, transformComponent->position );
					const bool isFrustumIntersectFlag = isFrustumIntersect( vulkanRenderer->mainCameraFrustum, worldAABB );

					if( isFrustumIntersectFlag ) {
						vulkanRenderer->collisionsWireframes.Push({});
						u32 meshID       = meshComponent->handle.id;
						const mat4 model = computeModelMatrix(transformComponent, rotationComponent);
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].model    = model;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].position = transformComponent->position;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].scale    = transformComponent->scale;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshID   = meshID;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[meshID];
						++collisionsWireframesCounter;
						
						vulkanRenderer->actors.Push({});
						vulkanRenderer->actors[animationActorsCounter].modelMatrix   = model;
						/// Joint matrices are computed into the animation component and copied to the renderer once
						if( animationComponent->isAnimatedOnFrame || animationComponent->jointMatrices.GetSize() != MAX_JOINTS_NUMBER ) {
							updateAnimationFrames(animationComponent, meshID);
							animationComponent->isAnimatedOnFrame = false;
						}
						vulkanRenderer->actors[animationActorsCounter].jointMatrices = animationComponent->jointMatrices;
						vulkanRenderer->actors[animationActorsCounter].meshID    = meshID;
						vulkanRenderer->actors[animationActorsCounter].diffuseTextureIndex  = materialComponent->diffuseTextureID_.id;
						vulkanRenderer->actors[animationActorsCounter].specularTextureIndex = materialComponent->specularTextureID_.id;
						vulkanRenderer->actors[animationActorsCounter].ambient   = materialComponent->ambient;
						vulkanRenderer->actors[animationActorsCounter].shininess = materialComponent->shininess;
						++animationActorsCounter;
					}
				}
			}
		}

		staticActorsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( staticActorsRequiredMask, cachedStaticActorsArchetypes, staticActorsArchetypesNumber );
		
		uint32_t staticActorsCounter = animationActorsCounter;
		for( uint32_t x = 0; x < staticActorsArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedStaticActorsArchetypes[x];
			cm::transform* staticActorTransforms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			cm::mesh*      staticActorMeshes     = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			cm::material*  staticActorMaterials  = (ecs::components::material*)arch->
				components[arch::ComponentsIndices::MATERIAL_COMPONENT];
			cm::rotation*  staticActorRotations  = (ecs::components::rotation*)arch->
				components[arch::ComponentsIndices::ROTATION_COMPONENT];
			
			const core::vector<mat4>& jointMatrices = identityJointMatrices;     ///< Not animated actors

			for( uint32_t n = 0; n < arch->entityCount; ++n ) {
				cm::transform* transformComponent = &staticActorTransforms[n];
				cm::material*  materialComponent  = &staticActorMaterials[n];
				cm::rotation*  rotationComponent  = &staticActorRotations[n];
				if( staticActorTransforms && staticActorMaterials &&
					staticActorRotations && staticActorMeshes ) {
					unsigned int meshID = staticActorMeshes[n].handle.id;

					const AABB worldAABB = computeWorldAABB( staticActorMeshes[n].aabb, transformComponent->position );
					const bool isFrustumIntersectFlag = isFrustumIntersect( vulkanRenderer->mainCameraFrustum, worldAABB );

					if( isFrustumIntersectFlag ) {
						vulkanRenderer->actors.Push({});
						vulkanRenderer->collisionsWireframes.Push({});

						const mat4 model = computeModelMatrix(transformComponent, rotationComponent);
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].model    = model;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].position = transformComponent->position;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].scale    = transformComponent->scale;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshID   = meshID;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[meshID];
						++collisionsWireframesCounter;
					
						vulkanRenderer->actors[staticActorsCounter].modelMatrix   = model;
						vulkanRenderer->actors[staticActorsCounter].jointMatrices = jointMatrices;
						vulkanRenderer->actors[staticActorsCounter].meshID        = meshID;
						vulkanRenderer->actors[staticActorsCounter].diffuseTextureIndex  = materialComponent->diffuseTextureID_.id;
						vulkanRenderer->actors[staticActorsCounter].specularTextureIndex = materialComponent->specularTextureID_.id;
						vulkanRenderer->actors[staticActorsCounter].ambient   = materialComponent->ambient;
						vulkanRenderer->actors[staticActorsCounter].shininess = materialComponent->shininess;
						++staticActorsCounter;
					}
				}
			}
		}

		projectileActorsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( projectileRequiredMask, cachedProjectileActorsArchetypes, projectileActorsArchetypesNumber );
		
		uint32_t projectileActorsCounter = staticActorsCounter;
		for( uint32_t x = 0; x < projectileActorsArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedProjectileActorsArchetypes[x];
			cm::transform*           actorTransforms         = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			cm::mesh*                actorMeshes             = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			arch::ProjectileBundle*  actorProjectileBundles  = (arch::ProjectileBundle*)arch->
				components[arch::ComponentsIndices::PROJECTILE_BUNDLE_COMPONENT];
			cm::rotation*            actorRotations          = (ecs::components::rotation*)arch->
				components[arch::ComponentsIndices::ROTATION_COMPONENT];

			const core::vector<mat4>& jointMatrices = identityJointMatrices;     ///< Not animated actors
			
			for( uint32_t n = 0; n < arch->entityCount; ++n ) {
				cm::transform* transformComponent = &actorTransforms[n];
				cm::material*  materialComponent  = &actorProjectileBundles[n].material;
				cm::rotation*  rotationComponent  = &actorRotations[n];
				if( actorTransforms && actorProjectileBundles &&
					actorRotations && actorMeshes ) {
					unsigned int meshID = actorMeshes[n].handle.id;
					const AABB worldAABB = computeWorldAABB( actorMeshes[n].aabb, transformComponent->position );
					const bool isFrustumIntersectFlag = isFrustumIntersect( vulkanRenderer->mainCameraFrustum, worldAABB );

					if( isFrustumIntersectFlag ) {
						vulkanRenderer->actors.Push({});
						vulkanRenderer->collisionsWireframes.Push({});

						const mat4 model = computeModelMatrix(transformComponent, rotationComponent);
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].model    = model;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].position = transformComponent->position;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].scale    = transformComponent->scale;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshID   = meshID;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[meshID];
						++collisionsWireframesCounter;
					
						vulkanRenderer->actors[projectileActorsCounter].modelMatrix   = model;
						vulkanRenderer->actors[projectileActorsCounter].jointMatrices = jointMatrices;
						vulkanRenderer->actors[projectileActorsCounter].meshID        = meshID;
						vulkanRenderer->actors[projectileActorsCounter].diffuseTextureIndex  = materialComponent->diffuseTextureID_.id;
						vulkanRenderer->actors[projectileActorsCounter].specularTextureIndex = materialComponent->specularTextureID_.id;
						vulkanRenderer->actors[projectileActorsCounter].ambient   = materialComponent->ambient;
						vulkanRenderer->actors[projectileActorsCounter].shininess = materialComponent->shininess;
						++projectileActorsCounter;
					}
				}
			}
		}


		/*
		 =====================================
		 Item actors renders in the game world
		 =====================================
		 */
		
		itemActorsArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( rotationItemRequiredMask, cachedItemActorsArchetypes, itemActorsArchetypesNumber );
		
		uint32_t itemActorsCounter = projectileActorsCounter;
		for( uint32_t x = 0; x < itemActorsArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedItemActorsArchetypes[x];
			cm::transform*           itemTransforms         = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			cm::mesh*                itemMeshes             = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			cm::material*            itemMaterials          = (cm::material*)arch->
				components[arch::ComponentsIndices::MATERIAL_COMPONENT];
			cm::rotation*            itemRotations          = (ecs::components::rotation*)arch->
				components[arch::ComponentsIndices::ROTATION_COMPONENT];
			cm::item*                items                  = (ecs::components::item*)arch->
				components[arch::ComponentsIndices::ITEM_COMPONENT];

			
			const core::vector<mat4>& jointMatrices = identityJointMatrices;     ///< Not animated actors
			
			for( uint32_t n = 0; n < arch->entityCount; ++n ) {
				if( items[n].isActor ) {
					cm::transform* transformComponent = &itemTransforms[n];
					cm::material*  materialComponent  = &itemMaterials[n];
					cm::rotation*  rotationComponent  = &itemRotations[n];
					if( itemTransforms && itemMaterials &&
						itemRotations && itemMeshes ) {
						unsigned int meshID = itemMeshes[n].handle.id;
						const AABB worldAABB = computeWorldAABB( itemMeshes[n].aabb, transformComponent->position );
						const bool isFrustumIntersectFlag = isFrustumIntersect( vulkanRenderer->mainCameraFrustum, worldAABB );

						if( isFrustumIntersectFlag ) {
							vulkanRenderer->actors.Push({});
							vulkanRenderer->collisionsWireframes.Push({});
							
							const mat4 model = computeModelMatrix(transformComponent, rotationComponent);
							vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].model    = model;
							vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].position = transformComponent->position;
							vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].scale    = transformComponent->scale;
							vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshID   = meshID;
							vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[meshID];
							++collisionsWireframesCounter;
						
							vulkanRenderer->actors[itemActorsCounter].modelMatrix   = model;
							vulkanRenderer->actors[itemActorsCounter].jointMatrices = jointMatrices;
							vulkanRenderer->actors[itemActorsCounter].meshID        = meshID;
							vulkanRenderer->actors[itemActorsCounter].diffuseTextureIndex  = materialComponent->diffuseTextureID_.id;
							vulkanRenderer->actors[itemActorsCounter].specularTextureIndex = materialComponent->specularTextureID_.id;
							vulkanRenderer->actors[itemActorsCounter].ambient   = materialComponent->ambient;
							vulkanRenderer->actors[itemActorsCounter].shininess = materialComponent->shininess;
							++itemActorsCounter;
						}
					}
				}
			}
		}

		
		vulkanRenderer->mathObjects.clear();
		mathObjectArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( mathObjectRequiredMask, cachedMathObjectArchetypes, mathObjectArchetypesNumber );

		uint32_t mathObjectEntityCount = 0;
		for( uint32_t x = 0; x < mathObjectArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedMathObjectArchetypes[x];
			cm::transform *mathObjectTransforms =
				(ecs::components::transform *)arch->components
				[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			cm::rotation*    mathObjectRotations = (ecs::components::rotation*)arch->
				components[arch::ComponentsIndices::ROTATION_COMPONENT];
			[[maybe_unused]] cm::mesh*        mathObjectMeshes    = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			cm::meshGeneration* mathObjectGeneratedMeshes = (ecs::components::meshGeneration*)arch->
				components[arch::ComponentsIndices::MESH_GENERATION_COMPONENT];
			
			for( unsigned int n = 0; n < arch->entityCount; ++n ) {
				cm::transform* mathObjectTransformComponent  = &mathObjectTransforms[n];
				cm::rotation*  mathObjectRotationComponent   = &mathObjectRotations[n];
				cm::meshGeneration* mathObjectGenerationMesh = &mathObjectGeneratedMeshes[n];
				if( mathObjectTransforms && mathObjectGeneratedMeshes ) {
//					const unsigned int meshID = mathObjectMeshes[n].handle.id;
					vulkanRenderer->mathObjects.Push({});

					const mat4 model = computeModelMatrix(mathObjectTransformComponent, mathObjectRotationComponent);
					vulkanRenderer->mathObjects[mathObjectEntityCount].meshID      = mathObjectGenerationMesh->meshID;
					vulkanRenderer->mathObjects[mathObjectEntityCount].modelMatrix = model;
					vulkanRenderer->mathObjects[mathObjectEntityCount].position    = mathObjectTransformComponent->position;
					vulkanRenderer->mathObjects[mathObjectEntityCount].forward     = mathObjectTransformComponent->forward;
					++mathObjectEntityCount;
				}
			}

		}

		vulkanRenderer->players.clear();
		playerArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( playerRequiredMask, cachedPlayerArchetypes, playerArchetypesNumber );


		uint32_t playerEntityCount = 0;
		for( uint32_t x = 0; x < playerArchetypesNumber; ++x ) {
			arch::Archetype* arch = cachedPlayerArchetypes[x];
			cm::transform*   playerTransforms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];
			cm::rotation*    playerRotations = (ecs::components::rotation*)arch->
				components[arch::ComponentsIndices::ROTATION_COMPONENT];
			cm::mesh*        playerMeshes    = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			
			for( unsigned int n = 0; n < arch->entityCount; ++n ) {
				cm::transform* playerTransformComponent = &playerTransforms[n];
				cm::rotation*  playerRotationComponent  = &playerRotations[n];
				if( playerTransforms && playerMeshes ) {
					const unsigned int meshID = playerMeshes[n].handle.id;

					const AABB worldAABB = computeWorldAABB( playerMeshes[n].aabb, playerTransformComponent->position );
					const bool isFrustumIntersectFlag = isFrustumIntersect( vulkanRenderer->mainCameraFrustum, worldAABB );

					if( isFrustumIntersectFlag ) {
						vulkanRenderer->players.Push({});
						vulkanRenderer->collisionsWireframes.Push({});
						
						const mat4 model = computeModelMatrix(playerTransformComponent, playerRotationComponent);
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].model    = model;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].position = playerTransformComponent->position;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].scale    = playerTransformComponent->scale;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshID   = meshID;
						vulkanRenderer->collisionsWireframes[collisionsWireframesCounter].meshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[meshID];
						++collisionsWireframesCounter;
					
						vulkanRenderer->players[playerEntityCount].position = playerTransformComponent->position;
						vulkanRenderer->players[playerEntityCount].forward  = playerTransformComponent->forward;
						++playerEntityCount;                   ///< Counts only pushed (visible) players
					}
				}
			}
		}
	}

    void Engine::loadWavefrontObj() {
        for (unsigned int m = 0; m < pathsArray_.size(); ++m) {
            CWaveFrontObjParser parser;
            CWaveFrontObjParser* wavefrontObjParser = &parser;
            
            if ( !wavefrontObjParser->ReadFile(pathsArray_[m]) )
				throw std::runtime_error(std::string("Can't read Wavefront .obj file: ") + pathsArray_[m]);
            wavefrontObjParser->ParseFile();                                                        ///< Faces are triangles with validated 0-based indices

            vulkanRenderer->aIndices_.emplace_back();
            vulkanRenderer->aVertices_.emplace_back();
			vulkanRenderer->highest_gltf_Y.emplace_back();
			vulkanRenderer->highest_gltf_Y[m] = -999.999f;

			vulkanRenderer->frames.Push({});
			vulkanRenderer->jointMatricesPerMesh.Push({});
            
            unsigned int vertexIndex  = 0;
            unsigned int textureIndex = 0;
			unsigned int normalIndex  = 0;
            unsigned int faceVerticesSize = wavefrontObjParser->getFaces().GetSize();
			vulkanRenderer->meshAxisLimitingValues.setToDefaultValues();
			
            for (unsigned int i = 0; i < faceVerticesSize; ++i)
                for (int j = 0; j < 3; ++j) {
                    vertexIndex     = wavefrontObjParser->getFaces()[i][0][j];
					vulkanRenderer->aIndices_[m].push_back(i * 3 + j);
                    SVertex vertex  = wavefrontObjParser->getCoordinateVertices()[vertexIndex];
                    textureIndex    = wavefrontObjParser->getFaces()[i][1][j];
                    SVertex texture = wavefrontObjParser->getTextureVertices()[textureIndex];
					normalIndex     = wavefrontObjParser->getFaces()[i][2][j];
					SVertex normal  = wavefrontObjParser->getNormals()[normalIndex];

					vec4 jointIndices;
					vec4 weights;

					if ( vertex[1] > vulkanRenderer->highest_gltf_Y[m] )
						vulkanRenderer->highest_gltf_Y[m] = vertex[1];

					if ( vertex[0] < vulkanRenderer->meshAxisLimitingValues.lowest_x ) {
						vulkanRenderer->meshAxisLimitingValues.lowest_x = vertex[0];
					} else if ( vertex[0] > vulkanRenderer->meshAxisLimitingValues.highest_x ) {
						vulkanRenderer->meshAxisLimitingValues.highest_x = vertex[0];
					}

					if ( vertex[1] < vulkanRenderer->meshAxisLimitingValues.lowest_y ) {
						vulkanRenderer->meshAxisLimitingValues.lowest_y = vertex[1];
					} else if ( vertex[1] > vulkanRenderer->meshAxisLimitingValues.highest_y ) {
						vulkanRenderer->meshAxisLimitingValues.highest_y = vertex[1];
					}

					if ( vertex[2] < vulkanRenderer->meshAxisLimitingValues.lowest_z ) {
						vulkanRenderer->meshAxisLimitingValues.lowest_z = vertex[2];
					} else if ( vertex[2] > vulkanRenderer->meshAxisLimitingValues.highest_z ) {
						vulkanRenderer->meshAxisLimitingValues.highest_z = vertex[2];
					}
					
					jointIndices[0] = -1;
					jointIndices[1] = -1;
					jointIndices[2] = -1;
					jointIndices[3] = -1;

					weights[0] = 1.0f;
					weights[1] = 1.0f;
					weights[2] = 1.0f;
					weights[2] = 1.0f;
					
                    vulkanRenderer->aVertices_[m].Push({{vertex[0], vertex[1], vertex[2]},
										{normal[0], normal[1], normal[2]},
										{texture[0], texture[1]},
										{jointIndices[0], jointIndices[1], jointIndices[2]},
										{weights[0], weights[1], weights[2]}});
                }
			setMeshBounds( vulkanRenderer->meshAxisLimitingValues );
			++wavefrontObjCounter;
        }

		ecs::arch::SpatialGrid& spatialGrid = ecs::arch::world.spatialGrid;
		assert( spatialGrid.width > 0 && spatialGrid.height > 0 && spatialGrid.depth > 0 );
		const float chunkSize = spatialGrid.grid[0][0][0].size;

		vulkanRenderer->renderSpacialGrid.chunkSize  = chunkSize;
		vulkanRenderer->renderSpacialGrid.halfWidth  = spatialGrid.width;
		vulkanRenderer->renderSpacialGrid.halfHeight = spatialGrid.height;
		vulkanRenderer->renderSpacialGrid.halfDepth  = spatialGrid.depth;
    }

	void Engine::calculateMeshBounds(const vec4& animatedVertex) {
		if ( animatedVertex.x < vulkanRenderer->meshAxisLimitingValues.lowest_x ) {
			vulkanRenderer->meshAxisLimitingValues.lowest_x = animatedVertex.x;
		} else if ( animatedVertex.x > vulkanRenderer->meshAxisLimitingValues.highest_x ) {
			vulkanRenderer->meshAxisLimitingValues.highest_x = animatedVertex.x;
		}

		if ( animatedVertex.y < vulkanRenderer->meshAxisLimitingValues.lowest_y ) {
			vulkanRenderer->meshAxisLimitingValues.lowest_y = animatedVertex.y;
		} else if ( animatedVertex.y > vulkanRenderer->meshAxisLimitingValues.highest_y ) {
			vulkanRenderer->meshAxisLimitingValues.highest_y = animatedVertex.y;
		}

		if ( animatedVertex.z < vulkanRenderer->meshAxisLimitingValues.lowest_z ) {
			vulkanRenderer->meshAxisLimitingValues.lowest_z = animatedVertex.z;
		} else if ( animatedVertex.z > vulkanRenderer->meshAxisLimitingValues.highest_z ) {
			vulkanRenderer->meshAxisLimitingValues.highest_z = animatedVertex.z;
		}
	}

	namespace
	{
		const char* const kModelsCachePath = "../cache/models/cache";

		/// Changes whenever the model file is re-exported: file size and last write time.
		std::string modelFileStamp( const std::string& modelFilePath ) {
			std::error_code error;
			const std::uintmax_t fileSize = std::filesystem::file_size( modelFilePath, error );
			if ( error )
				return "";
			const auto writeTime = std::filesystem::last_write_time( modelFilePath, error );
			if ( error )
				return "";
			return std::to_string( fileSize ) + ":" + std::to_string( writeTime.time_since_epoch().count() );
		}

		std::string firstToken( const std::string& line ) {
			std::istringstream iss(line);
			std::string token;
			iss >> token;
			return token;
		}
	}

	/*
	  Models cache line: <path> <highest_x> <lowest_x> <highest_y> <lowest_y> <highest_z> <lowest_z> <stamp>.
	  The path must match exactly and the stamp must match the current model file, otherwise the entry is stale.
	  The cache is only an optimization: any I/O problem just disables it.
	*/
	bool Engine::isModelCacheExists( const std::string& modelFilePath ) {
		std::ifstream file(kModelsCachePath);
		if( !file.is_open() ) {
			return false;
		}

		const std::string stamp = modelFileStamp( modelFilePath );
		if( stamp.empty() ) {
			return false;
		}

		std::string line;
		while (std::getline(file, line)) {
			std::istringstream iss(line);

			std::string keyword;
			std::string lineStamp;
			float highest_x, lowest_x, highest_y, lowest_y, highest_z, lowest_z;

			if ( !(iss >> keyword >> highest_x >> lowest_x >> highest_y >> lowest_y >> highest_z >> lowest_z >> lineStamp) ) {
				continue;                                                                       ///< Malformed or old format entry
			}
			if ( keyword != modelFilePath || lineStamp != stamp ) {
				continue;
			}

			std::cout << "Model with file path: " << modelFilePath << " is already exists in cache" << std::endl;
			vulkanRenderer->meshAxisLimitingValues.highest_x = highest_x;
			vulkanRenderer->meshAxisLimitingValues.lowest_x  = lowest_x;
			vulkanRenderer->meshAxisLimitingValues.highest_y = highest_y;
			vulkanRenderer->meshAxisLimitingValues.lowest_y  = lowest_y;
			vulkanRenderer->meshAxisLimitingValues.highest_z = highest_z;
			vulkanRenderer->meshAxisLimitingValues.lowest_z  = lowest_z;

			isAlreadyCached = true;
			return true;
		}

		return false;
	}
	
	void Engine::writeModelsCache( const std::string& modelFilePath ) {
		const std::string stamp = modelFileStamp( modelFilePath );
		if( stamp.empty() || modelFilePath.find_first_of(" \t") != std::string::npos ) {
			return;                                                                             ///< Can't build a reliable cache key
		}

		std::error_code error;
		std::filesystem::create_directories( std::filesystem::path(kModelsCachePath).parent_path(), error );

		/// Keep every other model's entry, replace the entry of this model
		std::vector<std::string> keptLines;
		{
			std::ifstream file(kModelsCachePath);
			std::string line;
			while ( std::getline(file, line) ) {
				if ( !line.empty() && firstToken(line) != modelFilePath ) {
					keptLines.push_back(line);
				}
			}
		}

		std::ofstream modelsCache(kModelsCachePath, std::ios::trunc);
		if( !modelsCache.is_open() ) {
			std::cerr << "Warning: can't write the models cache file " << kModelsCachePath << ", models cache is disabled" << std::endl;
			return;
		}

		for ( const std::string& line : keptLines ) {
			modelsCache << line << std::endl;
		}
		
		modelsCache << modelFilePath;
		modelsCache << " " << vulkanRenderer->meshAxisLimitingValues.highest_x << " " <<
			vulkanRenderer->meshAxisLimitingValues.lowest_x << " " <<
			vulkanRenderer->meshAxisLimitingValues.highest_y << " " <<
			vulkanRenderer->meshAxisLimitingValues.lowest_y << " " <<
			vulkanRenderer->meshAxisLimitingValues.highest_z << " " <<
			vulkanRenderer->meshAxisLimitingValues.lowest_z << " " << stamp << std::endl;

		modelsCache.close();
	}
	
	void Engine::initializeGLTF() {
		core::vector<bool> animationFlags;
		for (unsigned int m = 0; m < pathsGLTF_.GetSize(); ++m) {
			Core::CJsonParser jsonParser;
			vulkanRenderer->aVertexesTemp_.emplace_back();
			vulkanRenderer->aIndices_.emplace_back();
			vulkanRenderer->frames.Push({});
			vulkanRenderer->jointMatricesPerMesh.Push({});
			animationFlags.Push({});
			vulkanRenderer->highest_gltf_Y.emplace_back();
			uint32_t nextIndexGLTF = wavefrontObjCounter + m;
			jsonParser.LoadGLTF(pathsGLTF_[m], vulkanRenderer->aVertexesTemp_[m], vulkanRenderer->aIndices_[nextIndexGLTF],
								vulkanRenderer->jointMatricesPerMesh[nextIndexGLTF], vulkanRenderer->frames[nextIndexGLTF],
								animationFlags[m], vulkanRenderer->highest_gltf_Y[nextIndexGLTF]);
		}

		for (unsigned int m = 0; m < pathsGLTF_.GetSize(); ++m) {
//            aIndices_.emplace_back();
//            aVertices_.emplace_back();
			vulkanRenderer->aVertices_.emplace_back();
			vulkanRenderer->meshAxisLimitingValues.setToDefaultValues();

			isAlreadyCached = false;
			isModelCacheExists( pathsGLTF_[m] );
			
			int stepOffset = 0;
			if ( animationFlags[m] )
				stepOffset = 8;
			else
				stepOffset = 16;
			
			for ( unsigned int n = 0; n < vulkanRenderer->aVertexesTemp_[m].size(); n += stepOffset ) {
				SVertex vertex;
				vertex[0] = vulkanRenderer->aVertexesTemp_[m][n];
			    vertex[1] = vulkanRenderer->aVertexesTemp_[m][n + 1];
				vertex[2] = vulkanRenderer->aVertexesTemp_[m][n + 2];
				SVertex normal;
				normal[0] = vulkanRenderer->aVertexesTemp_[m][n + 3];
				normal[1] = vulkanRenderer->aVertexesTemp_[m][n + 4];
				normal[2] = vulkanRenderer->aVertexesTemp_[m][n + 5];
				SVertex texture;
				texture[0] = vulkanRenderer->aVertexesTemp_[m][n + 6];
				texture[1] = vulkanRenderer->aVertexesTemp_[m][n + 7];
				
				vec4 joinIndices;
				vec4 weights;
				if ( animationFlags[m] ) {
					joinIndices[0] = -1;
					joinIndices[1] = -1;
					joinIndices[2] = -1;
					joinIndices[3] = -1;

					weights[0] = 1;
					weights[1] = 1;
					weights[2] = 1;
					weights[3] = 1;
					
				} else {
					joinIndices[0] = vulkanRenderer->aVertexesTemp_[m][n + 8];
					joinIndices[1] = vulkanRenderer->aVertexesTemp_[m][n + 9];
					joinIndices[2] = vulkanRenderer->aVertexesTemp_[m][n + 10];
					joinIndices[3] = vulkanRenderer->aVertexesTemp_[m][n + 11];

					weights[0] = vulkanRenderer->aVertexesTemp_[m][n + 12];
					weights[1] = vulkanRenderer->aVertexesTemp_[m][n + 13];
					weights[2] = vulkanRenderer->aVertexesTemp_[m][n + 14];
					weights[3] = vulkanRenderer->aVertexesTemp_[m][n + 15];
				}

				uint32_t nextIndexGLTF = wavefrontObjCounter + m;
				vulkanRenderer->aVertices_[nextIndexGLTF].Push({{vertex[0], vertex[1], vertex[2]},
										 {normal[0], normal[1], normal[2]},
										 {texture[0], texture[1]},
										 {joinIndices[0], joinIndices[1], joinIndices[2], joinIndices[3]},
										 {weights[0], weights[1], weights[2], weights[3]}});

				if( isAlreadyCached ) {
					continue;
				}
							
				vec4 animatedVertex = vec4(vertex[0], vertex[1], vertex[2], 1.0);
				if( !animationFlags[m] && vulkanRenderer->jointMatricesPerMesh[nextIndexGLTF].GetSize() > 0 ) {
					for( unsigned int frame = 0; frame < vulkanRenderer->jointMatricesPerMesh[nextIndexGLTF][0].GetSize(); ++frame ) {
						mat4 skinMatrix =
							(vulkanRenderer->jointMatricesPerMesh[nextIndexGLTF][int(joinIndices[0])][frame] * weights[0]) +
							(vulkanRenderer->jointMatricesPerMesh[nextIndexGLTF][int(joinIndices[1])][frame] * weights[1]) +
							(vulkanRenderer->jointMatricesPerMesh[nextIndexGLTF][int(joinIndices[2])][frame] * weights[2]) +
							(vulkanRenderer->jointMatricesPerMesh[nextIndexGLTF][int(joinIndices[3])][frame] * weights[3]);

						animatedVertex = vec4(vertex[0], vertex[1], vertex[2], 1.0) * skinMatrix;
						calculateMeshBounds( animatedVertex );
					}
				} else {
					calculateMeshBounds( animatedVertex );
				}
			}

			if( !isAlreadyCached ) {
				writeModelsCache( pathsGLTF_[m] );
			}
			setMeshBounds( vulkanRenderer->meshAxisLimitingValues );
		}
	}

	void Engine::initializeFontData() {
		constexpr float fontStep = 1.0 / 12;
		constexpr unsigned int glyph_row = 7;
		constexpr unsigned int glyph_column = 12;

		vulkanRenderer->fontVertexBufferContainer.resize(128);
		vulkanRenderer->fontVertexBufferMemoryContainer.resize(128);

		vulkanRenderer->fontIndexBufferContainer.resize(128);
		vulkanRenderer->fontIndexBufferMemoryContaner.resize(128);
		
		for ( unsigned int i = 0; i < glyph_row; ++i )
			for ( unsigned int j = 0; j < glyph_column; ++j ) {
				core::vector<Vertex> symbol_g_vertices;
					symbol_g_vertices.Push({{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {fontStep * j, fontStep * i + fontStep}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}});
					symbol_g_vertices.Push({{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}, {fontStep * j + fontStep, fontStep * i + fontStep}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}});
					symbol_g_vertices.Push({{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {fontStep * j, fontStep * i}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}});
					symbol_g_vertices.Push({{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {fontStep * j + fontStep, fontStep * i}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}});
				unsigned int currentBufferIndex = i * glyph_column + j;

				bool exitFlag = false;
				const unsigned int nextBufferIndex = static_cast<const unsigned int>(vulkanRenderer->glyphs[currentBufferIndex]);
				for ( unsigned int n = 0; n < vulkanRenderer->fontIndicesContainer.size(); ++n ) {                 ///< TODO: Fix gabage algo
					if ( nextBufferIndex == vulkanRenderer->fontIndicesContainer[n] )
						exitFlag = true;
				}

				if ( exitFlag )
					continue;

				vulkanRenderer->symbolGVerticesContainer.Push(symbol_g_vertices);
				vulkanRenderer->fontIndicesContainer.push_back(nextBufferIndex);
			}
	}

	void Engine::initializeMathObjectsData() {
		namespace cm = GLVM::ecs::components;
		namespace arch = GLVM::ecs::arch;

		mathObjectArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( mathObjectRequiredMask, cachedMathObjectArchetypes, mathObjectArchetypesNumber );

		u32 generatedMeshesCounter = 0;                         ///< Mesh id is an index in mathObjectsVertices common for all archetypes
		for( u32 i0 = 0; i0 < mathObjectArchetypesNumber; ++i0 ) {
			arch::Archetype* arch = cachedMathObjectArchetypes[i0];
			cm::meshGeneration* mathObjectGeneratedMeshes = (ecs::components::meshGeneration*)arch->
				components[arch::ComponentsIndices::MESH_GENERATION_COMPONENT];
			cm::transform* transformGeneratedMeshes = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];

			for( u32 i1 = 0; i1 < arch->entityCount; ++i1 ) {
				const cm::transform* transform = &transformGeneratedMeshes[i1];
				mathObjectGeneratedMeshes[i1].meshID = generatedMeshesCounter++;
				const core::vector<vec3>& vertices = mathObjectGeneratedMeshes[i1].vertices;
				const u32 verticesNumber = vertices.GetSize();

				if( verticesNumber == 3) {
					std::vector<uint32_t> indices;
					for ( unsigned int i = 0; i < 3; ++i )
						indices.push_back(triangleIndexBufferData[i]);

					vulkanRenderer->mathObjectsIndices.push_back( indices );
				} else if( verticesNumber == 8 ) {
					std::vector<uint32_t> indices;
					for ( unsigned int i = 0; i < 24; ++i )
						indices.push_back(boxIndexBufferDataLineMode[i]);

					vulkanRenderer->mathObjectsIndices.push_back( indices );
				} else if( verticesNumber == 2 ) {
					std::vector<uint32_t> indices;
					for ( unsigned int i = 0; i < 2; ++i )
						indices.push_back(vectorIndexBufferData[i]);

					vulkanRenderer->mathObjectsIndices.push_back( indices );
				} else if( verticesNumber == 4 ) {
					std::vector<uint32_t> indices;
					for ( unsigned int i = 0; i < 8; ++i )
						indices.push_back(planeIndexBufferData[i]);

					vulkanRenderer->mathObjectsIndices.push_back( indices );
				} else {
					throw std::runtime_error("Passing wrong vertices number for generation mesh");
				}

				core::vector<core::Vertex> verticesVK;
				SVertex normal;
				normal[0] = 0;
				normal[1] = 1;
				normal[2] = 0;
				SVertex texture;
				texture[0] = 0;
				texture[1] = 1;
				for( u32 i2 = 0; i2 < verticesNumber; ++i2 ) {
					vec3 vertex = vertices[i2];

					mat4 view = LookAtMain( vec3(0.0, 0.0, 0.0),
											transform->forward,
											vec3( 0.0f, -1.0f, 0.0) );

					/// Debug geometry is generated once, so aspect rate of the moment of initialization is used
					const mat4 projectionMatrix = SetProjectionMatrix( 90.0f, getViewportAspectRate(), 1.0f, 0.1f, 100.0f );
					mat4 vp = view * projectionMatrix;
					vp = inverse_matrix_4x4( vp );
					const vec4 tempVector = vec4( vertex[0], vertex[1], vertex[2], 1.0 ) * vp;

					// std::cout << "VIEW MATRIX" << std::endl;
					// std::cout << view << std::endl;
					// std::cout << "PROJECTION MATRIX" << std::endl;
					// std::cout << vulkanRenderer->projectionMatrix << std::endl;
					// std::cout << "VP MATRIX" << std::endl;
					// std::cout << vp << std::endl;

					// printf("det(VP) = %f\n", determinant_4x4(vp));
						
					// printf(
					// 	"NDC (%f %f %f) -> (%f %f %f %f)\n",
					// 	vertex[0], vertex[1], vertex[2],
					// 	tempVector[0], tempVector[1], tempVector[2], tempVector[3]
					// 	);
					
					/// Points at infinity (w == 0) can not be projected back
					const float w = std::abs( tempVector[3] ) > 1e-6f ? tempVector[3] : 1e-6f;
					vertex = vec3(tempVector[0] / w, tempVector[1] / w, tempVector[2] / w);

					SVertex vertexVK;
					vertexVK[0] = vertex[0];
					vertexVK[1] = vertex[1];
					vertexVK[2] = vertex[2];

					verticesVK.Push({{vertexVK[0], vertexVK[1], vertexVK[2]},
								   {normal[0], normal[1], normal[2]},
								   {texture[0], texture[1]},
								   { -1, -1, -1, -1 },
								   { 1, 1, 1, 1 }});
				}

				vulkanRenderer->mathObjectsVertices.Push( verticesVK );
			}

		}
	}

	void Engine::initializeAABB() {
		namespace cm   = GLVM::ecs::components;
		namespace arch = GLVM::ecs::arch;
		
		meshObjectArchetypesNumber = 0;
		arch::world.searchCacheArchetypes( meshObjectRequiredMask, cachedMeshObjectArchetypes, meshObjectArchetypesNumber );

		for( u32 i0 = 0; i0 < meshObjectArchetypesNumber; ++i0 ) {
			arch::Archetype* arch = cachedMeshObjectArchetypes[i0];
			cm::mesh* meshes          = (ecs::components::mesh*)arch->
				components[arch::ComponentsIndices::MESH_COMPONENT];
			cm::transform* transforms = (ecs::components::transform*)arch->
				components[arch::ComponentsIndices::TRANSFORM_COMPONENT];

			for( uint32_t n = 0; n < arch->entityCount; ++n ) {
				cm::transform* transformComponent = &transforms[n];
				cm::mesh*      meshComponent      = &meshes[n];

				unsigned int meshID               = meshComponent->handle.id;
				if ( meshID >= allMeshMaxAbsoluteValues.GetSize() )
					continue;

				const GLVM::core::MeshAxisMaxAbsoluteValues& meshAxisMaxAbsoluteValues = allMeshMaxAbsoluteValues[meshID];
				const AABB localAABB = computeLocalAABB( transformComponent->scale,
														 vec3( meshAxisMaxAbsoluteValues.origin_offset_x,
															   meshAxisMaxAbsoluteValues.origin_offset_y,
															   meshAxisMaxAbsoluteValues.origin_offset_z ),
														 vec3( meshAxisMaxAbsoluteValues.absolute_x,
															   meshAxisMaxAbsoluteValues.absolute_y,
															   meshAxisMaxAbsoluteValues.absolute_z ) );
				meshComponent->aabb = localAABB;
			}
		}
	}
	
	mat4 Engine::computeModelMatrix(ecs::components::transform* _transformComponent, [[maybe_unused]] ecs::components::rotation* rotation) {
		mat4 rotationMatrix(1.0f);
        mat4 scalingMatrix(1.0f);
        mat4 translationMatrix(1.0f);
		
		scalingMatrix[0][0] = _transformComponent->scale;
		scalingMatrix[1][1] = _transformComponent->scale;
		scalingMatrix[2][2] = _transformComponent->scale;

		translationMatrix[3][0] = _transformComponent->position[0];
		translationMatrix[3][1] = _transformComponent->position[1];
		translationMatrix[3][2] = _transformComponent->position[2];
		translationMatrix[3][3] = 1.0f;

		const float yaw   = _transformComponent->yaw;
		const float pitch = _transformComponent->pitch;

		const float halfPitch = pitch * 0.5f;
		const float halfYaw   = yaw   * 0.5f;

		const Quaternion qPitch( cosf(halfPitch), 0.0f, sinf(halfPitch), 0.0f );
		const Quaternion qYaw( cosf(halfYaw), sinf(halfYaw), 0.0f, 0.0f );

		const Quaternion rotationQuat = qPitch * qYaw;
		rotationMatrix = rotateQuaternion<float, 4>(rotationQuat);
		
		rotationMatrix.SelfTensorTranspose();
        return scalingMatrix * rotationMatrix * translationMatrix;
	}
	
	void Engine::computeHudScreeenCoordinates() {
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
		hud_screen_y -= g_eEvent.mousePointerPosition.offset_Y / 1080.0f;
		hud_screen_x += g_eEvent.mousePointerPosition.offset_X / 1920.0f;
#else
//		hud_screen_y -= (previousMouseOffsetY - g_eEvent.mousePointerPosition.offset_Y) / 1080.0f;
//		hud_screen_x += (previousMouseOffsetX - g_eEvent.mousePointerPosition.offset_X) / 1920.0f;
		hud_screen_y -= g_eEvent.mousePointerPosition.offset_Y / 1080.0f;
		hud_screen_x += g_eEvent.mousePointerPosition.offset_X / 1920.0f;
		previousMouseOffsetX = g_eEvent.mousePointerPosition.offset_X;
		previousMouseOffsetY = g_eEvent.mousePointerPosition.offset_Y;
#endif

		if ( hud_screen_x > 1.0f )
			hud_screen_x = 1.0f;
		else if ( hud_screen_x < -1.0f )
			hud_screen_x = -1.0f;
		
		if ( hud_screen_y > 1.0f )
			hud_screen_y = 1.0f;
		else if ( hud_screen_y < -1.0f )
			hud_screen_y = -1.0f;
	}
	
	ecs::TextureHandle Engine::LoadTextureFromFile(const char* path_to_texture) {
		uint32_t textureID = textureVector.size();
		ecs::TextureHandle textureHandle;
		textureHandle.id = textureID;
		textureVector.push_back({ .path_to_image = path_to_texture });
		textureHandlers.Push(textureHandle);

		return textureHandle;
	}
	
	ecs::TextureHandle Engine::LoadTextureFromAddress(unsigned int iWidth, unsigned int iHeight,
								  unsigned int dat_length, unsigned char* u_iData) {
		uint32_t textureID = textureVector.size();
		ecs::TextureHandle textureHandle;
		textureHandle.id = textureID;
		textureVector.push_back({ .iWidth_ = iWidth, .iHeight_ = iHeight, .dat_length_ = dat_length, .u_iData_ = u_iData});
		textureHandlers.Push(textureHandle);

		return textureHandle;
    }

	ecs::components::MeshHandle Engine::LoadMeshFromFile_OBJ(const char* _pathToMesh) {
		ecs::components::MeshHandle meshHandle;
		meshHandle.id = meshID;
        pathsArray_.push_back(_pathToMesh);
		meshHandlers.Push(meshHandle);
		++meshID;

		return meshHandle;
    }

	ecs::components::MeshHandle Engine::LoadMeshFromFile_GLTF(const char* pathToMesh) {
		ecs::components::MeshHandle meshHandle;
		meshHandle.id = meshID;
        pathsGLTF_.Push(pathToMesh);
		meshHandlers.Push(meshHandle);
		++meshID;

		return meshHandle;
	}

	ecs::components::MeshHandle Engine::LoadMesh() {
		ecs::components::MeshHandle meshHandle;
		meshHandle.id = meshID;
		meshHandlers.Push(meshHandle);
		++meshID;

		return meshHandle;
	}
	
	void Engine::FPScounter() {
		++fpsCounter;
		fpsAccumulator += deltaFrameTime;
		if (fpsAccumulator > 1.0f) {
			std::cout << "FPS: " << fpsCounter << std::endl;
			fpsCounter = 0;
			fpsAccumulator = 0;
		}
	}
	
    void Engine::GameKill()
    {
		/// Stop the sound thread first and only then close the device: the thread can write into it while playing
		runningSound = false;
		if ( soundEngine )
			soundEngine->StopStream();

		if (sound_thread.joinable())
		{
			sound_thread.join();
		}

		if ( soundEngine )
			soundEngine->CloseDevice();
		delete soundEngine;
		soundEngine = nullptr;
		
		delete chrono;
		chrono = nullptr;
		delete collisionSystem;
		collisionSystem = nullptr;
		delete movementSystem;
		movementSystem = nullptr;
		delete physicsSystem;
		physicsSystem = nullptr;
		delete projectileSystem;
		projectileSystem = nullptr;
		delete damageSystem;
		damageSystem = nullptr;
		delete enemySytem;
		enemySytem = nullptr;
		delete itemSystem;
		itemSystem = nullptr;
		delete spatialGridSystem;
		spatialGridSystem = nullptr;
		delete procuduralLevelGeneratingSystem;
		procuduralLevelGeneratingSystem = nullptr;
		delete inventorySystem;
		inventorySystem = nullptr;
		// delete pSystem_Manager;
		// pSystem_Manager = nullptr;
    }
} // namespace GLVM::core
