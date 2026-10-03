// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "ArchetypeECS/ArchECS_Utils.hpp"
#include "ArchetypeECS/ArchECS_World.hpp"
#include "Archetypes/CrosshairArchetype.hpp"
#include "Archetypes/InventoryArchetype.hpp"
#include "Archetypes/LevelChunkArchetype.hpp"
#include "Archetypes/MathObjectaArchetype.hpp"
#include "Archetypes/RigidBodyArchetype.hpp"
#include "Archetypes/SpotLightArchetype.hpp"
#include "Archetypes/StaticMeshArchetype.hpp"
#include "Components/AnimationMoveComponent.hpp"
#include "Components/ColliderComponent.hpp"
#include "Components/CrosshairComponent.hpp"
#include "Components/EnemyComponent.hpp"
#include "Components/FontComponent.hpp"
#include "Components/HealthComponent.hpp"
#include "Components/InterfaceComponent.hpp"
#include "Components/InterfaceComponent.hpp"
#include "Components/InventoryComponent.hpp"
#include "Components/InventorySlotComponent.hpp"
#include "Components/MaterialComponent.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/VertexComponent.hpp"
#include "Components/ItemComponent.hpp"
#include "Components/ActorComponent.hpp"
#include "Engine.hpp"
#include "TextureFormatStructs.hpp"
#include "TextureLoadingUtils.hpp"
#include "Vector.hpp"
#ifdef __linux__
#include "Network/UDP_ServerLinux.hpp"
#endif
#include "SpritesData.hpp"
#include "Texture.hpp"
#include "VertexMath.hpp"
#include "PGA.hpp"

#include <cstdio>
#include <filesystem>
#include <map>
#include <random>
#ifdef _WIN32
#include <windows.h>
#endif

int main()
{
	/// Resources are referenced as "../<dir>/..." relative to the build directory, so work from the executable's directory
	[]() {
		std::error_code error;
#ifdef _WIN32
		wchar_t executablePath[MAX_PATH];
		const DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
		if ( length == 0 || length == MAX_PATH )
			return;
		const std::filesystem::path executable(executablePath);
#else
		const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
		if ( error )
			return;
#endif
		std::filesystem::current_path(executable.parent_path(), error);
		if ( error )
			std::cerr << "Warning: can't change working directory to " << executable.parent_path() << ": " << error.message() << std::endl;
	}();

	// GLVM::core::pga::plane plane;
	// GLVM::core::pga::point point = !plane;
	// std::cout << point.w << std::endl;
	// auto a = !plane;
	// std::cout << typeid(a).name() << std::endl;
	// std::cout << point.y << std::endl;
	// std::cout << !plane.x << std::endl;

	using namespace GLVM;
	namespace cm   = GLVM::ecs::components;
	namespace pga  = GLVM::core::pga;
	namespace arch = GLVM::ecs::arch;

	[[maybe_unused]] pga::plane plane0 = { 2.1f, 3.5f, 4.2f, 3.87f };
	[[maybe_unused]] pga::plane plane1 = { 3.17f, 10.20f, 7.832f, 3.87f };
	[[maybe_unused]] pga::point point0 = { 1.5f, 2.77f, 6.55f, 8.99f };
	[[maybe_unused]] pga::point point1 = { 3.577f, 0.787f, 16.575f, 888.99f };
	[[maybe_unused]] pga::line line0 = { 1.87, 2.053, 6.234, 10.34, 3234.32, 223.43 };
	[[maybe_unused]] pga::line line1 = { 5.723, 10.234, 3.343, 0.344, 234.123, 77.345 };
	[[maybe_unused]] pga::rline rline0 = { 21.87, 25.053, 63.234 };
	[[maybe_unused]] pga::rline rline1 = { 15.723, 510.234, 73.343 };
	
//	std::cout << (line0 ^ plane0) << std::endl;
	std::cout << (point0 * point1) << std::endl;

	
	ecs::EntityManager   * EntityManager     = ecs::EntityManager::GetInstance();
	ecs::ComponentManager* ComponentManager  = ecs::ComponentManager::GetInstance();

	arch::ArchetypeEntityManager* archEntityManager = arch::ArchetypeEntityManager::getInstance();
	
	core::Engine* GLVM = core::Engine::GetInstance();

	[[maybe_unused]] cm::MeshHandle cubeHandle_OBJ = GLVM->LoadMeshFromFile_OBJ("../waveFrontObj/cube.obj");
	[[maybe_unused]] cm::MeshHandle icoSphereHandle_OBJ = GLVM->LoadMeshFromFile_OBJ("../waveFrontObj/ico_sphere.obj");
	[[maybe_unused]] cm::MeshHandle monkeyHandle_OBJ = GLVM->LoadMeshFromFile_OBJ("../waveFrontObj/suzana.obj");
//	[[maybe_unused]] cm::MeshHandle coneHandle_OBJ = GLVM->LoadMeshFromFile_OBJ("../waveFrontObj/cone.obj");
//	[[maybe_unused]] cm::MeshHandle monkeyHandle_OBJ = GLVM->LoadMeshFromFile_OBJ("../waveFrontObj/suzana2.obj");
//	[[maybe_unused]] cm::MeshHandle uvSphereHandle_OBJ = GLVM->LoadMeshFromFile_OBJ("../waveFrontObj/uv_sphere.obj");
//	[[maybe_unused]] cm::MeshHandle torusHandle_OBJ = GLVM->LoadMeshFromFile_OBJ("../waveFrontObj/torus.obj");
//	[[maybe_unused]] cm::MeshHandle pipeHandle_OBJ = GLVM->LoadMeshFromFile_OBJ("../waveFrontObj/pipe.obj");
	[[maybe_unused]] cm::MeshHandle hyperCubeHandle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/hyper_cube.gltf");
	[[maybe_unused]] cm::MeshHandle hyperCubeHandle2_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/hyper_cube2.gltf");
	[[maybe_unused]] cm::MeshHandle megaChelHandle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/mega_chel.gltf");
	[[maybe_unused]] cm::MeshHandle simpleCubeHandle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/simpleCube2.gltf");
	[[maybe_unused]] cm::MeshHandle crosshair_001_Handle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/crosshair_001.gltf");
	[[maybe_unused]] cm::MeshHandle inventory_Handle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/inventory.gltf");
	[[maybe_unused]] cm::MeshHandle cyborg_Handle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/cyborg11.gltf");
//	[[maybe_unused]] cm::MeshHandle robot0_Handle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/robot3.gltf");
	[[maybe_unused]] cm::MeshHandle robot0_Handle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/scene.gltf");
	[[maybe_unused]] cm::MeshHandle tree_Handle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/tree.gltf");
	[[maybe_unused]] cm::MeshHandle debugVector_Handle_GLTF = GLVM->LoadMeshFromFile_GLTF("../gltf/debugVector.gltf");
	
	[[maybe_unused]] ecs::TextureHandle chelikTextureHandle = GLVM->LoadTextureFromAddress(128, 96, chelik_dat_len, chelik_dat);
	[[maybe_unused]] ecs::TextureHandle witchTexturehandle = GLVM->LoadTextureFromAddress(32, 32, witch_dat_len, witch_dat);
	[[maybe_unused]] ecs::TextureHandle grayTextureHandle = GLVM->LoadTextureFromAddress(32, 32, gray_dat_len, gray_dat);
	// core::DDSData grayTextureDDS = core::loadDDS( "../textures/data/gray_texture_bc7.dds" );
	// const u32 grayTextureDataLength = grayTextureDDS.height * grayTextureDDS.width;
	// [[maybe_unused]] ecs::TextureHandle grayTextureHandle = GLVM->LoadTextureFromAddress(grayTextureDDS.width, grayTextureDDS.height,
	// 																					 grayTextureDataLength, (unsigned char*)grayTextureDDS.data.data());
	[[maybe_unused]] ecs::TextureHandle container2Texturehandle = GLVM->LoadTextureFromAddress(500, 500, container2_dat_len, container2_dat);
	[[maybe_unused]] ecs::TextureHandle container2SpecularTextureHandle = GLVM->LoadTextureFromAddress(500, 500, container2_specular_dat_len, container2_specular_dat);
	[[maybe_unused]] ecs::TextureHandle crosshairTexturehandle = GLVM->LoadTextureFromAddress(32, 32, Crosshair_dat_len, Crosshair_dat);
	[[maybe_unused]] ecs::TextureHandle fontAtlasTexturehandle = GLVM->LoadTextureFromAddress(84, 132, fontAtlas_dat_len, fontAtlas_dat);
	[[maybe_unused]] ecs::TextureHandle inventoryTexturehandle = GLVM->LoadTextureFromAddress(64, 64, inventorySlot_dat_len, inventorySlot_dat);
	[[maybe_unused]] ecs::TextureHandle tilesetTexturehandle = GLVM->LoadTextureFromAddress(512, 512, tileset_dat_len, tileset_dat);

	{
		arch::LevelChunkArchetype* levelChunkArch = new arch::LevelChunkArchetype;
		arch::PlayerArchetype* playerArch         = new arch::PlayerArchetype;
		arch::EnemyArchetype* enemyArch           = new arch::EnemyArchetype;
		arch::ProjectileArchetype* projectileArch = new arch::ProjectileArchetype;
		arch::StaticMeshArchetype* staticMeshArch = new arch::StaticMeshArchetype;
		arch::CrosshairArchetype* crosshairArch   = new arch::CrosshairArchetype;
		arch::InventoryArchetype* inventoryArch   = new arch::InventoryArchetype;
		arch::ItemArchetype*      itemArch        = new arch::ItemArchetype;
		arch::DirectionalLightArchetype* directionalLightArch = new arch::DirectionalLightArchetype;
		arch::PointLightArchetype*       pointLightArch       = new arch::PointLightArchetype;
		arch::SpotLightArchetype*        spotLightArch        = new arch::SpotLightArchetype;
		arch::MathObjectArchetype*       mathObjectsArch      = new arch::MathObjectArchetype;
		
		arch::world.archetypes.Push( levelChunkArch );
		arch::world.archetypes.Push( playerArch );
		arch::world.archetypes.Push( enemyArch );
		arch::world.archetypes.Push( projectileArch );
		arch::world.archetypes.Push( staticMeshArch );
		arch::world.archetypes.Push( crosshairArch );
		arch::world.archetypes.Push( inventoryArch );
		arch::world.archetypes.Push( itemArch );
		arch::world.archetypes.Push( directionalLightArch );
		arch::world.archetypes.Push( pointLightArch );
		arch::world.archetypes.Push(spotLightArch);
		arch::world.archetypes.Push( mathObjectsArch );
	}

	/// Loading method with stb_image
	// [[maybe_unused]] ecs::TextureHandle chelikTextureHandle = GLVM->LoadTextureFromFile("../textures/chelik.h");
	// [[maybe_unused]] ecs::TextureHandle witchTexturehandle = GLVM->LoadTextureFromFile("../textures/witch.h");
	// [[maybe_unused]] ecs::TextureHandle grayTextureHandle = GLVM->LoadTextureFromFile("../textures/data/gray.png");
	// [[maybe_unused]] ecs::TextureHandle container2Texturehandle = GLVM->LoadTextureFromFile("../textures/data/container2.png");
	// [[maybe_unused]] ecs::TextureHandle container2SpecularTextureHandle = GLVM->LoadTextureFromFile("../textures/data/container2_specular.png");

	// while(1) {
	// 	core::UDP_ServerLinux serverLinux;
	// 	char* buffer = serverLinux.receive();
	// 	printf("Message from client: %s\n", buffer);
	// 	// for ( int i = 0; i < 1024; ++i ) {
	// 	// 	if ( buffer[i] == '\n' )
	// 	// 		break;

			
	// 	// }

	// 	serverLinux.response();
	// }
	
	arch::entity player = archEntityManager->createEntity();
//	std::cout << "player: " << ecs::arch::getId(player) << std::endl;
	arch::world.addEntityToArchetype( player, arch::world.archetypes[1] );
	arch::EntityLocation playerLocation = arch::world.entityLocations[arch::getId( player )];
	arch::PlayerArchetype* playerArch = static_cast<arch::PlayerArchetype*>(playerLocation.arch);
	const uint32_t playerIndex = playerLocation.index;

	playerArch->transforms[playerIndex]  = { .position = { 3.0f, 5.0f, 0.0f }, .forward = {}, .pitch = 3.14, .scale = 1.0f };
	std::cout << "new player address: " << &playerArch->transforms[playerIndex].position << std::endl;
	playerArch->rigidBodies[playerIndex] = { .fMass_ = 3.0f };
	playerArch->health[playerIndex]      = { .maxHealth = 100, .currentHealth = 100 };
	playerArch->beholders[playerIndex]   = {  .Position = {0.0f, 5.0f, 1.0f}, .forward = { 0.0f, 0.0f, -1.0f } };
	playerArch->meshes[playerIndex]      = { .handle = megaChelHandle_GLTF, .gltf = true };
	playerArch->materials[playerIndex]   = { .diffuseTextureID_ = grayTextureHandle, .specularTextureID_ = grayTextureHandle,
		.ambient = { 0.05f, 0.05f, 0.0f }, .shininess = 128.0f * 0.078125f }; 
	
    std::random_device rd;
    std::map<int, int> hist;
	std::mt19937 mersenne(rd());
    std::uniform_int_distribution<int> dist(0, 3);
	
	for ( u32 i = 0; i < 5; ++i ) {
	arch::entity enemy = archEntityManager->createEntity();
	arch::world.addEntityToArchetype( enemy, arch::world.archetypes[2] );
	arch::EntityLocation enemyLocation = arch::world.entityLocations[arch::getId( enemy )];
	arch::EnemyArchetype* enemyArch = static_cast<arch::EnemyArchetype*>(enemyLocation.arch);
	const uint32_t enemyIndex = enemyLocation.index;

	unsigned int random = dist(mersenne);
	vec3 randomDirection = {};
	switch( random ) {
	case 0:
		randomDirection = vec3( 3.0f, 0.0f, 0.0f );
		break;
	case 1:
		randomDirection = vec3( -3.0f, 0.0f, 0.0f );
		break;
	case 2:
		randomDirection = vec3( 0.0f, 0.0f, 3.0f );
		break;
	case 3:
		randomDirection = vec3( 0.0f, 0.0f, -3.0f );
		break;
	}

	enemyArch->transforms[enemyIndex]  = { .position = { vec3( (float)i * 20, 3.3f, -25.0f ) + randomDirection }, .scale = 0.05f };
	enemyArch->states[enemyIndex]      = { .state = core::States::ROAMING };
	enemyArch->rigidBodies[enemyIndex] = { .fMass_ = 0.0f };
	enemyArch->enemies[enemyIndex]     = { .detectRadius = 15.0f };
	enemyArch->health[enemyIndex]      = { .maxHealth = 100, .currentHealth = 100 };
	cm::font* enemyFontComponent       = &enemyArch->fonts[enemyIndex];
	
	if ( i < 3 ) {
		enemyFontComponent->font_string.Push('1');
	} else if ( i < 6 ) {
		enemyFontComponent->font_string.Push('1');
		enemyFontComponent->font_string.Push('0');
	} else if ( i < 10 ) {
		enemyFontComponent->font_string.Push('1');
		enemyFontComponent->font_string.Push('0');
		enemyFontComponent->font_string.Push('E');
	} else {
		enemyFontComponent->font_string.Push('J');
		enemyFontComponent->font_string.Push('r');
	}
	enemyFontComponent->lifeTime     = 0.0f;
	enemyFontComponent->removeble    = false;
	enemyArch->meshes[enemyIndex]    = { .handle = robot0_Handle_GLTF, .gltf = true };
	enemyArch->materials[enemyIndex] = { .diffuseTextureID_ = grayTextureHandle, .specularTextureID_ = grayTextureHandle,
		.ambient = { 0.05f, 0.05f, 0.05f }, .shininess = 32.0f * 0.078125f };
	}

	for( u32 i = 0; i < 5; ++i ) {
		unsigned int random = dist(mersenne);
		arch::entity cube = archEntityManager->createEntity();
		std::cout << "Cube: " << arch::getId( cube ) << std::endl;
		arch::world.addEntityToArchetype( cube, arch::world.archetypes[4] );
		arch::EntityLocation cubeLocation = arch::world.entityLocations[arch::getId( cube )];
		arch::StaticMeshArchetype* cubeArch = static_cast<arch::StaticMeshArchetype*>(cubeLocation.arch);
		const uint32_t cubeIndex = cubeLocation.index;
		cubeArch->transforms[cubeIndex] = { .position = { -8.0f + i * 6.0f, 2.0f, 15.0f + random * 2 }, .scale = 3.0f };
		cubeArch->meshes[cubeIndex]     = { .handle = hyperCubeHandle2_GLTF, .gltf = true };
		cubeArch->materials[cubeIndex]  = { .diffuseTextureID_ = tilesetTexturehandle, .specularTextureID_ = container2SpecularTextureHandle,
			.ambient = { 0.05f, 0.05f, 0.05f }, .shininess = 128.0f * 0.078125f };
//		cubeArch->fonts[cubeIndex].font_string.Push(' ');
	}
	
	arch::entity crosshair = archEntityManager->createEntity();
	arch::world.addEntityToArchetype( crosshair, arch::world.archetypes[5] );
	arch::EntityLocation crosshairLocation = arch::world.entityLocations[arch::getId( crosshair )];
	arch::CrosshairArchetype* crosshairArch = static_cast<arch::CrosshairArchetype*>(crosshairLocation.arch);
	const uint32_t crosshairIndex = crosshairLocation.index;
	crosshairArch->transforms[crosshairIndex]    = { .scale = 0.01f };
	crosshairArch->meshes[crosshairIndex].handle = crosshair_001_Handle_GLTF;
	crosshairArch->materials[crosshairIndex]     = { .diffuseTextureID_ = container2Texturehandle,
		.specularTextureID_ = container2SpecularTextureHandle, .ambient = { 0.05f, 0.05f, 0.05f }, .shininess = 128.0f * 0.078125f };
	
	arch::entity inventory = archEntityManager->createEntity();
	arch::world.addEntityToArchetype( inventory, arch::world.archetypes[6] );
	arch::EntityLocation inventoryLocation = arch::world.entityLocations[arch::getId( inventory )];
	arch::InventoryArchetype* inventoryArch = static_cast<arch::InventoryArchetype*>(inventoryLocation.arch);
	const uint32_t inventoryIndex = inventoryLocation.index;
	cm::inventory* inventoryComponent = &inventoryArch->invetories[inventoryIndex];
	inventoryComponent->entityOwner = player;
	inventoryComponent->slotMeshID  = inventory_Handle_GLTF;
	inventoryComponent->slotScale   = 0.05;
	cm::mesh* inventoryMesh = &inventoryArch->meshes[inventoryIndex];
	inventoryMesh->gltf = true;
	inventoryArch->transforms[inventoryIndex] = { .position = { 0.0f, -0.5f, 0.0f }, .scale = 1.0f };
	inventoryArch->materials[inventoryIndex]  = { .diffuseTextureID_ = inventoryTexturehandle, .specularTextureID_ = inventoryTexturehandle,
		.ambient = { 0.05f, 0.05f, 0.05f }, .shininess = 128.0f * 0.078125f };

	
	for ( unsigned int i = 0; i < 5; ++i ) {
		arch::entity item = archEntityManager->createEntity();
//		std::cout << "item: " << ecs::arch::getId(item) << " i: " << i << std::endl;
		arch::world.addEntityToArchetype( item, arch::world.archetypes[7] );
		arch::EntityLocation itemLocation = arch::world.entityLocations[arch::getId( item )];
		arch::ItemArchetype* itemArch = static_cast<arch::ItemArchetype*>(itemLocation.arch);
		const uint32_t itemIndex = itemLocation.index;
		unsigned int row = i + 1;
		itemArch->items[itemIndex].itemSlotType = { 2, row };
		itemArch->items[itemIndex].isActor      = true;
		itemArch->transforms[itemIndex]         = { .position = { 3.0f, 5.0f, 10.0f + i * 2.0f }, .scale = 0.05f };
		itemArch->rigidBodies[itemIndex]        = { .fMass_ = 1.0f };
		if ( i % 2 == 0 )
			itemArch->meshes[itemIndex].handle = hyperCubeHandle_GLTF;
		else
			itemArch->meshes[itemIndex].handle = hyperCubeHandle_GLTF;

		itemArch->materials[itemIndex] = { .diffuseTextureID_ = container2Texturehandle, .specularTextureID_ = container2SpecularTextureHandle,
			.ambient = { 0.05f, 0.05f, 0.05f }, .shininess = 128.0f * 0.078125f };
	}

	arch::entity directionalLight = archEntityManager->createEntity();
	arch::world.addEntityToArchetype( directionalLight, arch::world.archetypes[8] );
	arch::EntityLocation directionalLightLocation = arch::world.entityLocations[arch::getId( directionalLight )];
	arch::DirectionalLightArchetype* directionalLightArch = static_cast<arch::DirectionalLightArchetype*>(directionalLightLocation.arch);
	const uint32_t directionalLightIndex = directionalLightLocation.index;
	directionalLightArch->directionalLights[directionalLightIndex] = { .position = { 0.0f, 20.0f, 15.0f },
		.direction = { 5.0f, -20.0f, -8.0f}, .ambient = { 0.05f, 0.05f, 0.05f }, .diffuse = {0.4f, 0.4f, 0.4f},  ///< Towards (5, 0, 7)
		.specular = {0.1f, 0.1f, 0.1f}};
	directionalLightArch->transforms[directionalLightIndex]    = { .position = { 0.0f, 20.0f, 15.0f }, .scale = 0.1f };
	directionalLightArch->meshes[directionalLightIndex].handle = hyperCubeHandle_GLTF;
	directionalLightArch->materials[directionalLightIndex] = { .diffuseTextureID_ = container2Texturehandle, .specularTextureID_ = container2Texturehandle,
		.ambient = { 0.05f, 0.05f, 0.05f }, .shininess = 128.0f * 0.078125f };
	
	
	arch::entity pointLight = archEntityManager->createEntity();
	arch::world.addEntityToArchetype( pointLight, arch::world.archetypes[9] );
	arch::EntityLocation pointLightLocation = arch::world.entityLocations[arch::getId( pointLight )];
	arch::PointLightArchetype* pointLightArch = static_cast<arch::PointLightArchetype*>(pointLightLocation.arch);
	const uint32_t pointLightIndex = pointLightLocation.index;
	pointLightArch->pointLights[pointLightIndex]   = { .position = { 3.0f, 20.0f, 0.0f },
		.ambient = { 0.1f, 0.1f, 0.1f }, .diffuse  = { 0.8f, 0.8f, 0.8f }, .specular = { 2.0f, 2.0f, 2.0f },
		.constant = 1.0f, .linear = 0.09f, .quadratic = 0.0032f };
	pointLightArch->transforms[pointLightIndex]    = { .position = { 3.0f, 20.0f, 0.0f }, .scale = 0.2f };
	pointLightArch->meshes[pointLightIndex].handle = hyperCubeHandle_GLTF;
	pointLightArch->materials[pointLightIndex]     = { .diffuseTextureID_ = container2Texturehandle, .specularTextureID_ = container2Texturehandle,
		.ambient = { 0.05f, 0.05f, 0.0f }, .shininess = 128.0f * 0.078125f };
	
	
	arch::entity spotLight = archEntityManager->createEntity();
	arch::world.addEntityToArchetype( spotLight, arch::world.archetypes[10] );
	arch::EntityLocation spotLightLocation = arch::world.entityLocations[arch::getId( spotLight )];
	arch::SpotLightArchetype* spotLightArch = static_cast<arch::SpotLightArchetype*>(spotLightLocation.arch);
	const uint32_t spotLightIndex = spotLightLocation.index;
	spotLightArch->spotLights[spotLightIndex]    = { .position = { 1.0f, 17.0f, -5.0f },
		.direction = { -1.0f, -18.0f, 8.0f }, .cutOff = 32.5f, .outerCutOff = 37.5f, .ambient = { 0.05f, 0.05f, 0.05f },  ///< Towards (0, -1, 3)
		.diffuse = { 3.8f, 3.8f, 3.8f }, .specular = { 5.0f, 5.0f, 5.0f }, .constant = 1.0f, .linear = 0.09f,
		.quadratic = 0.032f };
	spotLightArch->transforms[spotLightIndex]    = { .position = { 1.0f, 17.0f, -5.0f }, .scale = 0.2f };
	spotLightArch->meshes[spotLightIndex].handle = simpleCubeHandle_GLTF;
	spotLightArch->materials[spotLightIndex]     = { .diffuseTextureID_ = grayTextureHandle, .specularTextureID_ = grayTextureHandle };

	arch::entity vector = archEntityManager->createEntity();
	arch::world.addEntityToArchetype( vector, arch::world.archetypes[11] );
	arch::EntityLocation vectorLocation = arch::world.entityLocations[arch::getId( vector )];
	arch::MathObjectArchetype* vectorArch = static_cast<arch::MathObjectArchetype*>(vectorLocation.arch);
	const uint32_t vectorIndex = vectorLocation.index;

	vectorArch->transforms[vectorIndex]    = { .position = { 0.0f, 0.0f, 0.0f }, .forward = { 0.0, 0.0, 1.0 }, .yaw = 0.0, .pitch = 0.0, .scale = 1.0f };
	core::vector<vec3>& vectorVertices =  vectorArch->generatedMesh[vectorIndex].vertices;
	/// Vector
	// vectorVertices.Push( vec3( 1.0, 1.0, 0.0) );
	// vectorVertices.Push( vec3( 2.0, 4.0, 0.0) );

	/// Triangle
	// vectorVertices.Push( vec3( -1.0, 0.0, 0.0) );
	// vectorVertices.Push( vec3( 1.0, 0.0, 0.0) );
	// vectorVertices.Push( vec3( 0.0, 3.0, 0.0) );

	/// Plane
	// vectorVertices.Push( vec3( -1.0, -1.0, -1.0) );
	// vectorVertices.Push( vec3( 1.0, -1.0, -1.0) );
	// vectorVertices.Push( vec3( 1.0, 1.0, -1.0) );
	// vectorVertices.Push( vec3( -1.0, 1.0, -1.0) );

	/// Frustum Cube
	// vectorVertices.Push( vec3( -1.0, 1.0, 0.0) );
	// vectorVertices.Push( vec3( 1.0, 1.0, 0.0) );
	// vectorVertices.Push( vec3( 1.0, -1.0, 0.0) );
	// vectorVertices.Push( vec3( -1.0, -1.0, 0.0) );

	// vectorVertices.Push( vec3( 1.0, -1.0, 1.0) );
	// vectorVertices.Push( vec3( -1.0, -1.0, 1.0) );
	// vectorVertices.Push( vec3( -1.0, 1.0, 1.0) );
	// vectorVertices.Push( vec3( 1.0, 1.0, 1.0) );

	/// Hvostum Cube
	vectorVertices.Push( vec3( -1.0, 1.0, 0.0) );
	vectorVertices.Push( vec3( 1.0, 1.0, 0.0) );
	vectorVertices.Push( vec3( 1.0, -1.0, 0.0) );
	vectorVertices.Push( vec3( -1.0, -1.0, 0.0) );

	vectorVertices.Push( vec3( -1.0, 1.0, 0.999) );
	vectorVertices.Push( vec3( 1.0, 1.0, 0.999) );
	vectorVertices.Push( vec3( 1.0, -1.0, 0.999) );
	vectorVertices.Push( vec3( -1.0, -1.0, 0.999) );
	
	vectorArch->materials[vectorIndex]     = { .diffuseTextureID_ = grayTextureHandle, .specularTextureID_ = grayTextureHandle,
		.ambient = { 0.05f, 0.05f, 0.0f }, .shininess = 128.0f * 0.078125f }; 

	arch::entity frustum1 = archEntityManager->createEntity();
	arch::world.addEntityToArchetype( frustum1, arch::world.archetypes[11] );
	arch::EntityLocation frustumLocation1 = arch::world.entityLocations[arch::getId( frustum1 )];
	arch::MathObjectArchetype* frustumArch1 = static_cast<arch::MathObjectArchetype*>(frustumLocation1.arch);
	const uint32_t frustumIndex1 = frustumLocation1.index;

	frustumArch1->transforms[frustumIndex1]    = { .position = { 0.0f, 0.0f, 0.0f }, .forward = { 0.0, 0.0, -1.0 }, .yaw = 0.0, .pitch = 0.0, .scale = 1.0f };
	core::vector<vec3>& frustumVertices1 =  frustumArch1->generatedMesh[frustumIndex1].vertices;

	/// Hvostum Cube
	frustumVertices1.Push( vec3( -1.0, 1.0, 0.0) );
	frustumVertices1.Push( vec3( 1.0, 1.0, 0.0) );
	frustumVertices1.Push( vec3( 1.0, -1.0, 0.0) );
	frustumVertices1.Push( vec3( -1.0, -1.0, 0.0) );

	frustumVertices1.Push( vec3( -1.0, 1.0, 0.5) );
	frustumVertices1.Push( vec3( 1.0, 1.0, 0.5) );
	frustumVertices1.Push( vec3( 1.0, -1.0, 0.5) );
	frustumVertices1.Push( vec3( -1.0, -1.0, 0.5) );
	
	frustumArch1->materials[frustumIndex1] = { .diffuseTextureID_ = grayTextureHandle, .specularTextureID_ = grayTextureHandle,
		.ambient = { 0.05f, 0.05f, 0.0f }, .shininess = 128.0f * 0.078125f }; 
	
    ///< Game rendering loop
	std::cout << "ARCHETYPES NUMBER: " << arch::world.archetypes.GetSize() << std::endl;
	GLVM->GameLoop();

	GLVM->GameKill();

	delete EntityManager;
	delete ComponentManager;
	delete GLVM;
	
    return 0;
}
