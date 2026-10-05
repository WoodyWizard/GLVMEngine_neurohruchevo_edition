// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  Fluid demo: a glass tank on a sunny terrace with five scenarios of the GPU liquid simulation.

  Controls: mouse - orbit the camera, W / S - zoom, A / D - turn, Space - next scenario, I - view mode
  (final, particles, depth, thickness, normals), O - pause, left button - drop water, Esc - quit.

  Options:
    --scenario N           start with scenario N (1..5)
    --frames N             quit after N frames (fixed 60 Hz steps, for screenshots and benchmarks)
    --screenshot F:PATH    save frame F as a PNG (repeatable)
    --mode N               view mode 0..4
    --camera Y,P,D         camera yaw, pitch (radians) and distance (meters)
    --spacing S            particle spacing in meters (default: 0.016 on discrete GPUs, 0.025 on integrated ones)
    --no-vsync             present without vertical sync
    --integrated           prefer an integrated GPU
    --help                 print the options
*/

#include "DemoRenderer.hpp"
#include "Fluid/FluidMeshes.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

using namespace GLVM::fluid;
using namespace GLVM::fluid::demo;

namespace
{
	/// Inner box of the glass tank, it is also the simulation domain (with room for splashes above the glass).
	const Vec3 TANK_MIN = { -0.8f, 0.0f, -0.4f };
	const Vec3 TANK_MAX = { 0.8f, 0.8f, 0.4f };
	constexpr float GLASS_THICKNESS = 0.012f;
	constexpr float FIXED_STEP = 1.0f / 60.0f;

	constexpr std::array<float, 3> POOL_WATER = { 0.45f, 0.8f, 1.0f };     ///< Dye: the colors the fluid lets through

	struct Options {
		int   scenario = 0;
		int   frames = 0;
		std::vector<std::pair<int, std::string>> screenshots;
		int   mode = 0;
		bool  hasCamera = false;
		float yaw = 0.0f, pitch = 0.0f, distance = 0.0f;
		float spacing = 0.0f;
		bool  isVsync = true;
		bool  preferIntegrated = false;
		bool  isHelp = false;
	};

	struct OrbitCamera {
		float yaw = 0.55f;
		float pitch = 0.4f;
		float distance = 2.25f;
		Vec3  target = { 0.0f, 0.22f, 0.0f };

		Vec3 eye() const {
			return target + Vec3{ std::sin( yaw ) * std::cos( pitch ), std::sin( pitch ), std::cos( yaw ) * std::cos( pitch ) } * distance;
		}
	};

	/// Scenario state that changes every frame.
	struct ScenarioState {
		bool      hasPaddle = false;
		uint32_t  paddleIndex = 0;
		FluidBody paddle;
		float     time = 0.0f;
	};

	FluidBody crate( std::array<float, 3> position, float halfSize, float density, float yaw ) {
		FluidBody body;
		body.type = BodyType::BOX;
		body.position = position;
		body.rotation = { 0.0f, std::sin( yaw * 0.5f ), 0.0f, std::cos( yaw * 0.5f ) };
		body.halfExtents = { halfSize, halfSize, halfSize };
		body.density = density;
		body.color = density > 1500.0f ? std::array<float, 4>{ 0.55f, 0.57f, 0.6f, 2.0f } : std::array<float, 4>{ 0.72f, 0.5f, 0.3f, 3.0f };
		return body;
	}

	FluidBody ball( std::array<float, 3> position, float radius, float density ) {
		FluidBody body;
		body.type = BodyType::SPHERE;
		body.position = position;
		body.halfExtents = { radius, radius, radius };
		body.density = density;
		body.color = density > 1500.0f ? std::array<float, 4>{ 0.75f, 0.75f, 0.78f, 2.0f } : std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 4.0f };
		return body;
	}

	struct Scenario {
		const char* name;
		const char* description;
		void (*setup)( FluidSimulation&, ScenarioState& );
	};

	const Scenario SCENARIOS[] = {
		{ "Dam break", "a water column collapses onto floating crates and a steel block",
		  []( FluidSimulation& simulation, ScenarioState& ) {
			  simulation.addBlock( { -0.8f, 0.0f, -0.4f }, { -0.28f, 0.58f, 0.4f }, { 0.0f, 0.0f, 0.0f }, POOL_WATER );
			  simulation.setBody( 0, crate( { 0.25f, 0.07f, -0.15f }, 0.07f, 550.0f, 0.3f ) );
			  simulation.setBody( 1, crate( { 0.45f, 0.07f, 0.18f }, 0.06f, 550.0f, -0.5f ) );
			  simulation.setBody( 2, crate( { 0.6f, 0.06f, -0.1f }, 0.055f, 2700.0f, 0.1f ) );
		  } },
		{ "Two dyes", "red and blue columns collide in the middle and mix",
		  []( FluidSimulation& simulation, ScenarioState& ) {
			  simulation.addBlock( { -0.8f, 0.0f, -0.4f }, { -0.44f, 0.62f, 0.4f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.22f, 0.3f } );
			  simulation.addBlock( { 0.44f, 0.0f, -0.4f }, { 0.8f, 0.62f, 0.4f }, { 0.0f, 0.0f, 0.0f }, { 0.15f, 0.5f, 1.0f } );
		  } },
		{ "Fountain", "a pump recirculates the pool through an orange dyed jet, a beach ball floats",
		  []( FluidSimulation& simulation, ScenarioState& ) {
			  simulation.addBlock( { -0.8f, 0.0f, -0.4f }, { 0.8f, 0.2f, 0.4f }, { 0.0f, 0.0f, 0.0f }, POOL_WATER );
			  FluidPump pump;
			  pump.isEnabled = true;
			  pump.drainMin = { 0.55f, -1.0f, -0.4f };
			  pump.drainMax = { 0.81f, 0.08f, 0.4f };
			  pump.nozzle = { -0.62f, 0.32f, 0.0f };
			  pump.nozzleRadius = 0.05f;
			  pump.velocity = { 1.55f, 2.6f, 0.0f };
			  pump.color = { 1.0f, 0.55f, 0.08f };
			  simulation.setPump( pump );
			  simulation.setBody( 0, ball( { 0.15f, 0.35f, 0.05f }, 0.09f, 90.0f ) );
			  simulation.setBody( 1, crate( { -0.3f, 0.3f, -0.2f }, 0.06f, 500.0f, 0.4f ) );
		  } },
		{ "Drop", "a green water ball and a steel ball fall into a pool",
		  []( FluidSimulation& simulation, ScenarioState& ) {
			  simulation.addBlock( { -0.8f, 0.0f, -0.4f }, { 0.8f, 0.2f, 0.4f }, { 0.0f, 0.0f, 0.0f }, POOL_WATER );
			  simulation.addSphere( { -0.25f, 0.62f, 0.0f }, 0.15f, { 0.0f, -1.0f, 0.0f }, { 0.35f, 1.0f, 0.45f } );
			  simulation.setBody( 0, ball( { 0.42f, 0.75f, 0.05f }, 0.07f, 3000.0f ) );
		  } },
		{ "Wave pool", "a paddle drives waves through floating crates",
		  []( FluidSimulation& simulation, ScenarioState& state ) {
			  simulation.addBlock( { -0.62f, 0.0f, -0.4f }, { 0.8f, 0.26f, 0.4f }, { 0.0f, 0.0f, 0.0f }, POOL_WATER );
			  state.hasPaddle = true;
			  state.paddleIndex = 0;
			  state.paddle.type = BodyType::BOX;
			  state.paddle.position = { -0.7f, 0.25f, 0.0f };
			  state.paddle.halfExtents = { 0.035f, 0.25f, 0.395f };
			  state.paddle.density = 0.0f;                               ///< Kinematic: moved by the demo
			  state.paddle.color = { 0.85f, 0.85f, 0.88f, 5.0f };
			  simulation.setBody( 0, state.paddle );
			  simulation.setBody( 1, crate( { 0.1f, 0.4f, -0.15f }, 0.06f, 500.0f, 0.2f ) );
			  simulation.setBody( 2, crate( { 0.35f, 0.4f, 0.15f }, 0.05f, 450.0f, 0.9f ) );
			  simulation.setBody( 3, ball( { 0.6f, 0.4f, -0.1f }, 0.08f, 90.0f ) );
		  } },
	};
	constexpr int SCENARIO_COUNT = (int)(sizeof(SCENARIOS) / sizeof(SCENARIOS[0]));

	const char* MODE_NAMES[] = { "final", "particles (speed)", "depth (smoothed)", "thickness", "normals" };

	bool parseOptions( int argc, char** argv, Options& options ) {
		for ( int i = 1; i < argc; ++i ) {
			const std::string argument = argv[i];
			const bool hasValue = i + 1 < argc;
			if ( argument == "--scenario" && hasValue ) {
				options.scenario = std::clamp( std::atoi( argv[++i] ) - 1, 0, SCENARIO_COUNT - 1 );
			} else if ( argument == "--frames" && hasValue ) {
				options.frames = std::atoi( argv[++i] );
			} else if ( argument == "--screenshot" && hasValue ) {
				const std::string value = argv[++i];
				const size_t colon = value.find( ':' );
				if ( colon == std::string::npos )
					return false;
				options.screenshots.push_back( { std::atoi( value.substr( 0, colon ).c_str() ), value.substr( colon + 1 ) } );
			} else if ( argument == "--mode" && hasValue ) {
				options.mode = std::clamp( std::atoi( argv[++i] ), 0, (int)FluidRenderMode::COUNT - 1 );
			} else if ( argument == "--camera" && hasValue ) {
				if ( std::sscanf( argv[++i], "%f,%f,%f", &options.yaw, &options.pitch, &options.distance ) != 3 )
					return false;
				options.hasCamera = true;
			} else if ( argument == "--spacing" && hasValue ) {
				options.spacing = (float)std::atof( argv[++i] );
			} else if ( argument == "--no-vsync" ) {
				options.isVsync = false;
			} else if ( argument == "--help" || argument == "-h" ) {
				options.isHelp = true;
			} else if ( argument == "--integrated" ) {
				options.preferIntegrated = true;
			} else {
				return false;
			}
		}
		return true;
	}

	/// Static scene: terrace floor, glass tank with a steel frame and a few props around it.
	std::vector<SceneObject> buildScene() {
		std::vector<SceneObject> objects;
		auto box = []( Vec3 center, Vec3 half, std::array<float, 4> color ) {
			SceneObject object;
			object.model = boxModel( center, half );
			std::memcpy( object.color, color.data(), sizeof(object.color) );
			return object;
		};

		SceneObject floor = box( { 0.0f, 0.0f, 0.0f }, { 30.0f, 1.0f, 30.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } );
		floor.mesh = MeshKind::FLOOR;
		floor.castsShadow = false;
		objects.push_back( floor );

		/// Glass: the open box around the inner tank, walls of GLASS_THICKNESS.
		const Vec3 center = (TANK_MIN + TANK_MAX) * 0.5f;
		const Vec3 half = (TANK_MAX - TANK_MIN) * 0.5f + Vec3{ GLASS_THICKNESS, 0.0f, GLASS_THICKNESS };
		SceneObject glass = box( center, half, { 1.0f, 1.0f, 1.0f, 1.0f } );
		glass.mesh = MeshKind::OPEN_BOX;
		glass.isGlass = true;
		glass.castsShadow = false;
		objects.push_back( glass );

		/// Steel frame: vertical posts at the corners, a top rim and a base.
		const std::array<float, 4> steel = { 0.32f, 0.34f, 0.37f, 2.0f };
		const float post = 0.018f;
		const float outerX = half.x + post, outerZ = half.z + post;
		for ( float sx : { -1.0f, 1.0f } )
			for ( float sz : { -1.0f, 1.0f } )
				objects.push_back( box( { center.x + sx * outerX, center.y + 0.02f, center.z + sz * outerZ }, { post, half.y + 0.02f, post }, steel ) );
		for ( float sz : { -1.0f, 1.0f } ) {
			objects.push_back( box( { center.x, TANK_MAX.y + 0.02f, center.z + sz * outerZ }, { outerX + post, 0.012f, post }, steel ) );
			objects.push_back( box( { center.x, 0.02f, center.z + sz * outerZ }, { outerX + post, 0.02f, post }, steel ) );
		}
		for ( float sx : { -1.0f, 1.0f } ) {
			objects.push_back( box( { center.x + sx * outerX, TANK_MAX.y + 0.02f, center.z }, { post, 0.012f, outerZ + post }, steel ) );
			objects.push_back( box( { center.x + sx * outerX, 0.02f, center.z }, { post, 0.02f, outerZ + post }, steel ) );
		}

		/// Props: a stack of crates and a steel drum stand-in.
		objects.push_back( box( { 1.45f, 0.16f, -0.55f }, { 0.16f, 0.16f, 0.16f }, { 0.7f, 0.5f, 0.3f, 3.0f } ) );
		objects.push_back( box( { 1.4f, 0.44f, -0.52f }, { 0.12f, 0.12f, 0.12f }, { 0.66f, 0.47f, 0.28f, 3.0f } ) );
		objects.push_back( box( { -1.5f, 0.25f, 0.7f }, { 0.2f, 0.25f, 0.2f }, { 0.15f, 0.35f, 0.6f, 5.0f } ) );
		return objects;
	}

	std::string format( const char* pattern, ... ) __attribute__((format(printf, 1, 2)));
	std::string format( const char* pattern, ... ) {
		char buffer[256];
		va_list arguments;
		va_start( arguments, pattern );
		std::vsnprintf( buffer, sizeof(buffer), pattern, arguments );
		va_end( arguments );
		return buffer;
	}

	int run( const Options& options ) {
		DemoWindow window;
		RendererOptions rendererOptions;
		rendererOptions.isVsync = options.isVsync && options.frames == 0;
		rendererOptions.preferIntegrated = options.preferIntegrated;
		DemoRenderer renderer( window, rendererOptions );
		const GpuContext& context = renderer.context();
		std::printf( "Device: %s\n", context.properties.deviceName );

		/// Quality preset by GPU class: integrated GPUs are memory bound, they get fewer particles and iterations.
		const bool isDiscrete = renderer.isDiscreteGpu();
		FluidSettings settings = isDiscrete ? FluidSettings() : FluidSettings::fast();
		settings.particleSpacing = options.spacing > 0.0f ? options.spacing : (isDiscrete ? 0.016f : 0.025f);
		settings.maxParticles = isDiscrete ? 131072u : 65536u;
		settings.domainMin = { TANK_MIN.x, TANK_MIN.y, TANK_MIN.z };
		settings.domainMax = { TANK_MAX.x, TANK_MAX.y + 0.6f, TANK_MAX.z };
		FluidSimulation simulation( context, settings );
		simulation.setProfiling( true );
		renderer.setSimulation( &simulation );
		std::printf( "Particle spacing %.3f m, %u substeps x %u iterations\n", settings.particleSpacing, settings.substeps, settings.iterations );

		const std::vector<SceneObject> sceneObjects = buildScene();
		OrbitCamera camera;
		if ( options.hasCamera ) {
			camera.yaw = options.yaw;
			camera.pitch = options.pitch;
			camera.distance = options.distance;
		}
		int scenario = options.scenario;
		int mode = options.mode;
		bool isPaused = false;
		ScenarioState state;
		uint32_t dropSeed = 12345u;

		auto startScenario = [&]( int index ) {
			renderer.waitIdle();
			simulation.reset();
			state = ScenarioState();
			SCENARIOS[index].setup( simulation, state );
			std::printf( "Scenario %d: %s, %u particles\n", index + 1, SCENARIOS[index].name, simulation.particleCount() );
			std::fflush( stdout );
		};
		startScenario( scenario );

		using Clock = std::chrono::steady_clock;
		Clock::time_point previous = Clock::now();
		Clock::time_point fpsStart = previous;
		int fpsFrames = 0;
		float fps = 0.0f, frameMilliseconds = 0.0f;
		FluidTimings simulationTimings;
		float elapsed = 0.0f;
		float accumulatedTime = 0.0f;                                      ///< Frame time not simulated yet

		for ( int frameNumber = 0; options.frames == 0 || frameNumber < options.frames; ++frameNumber ) {
			const DemoInput input = window.poll();
			if ( input.quit )
				break;
			const Clock::time_point now = Clock::now();
			const float realStep = std::chrono::duration<float>( now - previous ).count();
			previous = now;
			const float step = options.frames > 0 ? FIXED_STEP : std::min( realStep, 1.0f / 30.0f );
			elapsed += step;

			++fpsFrames;
			const float fpsWindow = std::chrono::duration<float>( now - fpsStart ).count();
			if ( fpsWindow >= 0.5f ) {
				fps = fpsFrames / fpsWindow;
				frameMilliseconds = 1000.0f * fpsWindow / fpsFrames;
				fpsFrames = 0;
				fpsStart = now;
				simulationTimings = simulation.timings();
			}

			/// Input.
			if ( input.nextScenario ) {
				scenario = (scenario + 1) % SCENARIO_COUNT;
				startScenario( scenario );
			}
			if ( input.nextMode )
				mode = (mode + 1) % (int)FluidRenderMode::COUNT;
			if ( input.togglePause )
				isPaused = !isPaused;
			camera.yaw -= input.mouseDeltaX * 0.004f;
			camera.pitch = std::clamp( camera.pitch + input.mouseDeltaY * 0.004f, 0.05f, 1.45f );
			if ( input.forward )  camera.distance = std::max( camera.distance * (1.0f - 1.5f * step), 0.7f );
			if ( input.backward ) camera.distance = std::min( camera.distance * (1.0f + 1.5f * step), 7.0f );
			if ( input.left )     camera.yaw += 1.2f * step;
			if ( input.right )    camera.yaw -= 1.2f * step;
			if ( input.click ) {
				/// A water ball above a random point of the tank.
				dropSeed = dropSeed * 1664525u + 1013904223u;
				const float x = TANK_MIN.x + 0.2f + (TANK_MAX.x - TANK_MIN.x - 0.4f) * (float)(dropSeed >> 8) / 16777216.0f;
				const float radius = 0.08f;
				const float volume = 4.18879f * radius * radius * radius;
				const float spacing = settings.particleSpacing;
				if ( simulation.particleCount() + (uint32_t)(volume / (spacing * spacing * spacing)) + 64 < settings.maxParticles ) {
					renderer.waitIdle();
					simulation.addSphere( { x, TANK_MAX.y - 0.05f, 0.0f }, radius, { 0.0f, -1.5f, 0.0f }, POOL_WATER );
				}
			}

			/// Frame description.
			DemoFrame frame;
			frame.eye = camera.eye();
			frame.target = camera.target;
			frame.mode = (FluidRenderMode)mode;
			frame.light.sunDirection = normalize( { -0.5f, 0.62f, -0.42f } );
			frame.light.sunIntensity = 1.7f;
			frame.light.sunColor = { 1.0f, 0.93f, 0.82f };
			frame.light.skyZenith = { 0.16f, 0.36f, 0.78f };
			frame.light.skyHorizon = { 0.62f, 0.74f, 0.88f };
			frame.light.groundColor = { 0.3f, 0.28f, 0.25f };
			frame.appearance.dyeAbsorption = 16.0f;
			frame.tankMin = TANK_MIN;
			frame.tankMax = TANK_MAX;
			frame.objects = sceneObjects;
			frame.time = elapsed;
			for ( const auto& screenshot : options.screenshots )
				if ( screenshot.first == frameNumber )
					frame.screenshotPath = screenshot.second;

			const GpuTimings& gpu = renderer.timings();
			const float white[4] = { 1.0f, 1.0f, 1.0f, 0.95f };
			const float dim[4] = { 0.85f, 0.9f, 1.0f, 0.8f };
			auto line = [&]( float y, const std::string& text, const float* color, float scale = 2.0f ) {
				TextLine textLine;
				textLine.text = text;
				textLine.x = 16.0f;
				textLine.y = y;
				textLine.scale = scale;
				std::memcpy( textLine.color, color, sizeof(textLine.color) );
				frame.text.push_back( textLine );
			};
			line( 14.0f, format( "Scenario %d/%d: %s%s", scenario + 1, SCENARIO_COUNT, SCENARIOS[scenario].name, isPaused ? " (paused)" : "" ), white, 3.0f );
			line( 52.0f, SCENARIOS[scenario].description, dim );
			line( 84.0f, format( "%u particles, spacing %.0f mm, %u x %u iterations, %s", simulation.particleCount(),
								 settings.particleSpacing * 1000.0f, settings.substeps, settings.iterations, context.properties.deviceName ), dim );
			line( 110.0f, format( "FPS %.0f (%.1f ms)  GPU %.1f ms: simulation %.1f, scene %.1f, fluid %.1f", fps, frameMilliseconds,
								  gpu.total, gpu.simulation, gpu.scene, gpu.fluid ), dim );
			line( 136.0f, format( "simulation: sort %.2f, neighbors %.2f, solver %.2f, viscosity+vorticity %.2f, bodies %.2f",
								  simulationTimings.sort, simulationTimings.neighborSearch, simulationTimings.solver,
								  simulationTimings.velocity, simulationTimings.bodies ), dim );
			line( 162.0f, format( "View: %s", MODE_NAMES[mode] ), dim );
			line( (float)renderer.height() - 36.0f, "Mouse, W S A D: camera  Space: scenario  I: view  O: pause  Click: drop water  Esc: quit", dim );

			/*
			  Simulation commands at the beginning of the frame, in fixed steps (the frame time is accumulated, at most two
			  steps per frame): PBF velocities are position corrections / dt, short frames would make the water jump.
			  The paddle moves before every step.
			*/
			const bool isRunning = !isPaused;
			renderer.renderFrame( frame, [&]( VkCommandBuffer commandBuffer ) {
				if ( !isRunning )
					return;
				accumulatedTime = std::min( accumulatedTime + step, 2.0f * FIXED_STEP );
				for ( ; accumulatedTime >= FIXED_STEP; accumulatedTime -= FIXED_STEP ) {
					state.time += FIXED_STEP;
					if ( state.hasPaddle ) {
						const float omega = 6.2831853f / 1.7f;
						const float amplitude = 0.1f;
						const float ramp = std::min( state.time / 1.5f, 1.0f );
						state.paddle.position[0] = -0.7f + amplitude * ramp * std::sin( omega * state.time );
						state.paddle.velocity[0] = amplitude * ramp * omega * std::cos( omega * state.time );
						simulation.recordBodyUpdate( commandBuffer, state.paddleIndex, state.paddle );
					}
					simulation.recordStep( commandBuffer, FIXED_STEP );
				}
			});
		}
		renderer.waitIdle();
		return 0;
	}
}

int main( int argc, char** argv ) {
	Options options;
	const bool isValid = parseOptions( argc, argv, options );
	if ( !isValid || options.isHelp ) {
		std::printf( "Usage: fluidDemo [--scenario N] [--frames N] [--screenshot FRAME:PATH] [--mode N] [--camera YAW,PITCH,DISTANCE]\n"
					 "                 [--spacing METERS] [--no-vsync] [--integrated]\n"
					 "Controls: mouse, W S A D - camera, Space - next scenario, I - view mode, O - pause,\n"
					 "          left button - drop water, Esc - quit\n" );
		return isValid ? 0 : 1;
	}
	try {
		return run( options );
	} catch ( const std::exception& error ) {
		std::fprintf( stderr, "fluidDemo: %s\n", error.what() );
		return 1;
	}
}
