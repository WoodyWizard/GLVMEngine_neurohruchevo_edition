// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  GPU liquid simulation: Position Based Fluids (Macklin & Mueller, "Position Based Fluids", SIGGRAPH 2013),
  the method of NVIDIA Flex, with
  - neighbor search on a uniform grid with counting sort (particles of a cell are contiguous in memory),
  - analytic wall density (no boundary particles, no sticking to the walls),
  - XSPH viscosity, vorticity confinement, artificial pressure against clustering,
  - two way coupled rigid bodies (spheres, boxes): buoyancy and drag come from the particle impulses,
  - dye colors that diffuse and mix, a foam measure from trapped air,
  - a pump that moves particles from a drain region to a nozzle (fountains without growing particle counts).

  Units are meters, seconds and kilograms. The simulation records its work into a command buffer
  of the host renderer; the particle buffers can be read by rendering shaders afterwards.
*/

#ifndef GLVM_FLUID_SIMULATION_HPP
#define GLVM_FLUID_SIMULATION_HPP

#include "Fluid/GpuContext.hpp"

#include <array>

namespace GLVM::fluid
{
	/*
	  Quality presets (see tests/FluidTests.cpp for the measurements): the defaults are 2 substeps x 4 iterations,
	  a pool settles to ~0.005 m/s and is compressed by ~4%. FluidSettings::fast() uses 3 iterations with a softer
	  constraint: 25% cheaper, as calm at rest, compressed by ~6%.
	*/
	struct FluidSettings {
		uint32_t maxParticles   = 131072;
		uint32_t maxBodies      = 32;
		float    particleSpacing = 0.02f;                                  ///< Rest distance between particles (meters)
		float    kernelScale     = 2.0f;                                   ///< Kernel radius h = kernelScale * particleSpacing
		std::array<float, 3> domainMin = { 0.0f, 0.0f, 0.0f };             ///< Inner faces of the container
		std::array<float, 3> domainMax = { 1.0f, 1.0f, 1.0f };
		std::array<float, 3> gravity   = { 0.0f, -9.81f, 0.0f };
		uint32_t substeps        = 2;                                      ///< Per frame
		uint32_t iterations      = 4;                                      ///< Density solver iterations per substep
		float    relaxation      = 0.5f;                                   ///< Constraint softness epsilon, relative to the rest gradient sum
		float    artificialPressure = 0.02f;                               ///< s_corr strength, relative to the rest gradient sum
		float    artificialPressureDistance = 0.25f;                       ///< dq / h of s_corr
		float    viscosity       = 0.08f;                                  ///< XSPH factor
		float    vorticity       = 0.04f;                                  ///< Vorticity confinement factor
		float    maxSpeed        = 12.0f;
		float    wallFriction    = 0.1f;
		float    dyeDiffusion    = 0.01f;
		float    waterDensity    = 1000.0f;                                ///< kg / m^3, gives the particle mass for rigid coupling
		float    bodyRestitution = 0.25f;

		static FluidSettings fast() {
			FluidSettings settings;
			settings.iterations = 3;
			settings.relaxation = 1.0f;
			settings.viscosity  = 0.1f;
			return settings;
		}
	};

	enum class BodyType : uint32_t { NONE = 0, SPHERE = 1, BOX = 2 };

	struct FluidBody {
		BodyType             type = BodyType::NONE;
		std::array<float, 3> position = { 0.0f, 0.0f, 0.0f };
		std::array<float, 4> rotation = { 0.0f, 0.0f, 0.0f, 1.0f };        ///< Quaternion xyzw
		std::array<float, 3> halfExtents = { 0.1f, 0.1f, 0.1f };          ///< Box half sizes, x = radius for spheres
		std::array<float, 3> velocity = { 0.0f, 0.0f, 0.0f };
		std::array<float, 3> angularVelocity = { 0.0f, 0.0f, 0.0f };
		float                density = 500.0f;                             ///< kg / m^3, 0 = kinematic (moved only by the application)
		float                damping = 0.2f;
		std::array<float, 4> color = { 0.6f, 0.4f, 0.2f, 0.0f };           ///< rgb albedo, w material id for the renderer
	};

	/// GPU layout of a body (std430), see VKshaders/fluid/pbf_common.glsl.
	struct GpuBody {
		float positionType[4];
		float rotation[4];
		float halfExtents[4];
		float velocityInverseMass[4];
		float angularVelocity[4];
		float inverseInertia[4];
		float color[4];
	};
	static_assert(sizeof(GpuBody) == 112, "GpuBody must match the shader layout");

	struct FluidPump {
		bool                 isEnabled = false;
		std::array<float, 3> drainMin = { 0.0f, 0.0f, 0.0f };              ///< Particles in this box are pumped
		std::array<float, 3> drainMax = { 0.0f, 0.0f, 0.0f };
		std::array<float, 3> nozzle = { 0.0f, 0.0f, 0.0f };
		float                nozzleRadius = 0.05f;
		std::array<float, 3> velocity = { 0.0f, 0.0f, 0.0f };              ///< Jet velocity
		std::array<float, 3> color = { 0.2f, 0.5f, 1.0f };
	};

	/// Values derived from the settings.
	struct FluidConstants {
		float    kernelRadius = 0.0f;
		float    particleRadius = 0.0f;                                    ///< Collision radius (half the spacing)
		float    restDensity = 0.0f;                                       ///< Kernel sum of a particle in a rest lattice (unit masses)
		float    restGradientSum = 0.0f;                                   ///< sum_k |grad_k C|^2 in a rest lattice
		float    particleMass = 0.0f;                                      ///< kg
		uint32_t gridDims[3] = { 0, 0, 0 };
		uint32_t cellCount = 0;
	};

	/// GPU time of the simulation stages of one frame, milliseconds.
	struct FluidTimings {
		float sort = 0.0f;                                                 ///< Prediction and counting sort by grid cell
		float neighborSearch = 0.0f;                                       ///< Neighbor lists with the first lambda
		float solver = 0.0f;                                               ///< Density constraint iterations
		float velocity = 0.0f;                                             ///< XSPH viscosity, vorticity confinement, dye, foam
		float bodies = 0.0f;
		float total = 0.0f;
	};

	/// Particle data for uploads and downloads.
	struct FluidParticle {
		float position[4];                                                 ///< w: free for the application
		float velocity[4];                                                 ///< w: particle id
		float color[4];                                                    ///< rgb dye, w foam
	};

	class FluidSimulation {
	public:
		FluidSimulation( const GpuContext& context, const FluidSettings& settings );
		~FluidSimulation();
		FluidSimulation( const FluidSimulation& ) = delete;
		FluidSimulation& operator=( const FluidSimulation& ) = delete;

		const FluidSettings&  settings() const { return settings_; }
		const FluidConstants& constants() const { return constants_; }
		uint32_t particleCount() const { return particleCount_; }

		/// Removes all particles and bodies (immediate).
		void reset();
		/// Fills a box with particles on a lattice of the particle spacing (immediate upload). Returns the number added.
		uint32_t addBlock( const std::array<float, 3>& minimum, const std::array<float, 3>& maximum,
						   const std::array<float, 3>& velocity, const std::array<float, 3>& color );
		/// Adds particles inside a sphere.
		uint32_t addSphere( const std::array<float, 3>& center, float radius, const std::array<float, 3>& velocity,
							const std::array<float, 3>& color );
		uint32_t addParticles( const std::vector<FluidParticle>& particles );

		/// Sets body slot `index` (immediate upload). Slots above the highest used one are not simulated.
		void setBody( uint32_t index, const FluidBody& body );
		/// Records an update of a body (e.g. a kinematic paddle) into a command buffer, outside of rendering.
		void recordBodyUpdate( VkCommandBuffer commandBuffer, uint32_t index, const FluidBody& body );
		uint32_t bodyCount() const { return bodyCount_; }
		void setPump( const FluidPump& pump ) { pump_ = pump; }
		void setGravity( const std::array<float, 3>& gravity ) { settings_.gravity = gravity; }

		/// Records one step of simulation: substeps of frameTime / substeps. Must be outside of rendering.
		/// Use the same frameTime every step (e.g. 1/60 s with an accumulator of the host frame time): the velocities are the
		/// position corrections divided by the step, a much shorter step turns the remaining corrections into jumps.
		void recordStep( VkCommandBuffer commandBuffer, float frameTime );

		/// Buffers with the current state (after the recorded steps), particleCount() elements.
		const GpuBuffer& positionBuffer() const { return positions_[stateParity_]; }
		const GpuBuffer& velocityBuffer() const { return velocities_[stateParity_]; }
		const GpuBuffer& colorBuffer() const { return colors_[stateParity_]; }
		const GpuBuffer& bodyBuffer() const { return bodies_; }
		/// Both buffers of the double buffered state (descriptor sets can be prepared once), stateParity() is the current one.
		uint32_t stateParity() const { return stateParity_; }
		const GpuBuffer& positionBuffer( uint32_t parity ) const { return positions_[parity & 1u]; }
		const GpuBuffer& velocityBuffer( uint32_t parity ) const { return velocities_[parity & 1u]; }
		const GpuBuffer& colorBuffer( uint32_t parity ) const { return colors_[parity & 1u]; }

		/// GPU timestamps around the simulation stages (needs a queue with timestamp support).
		void setProfiling( bool isEnabled ) { isProfiling_ = isEnabled; }
		/// Timings of the latest frame whose results are available (the GPU runs behind the CPU, nothing waits).
		FluidTimings timings();

		/// Reads the current particles back (waits for the GPU). Used by tests and tools.
		std::vector<FluidParticle> downloadParticles() const;
		std::vector<GpuBody> downloadBodies() const;
		/// GPU layout of a body.
		GpuBody toGpuBody( const FluidBody& body ) const;

	private:
		const GpuContext&     context_;
		FluidSettings         settings_;
		FluidConstants        constants_;
		FluidPump             pump_;
		uint32_t              particleCount_ = 0;
		uint32_t              nextParticleId_ = 0;
		uint32_t              bodyCount_ = 0;
		uint32_t              stateParity_ = 0;

		GpuBuffer             parameters_;
		GpuBuffer             positions_[2];
		GpuBuffer             velocities_[2];
		GpuBuffer             colors_[2];
		GpuBuffer             predicted_[2];
		GpuBuffer             cellData_;
		GpuBuffer             particleCell_;
		GpuBuffer             particleOffset_;
		GpuBuffer             blockSums_;
		GpuBuffer             lambdas_;
		GpuBuffer             omegas_;
		GpuBuffer             neighborCounts_;
		GpuBuffer             neighbors_;
		GpuBuffer             bodies_;
		GpuBuffer             bodyImpulses_;
		GpuBuffer             counters_;

		VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
		VkDescriptorPool      descriptorPool_ = VK_NULL_HANDLE;
		VkDescriptorSet       sets_[2][2] = {};                            ///< [state parity][predicted parity]

		Pipeline              predict_;
		Pipeline              scan_;
		Pipeline              scanAdd_;
		Pipeline              reorder_;
		Pipeline              lambda_;
		Pipeline              delta_;
		Pipeline              neighborSearch_;
		Pipeline              viscosity_;
		Pipeline              vorticity_;
		Pipeline              bodyStep_;

		static constexpr uint32_t QUERY_SLOTS = 4;                         ///< Frames whose timestamps can be in flight
		static constexpr uint32_t QUERIES_PER_SLOT = 64;
		VkQueryPool           queryPool_ = VK_NULL_HANDLE;
		bool                  isProfiling_ = false;
		uint32_t              querySlot_ = 0;
		uint32_t              queryCount_[QUERY_SLOTS] = {};               ///< Timestamps written in the frame of a slot
		uint8_t               queryStage_[QUERY_SLOTS][QUERIES_PER_SLOT] = {};   ///< Stage that ends at a timestamp
		FluidTimings          timings_;

		void computeConstants();
		void writeTimestamp( VkCommandBuffer commandBuffer, uint32_t stage );
		void createBuffers();
		void createPipelines();
		void writeParameters( VkCommandBuffer commandBuffer, float substepTime );
		void dispatch( VkCommandBuffer commandBuffer, const Pipeline& pipeline, uint32_t stateParity, uint32_t predictedParity,
					   uint32_t groups, const void* push = nullptr, uint32_t pushSize = 0 );
	};
}

#endif
