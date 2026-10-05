// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  A glass tank of water as an object of a host scene (the game level): a piston driven by a linear actuator on top of
  the tank makes waves, beach balls and a crate float, host objects (projectiles) splash into the water.

  The tank renders itself into the frame of the host renderer after the host main pass:
    1. opaque parts (steel frame, tiles, piston, actuator, bodies) and the far glass walls into the host color and depth;
    2. the scene behind the water is copied (refraction), the fluid is composited over the frame (FluidRenderer);
    3. the near glass walls.
  The host color image needs TRANSFER_SRC usage, the host depth buffer SAMPLED usage and a main pass that stores it.
  The device needs Vulkan 1.3 dynamic rendering and a queue with graphics and compute.
*/

#ifndef GLVM_FLUID_TANK_HPP
#define GLVM_FLUID_TANK_HPP

#include "Fluid/FluidRenderer.hpp"

#include <memory>
#include <vector>

namespace GLVM::fluid
{
	struct FluidTankDescription {
		Vec3  floorCenter     = { 0.0f, 0.0f, 0.0f };      ///< Where the tank stands: center of its base on the host floor
		Vec3  innerSize       = { 3.0f, 1.4f, 1.6f };      ///< Inside of the glass: length (x, the piston moves along it), height, depth
		float waterDepth      = 0.55f;
		float particleSpacing = 0.05f;
		bool  isHighQuality   = false;                     ///< 4 solver iterations instead of 3 (discrete GPUs)
	};

	/// Host frame the tank renders into.
	struct FluidTankTarget {
		VkImage            colorImage  = VK_NULL_HANDLE;   ///< TRANSFER_SRC usage: the scene behind the water is copied
		VkImageView        colorView   = VK_NULL_HANDLE;
		VkImageLayout      colorLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;                      ///< Before and after the tank
		VkImage            depthImage  = VK_NULL_HANDLE;
		VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;                            ///< Depth and stencil for combined formats
		VkImageLayout      depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;     ///< Before and after the tank
	};

	/// Camera and light of the host frame.
	struct FluidTankCamera {
		Mat4  view;                                        ///< Host matrices: column-major, Vulkan clip space with depth 0..1;
		Mat4  projection;                                  ///< the y direction and the handedness of the view may be any
		float nearPlane        = 0.1f;
		float farPlane         = 100.0f;
		float verticalFov      = 1.5708f;                  ///< Radians
		Vec3  lightDirection   = { 0.3f, 0.9f, 0.3f };     ///< Towards the host light
		Vec3  lightColor       = { 1.0f, 1.0f, 1.0f };
		float lightIntensity   = 1.0f;
		Vec3  ambientColor     = { 0.3f, 0.3f, 0.3f };
		Vec3  environmentColor = { 0.4f, 0.42f, 0.45f };   ///< Surroundings seen in the reflections of the water and the glass
	};

	/// A host object that pushes the water this frame (a kinematic sphere), e.g. a projectile.
	struct FluidSplasher {
		Vec3  position;
		Vec3  velocity;
		float radius = 0.15f;
	};

	class FluidTank {
	public:
		static constexpr uint32_t MAX_SPLASHERS = 6;

		/// colorFormat, depthFormat: formats of the host frame.
		FluidTank( const GpuContext& context, const FluidTankDescription& description, VkFormat colorFormat, VkFormat depthFormat );
		~FluidTank();
		FluidTank( const FluidTank& ) = delete;
		FluidTank& operator=( const FluidTank& ) = delete;

		/// Size of the host frame and its depth buffer view (depth aspect). Call at creation and when they change.
		void setTargets( uint32_t width, uint32_t height, VkImageView depthView );
		/// Host objects in the water this frame (at most MAX_SPLASHERS, the rest is ignored).
		void setSplashers( const std::vector<FluidSplasher>& splashers );

		/// Moves the piston and records the simulation steps of the frame time (fixed steps of 1/60 s, at most 2 per frame),
		/// outside of rendering (before the host passes).
		void recordSimulation( VkCommandBuffer commandBuffer, float frameTime );
		/// Renders the tank into the host frame, outside of rendering, after the host main pass.
		void recordRendering( VkCommandBuffer commandBuffer, const FluidTankTarget& target, const FluidTankCamera& camera );

		/// Bounds of the whole object (frame and actuator included), for culling.
		Vec3 boundsMin() const { return boundsMin_; }
		Vec3 boundsMax() const { return boundsMax_; }
		/// Moves a point with a radius (a character) out of the tank; returns whether it was inside.
		bool pushOut( Vec3& position, float radius ) const;
		/// Whether a point is inside the water box (glass walls, up to the domain top).
		bool isInsideWater( const Vec3& position ) const;
		uint32_t particleCount() const { return simulation_->particleCount(); }

	private:
		enum Mesh { CUBE, OPEN_BOX, SPHERE, MESH_COUNT };
		struct Object {
			Mesh  mesh = CUBE;
			Mat4  model;
			float color[4] = { 1.0f, 1.0f, 1.0f, 0.0f };   ///< rgb albedo, w material (tank_common.glsl)
		};

		const GpuContext&                context_;
		FluidTankDescription             description_;
		VkFormat                         colorFormat_;
		std::unique_ptr<FluidSimulation> simulation_;
		std::unique_ptr<FluidRenderer>   renderer_;

		Vec3                             innerMin_, innerMax_, boundsMin_, boundsMax_;
		float                            glassThickness_ = 0.015f;
		float                            time_ = 0.0f;                 ///< Simulated time
		float                            accumulatedTime_ = 0.0f;      ///< Host time not simulated yet (fixed steps)
		FluidBody                        piston_;
		float                            pistonRest_ = 0.0f;           ///< x of the piston plate center at rest
		float                            pistonHalfHeight_ = 0.0f;     ///< Of the visible plate, it stands on the bottom
		std::vector<FluidSplasher>       splashers_;
		uint32_t                         activeSplashers_ = 0;         ///< Splasher bodies active on the GPU

		GpuBuffer                        vertices_;
		GpuBuffer                        parameters_;
		GpuImage                         sceneCopy_;                   ///< The host frame behind the water
		VkImageView                      depthView_ = VK_NULL_HANDLE;  ///< Host depth buffer
		VkSampler                        linearSampler_ = VK_NULL_HANDLE;
		VkSampler                        nearestSampler_ = VK_NULL_HANDLE;
		VkDescriptorSetLayout            setLayout_ = VK_NULL_HANDLE;
		VkDescriptorPool                 descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSet                  set_ = VK_NULL_HANDLE;
		Pipeline                         objectPipeline_;
		Pipeline                         glassPipeline_;
		uint32_t                         meshFirst_[MESH_COUNT] = {};
		uint32_t                         meshCount_[MESH_COUNT] = {};
		std::vector<Object>              staticObjects_;
		Object                           glass_;

		void createScene();
		void createGpuResources( VkFormat depthFormat );
		float pistonX() const;
		std::vector<Object> movingObjects() const;
		void drawObject( VkCommandBuffer commandBuffer, const Pipeline& pipeline, const Object& object, uint32_t flags ) const;
		void drawBodies( VkCommandBuffer commandBuffer ) const;
	};
}

#endif
