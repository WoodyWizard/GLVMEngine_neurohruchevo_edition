# Game Loop Versatile Modules (GLVM)

This is my simple game engine for Linux and Windows OS's with Vulkan support. Its based on entity component system (ECS) with user friendly C++ interface. Also it has partial support of GLTf and wavefront.obj 3D model formats. With GLVM you can make simple phong light of three types (directional, spot, point). Very basic physics included (collitions, gravity).
Updated version 2.0 with: new Archetype ECS (SOA powered), Vulkan config, read-only render objects, VK command sub-buffers. Refactored: inventory system, gltf parser...

## Linux

The Linux build uses the **Wayland** window backend (run it inside a Wayland session: GNOME, KDE Plasma, Sway, ...).
X11 backends (Xlib, XCB) exist too; they are selected at compile time with `VK_USE_PLATFORM_XLIB_KHR` /
`VK_USE_PLATFORM_XCB_KHR` instead of `VK_USE_PLATFORM_WAYLAND_KHR` in `include/GraphicAPI/Vulkan.hpp`.

* ### Requirements

        C++20 compiler: clang (default) with libstdc++ 14 or newer.

        Libraries: Wayland client, X11 (Xlib, XCB), ALSA, Vulkan loader + headers.

        A Vulkan driver for your GPU (Mesa RADV/ANV, NVIDIA).

* ### Packages
* #### Fedora:
        sudo dnf install clang make wayland-devel libX11-devel libxcb-devel \
                         alsa-lib-devel vulkan-loader-devel vulkan-headers
        # sanitizer build (make SANITIZE=1):
        sudo dnf install compiler-rt
        # optional: shader compiler for VKshaders/*/compile.sh, Vulkan validation layers
        sudo dnf install glslang vulkan-validation-layers

* #### Debian / Ubuntu:
        sudo apt install clang make libwayland-dev libx11-dev libxcb1-dev \
                         libasound2-dev libvulkan-dev
        # sanitizer build (make SANITIZE=1): the clang runtime package, e.g. libclang-rt-18-dev
        # optional:
        sudo apt install glslang-tools vulkan-validationlayers

* #### Arch:
        sudo pacman -S clang make wayland libx11 libxcb alsa-lib vulkan-icd-loader vulkan-headers
        # sanitizer build (make SANITIZE=1):
        sudo pacman -S compiler-rt
        # optional:
        sudo pacman -S glslang vulkan-validation-layers

* #### Gentoo:
        emerge --ask llvm-core/clang dev-libs/wayland x11-libs/libX11 x11-libs/libxcb \
                     media-libs/alsa-lib media-libs/vulkan-loader dev-util/vulkan-headers

* ### Building and running
  Run make in the project root:

      make -f MakefileLin -j$(nproc)              # build/linGame (-O2 -g)
      make -f MakefileLin run                     # build and start the game

  The game can be started from any directory (`./build/linGame`), but the executable must stay in
  `build/`: resources are looked up as `../<dir>/` relative to it.

  Build options:

      make -f MakefileLin SANITIZE=1 -j$(nproc)   # ASan + UBSan build: build/linGame-asan
      make -f MakefileLin WERROR=1                # treat warnings as errors
      make -f MakefileLin clean
      make -f MakefileLin wayland-protocols       # regenerate Wayland protocol code from WaylandProtocols/*.xml
      make -f MakefileLin udp-client              # small UDP test client: build/udpClient

## Windows

* ### Development libraries:

        Vulkan SDK (https://vulkan.lunarg.com), it sets the VULKAN_SDK environment variable.

* ### Specific tools:
* #### First of all you need MSYS2:
        You can get it from official website (https://www.msys2.org/) or

         winget install MSYS2.MSYS2

* #### Then get needed compiler tools and Vulkan:
  Inside MSYS2 for simplier way of installing packages frist of all we need to install pactoys:

      pacman -S pactoys

  Now we can use just shortened names of packages inside any MSYS2 toolchain:

      pacboy -S make:p gcc:p vulkan-devel:p

* ### Building
  Run make in the project root with the makefile for your shell:

      make -f make_files/Windows/MakefileWinMSYS           # MSYS2 UCRT64/CLANG64 shell, Vulkan from MSYS2
      mingw32-make -f make_files/Windows/MakefileWin       # cmd.exe, MinGW g++ + Vulkan SDK
      mingw32-make -f make_files/Windows/MakefileWinPS     # PowerShell, MinGW g++ + Vulkan SDK
      mingw32-make -f make_files/Windows/MakefileWinClang  # cmd.exe, LLVM clang + Vulkan SDK

  The result is `build\winGame.exe`; it must stay in `build\` (resources are looked up as `..\<dir>\`).

* ### Cross-compiling from Linux

      VULKAN_SDK=/path/to/VulkanSDK make -f MakefileMingw -j$(nproc)          # MinGW-w64 GCC
      make -f MakefileWine LLVM_MINGW=/path/to/llvm-mingw -j$(nproc)          # llvm-mingw, run with: cd build && wine winGame.exe

## glTF models

Models are loaded by the glTF 2.0 loader in `src/Gltf` (interface: `include/Gltf/Gltf.hpp`):

* `.gltf` with external or embedded (`data:` URI) buffers and `.glb`, any number of buffers;
* the whole core specification: all meshes, primitives and primitive modes, interleaved, normalized and
  sparse accessors, node hierarchy (TRS and matrices), scenes, skins, morph targets, animations
  (translation, rotation, scale, weights with LINEAR, STEP and CUBICSPLINE interpolation), materials,
  textures, samplers, cameras; PNG and JPEG images are decoded with stb_image (`include/ThirdParty`);
* extensions: `KHR_mesh_quantization`, `KHR_texture_transform`, `KHR_lights_punctual`,
  `KHR_materials_emissive_strength`, `KHR_materials_unlit`. A file that requires another extension
  (Draco, meshopt, Basis Universal...) is rejected with a message.

Broken files are reported with `std::runtime_error("glTF loader: <file>: <what is wrong>")`, non fatal problems are
printed as warnings.

The engine gets a model through `GLVM::gltf::bakeForEngine` (`include/Gltf/GltfEngineAdapter.hpp`): every mesh node
of the scene goes into one vertex buffer. Static models get their node transforms baked into the vertices, animated
models (skins or animated nodes) are skinned on the GPU, nodes animated without a skin follow their node as a rigid
joint. `animations[0]` is sampled at its key frame times. Engine limits: one animation per model, 128 joints, morph
targets are shown with their default weights, points and lines are skipped.

Tests (synthetic models, plus every model of the given directories, e.g. the
[Khronos sample models](https://github.com/KhronosGroup/glTF-Sample-Assets)):

    make -f MakefileLin gltf-tests
    ./build/gltfTests path/to/glTF-Sample-Assets/Models

## Controls

    W A S D     move
    Space       jump
    Mouse       look around (the cursor is captured after the first click into the window)
    Left button shoot / drag items in the inventory
    I           inventory
    O           collision debug view
    Esc         quit (closing the window works too)

## Shaders

Compiled SPIR-V shaders are in the repository. After editing a shader run `compile.sh` (needs `glslangValidator`)
in its directory under `VKshaders/`.
