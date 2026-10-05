// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  Converts a glTF model into the engine mesh format: one de-indexed triangle list with
  position(3) normal(3) uv(2) per vertex, plus joints(4) weights(4) for animated meshes, and
  joint matrices sampled at key frame times.

  - Every mesh node of the scene is included. Static models get node transforms baked into the vertices.
  - Animated models (skins, or animated nodes) keep vertices in joint space: skinned meshes use their skin
    joints, other mesh nodes become rigid joints that follow the node's world transform. So node animations
    without a skin work through the engine's skinning path too.
  - Animation `animation` is sampled at its key frame times (long gaps are resampled), every channel is
    interpolated at its own times (LINEAR / STEP / CUBICSPLINE).
  - Strips and fans are triangulated, points and lines are skipped. Missing normals become flat normals.
  - Morph targets are applied with their default weights (morph animation can't be expressed with joints).
  - UVs are TEXCOORD set of the base color texture with its KHR_texture_transform applied.
*/

#ifndef GLVM_GLTF_ENGINE_ADAPTER_HPP
#define GLVM_GLTF_ENGINE_ADAPTER_HPP

#include "Gltf/Gltf.hpp"

namespace GLVM::gltf
{
	struct EngineBakeOptions {
		uint32_t animation    = 0;                                 ///< Animation baked into joint matrices
		uint32_t maxJoints    = 128;                               ///< Engine limit, exceeding it is reported
		float    maxFrameStep = 1.0f / 30.0f;                      ///< Key frame gaps longer than subdivideGap are resampled with this step
		float    subdivideGap = 0.05f;
	};

	struct EngineMesh {
		bool                           isAnimated = false;         ///< 16 floats per vertex (with joints and weights), else 8
		std::vector<float>             vertices;
		std::vector<uint32_t>          indices;                    ///< 0 ... vertexCount - 1
		std::vector<std::vector<Mat4>> jointMatrices;              ///< [joint][frame], column-major glTF matrices
		std::vector<float>             frameTimes;                 ///< Time of every frame, seconds
		float                          topY = 0.0f;                ///< Highest vertex Y in the bind pose (skinned vertices are not posed)
		std::vector<std::string>       warnings;

		uint32_t floatsPerVertex() const { return isAnimated ? 16u : 8u; }
	};

	/// Throws std::runtime_error if the scene has no triangles.
	EngineMesh bakeForEngine( const Model& model, const EngineBakeOptions& options = {} );
}

#endif
