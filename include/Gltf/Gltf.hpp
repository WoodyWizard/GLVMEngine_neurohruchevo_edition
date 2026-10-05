// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  glTF 2.0 loader.

  Supported: the whole core specification - .gltf with external or embedded (data: URI) buffers and .glb,
  any number of buffers, interleaved buffer views, all accessor types (SCALAR ... MAT4 with column padding)
  and component types, normalized integers, sparse accessors, all meshes and primitives with any attributes,
  all primitive modes, morph targets, node hierarchy (TRS and matrix), scenes, skins, animations
  (translation / rotation / scale / weights, LINEAR / STEP / CUBICSPLINE), materials, textures, samplers,
  images (PNG and JPEG are decoded), cameras.
  Extensions: KHR_mesh_quantization, KHR_texture_transform, KHR_lights_punctual,
  KHR_materials_emissive_strength, KHR_materials_unlit. A file that requires any other extension
  is rejected, other used (optional) extensions are ignored.

  All errors (missing files, malformed JSON, invalid references, out of bounds data) are reported with
  std::runtime_error("glTF loader: <file>: <message>"). Non fatal problems are collected in Model::warnings.
*/

#ifndef GLVM_GLTF_HPP
#define GLVM_GLTF_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace GLVM::gltf
{
	constexpr uint32_t INVALID_INDEX = UINT32_MAX;                 ///< Absent reference to another glTF object

	enum class ComponentType : uint32_t {
		BYTE           = 5120,
		UNSIGNED_BYTE  = 5121,
		SHORT          = 5122,
		UNSIGNED_SHORT = 5123,
		UNSIGNED_INT   = 5125,
		FLOAT          = 5126
	};

	enum class AccessorType : uint32_t { SCALAR, VEC2, VEC3, VEC4, MAT2, MAT3, MAT4 };

	enum class PrimitiveMode : uint32_t {
		POINTS         = 0,
		LINES          = 1,
		LINE_LOOP      = 2,
		LINE_STRIP     = 3,
		TRIANGLES      = 4,
		TRIANGLE_STRIP = 5,
		TRIANGLE_FAN   = 6
	};

	enum class AlphaMode : uint32_t { ALPHA_OPAQUE, ALPHA_MASK, ALPHA_BLEND };   ///< Not OPAQUE: it is a macro in <windows.h>
	enum class Interpolation : uint32_t { LINEAR, STEP, CUBICSPLINE };
	enum class TargetPath : uint32_t { TRANSLATION, ROTATION, SCALE, WEIGHTS };
	enum class CameraType : uint32_t { PERSPECTIVE, ORTHOGRAPHIC };
	enum class LightType : uint32_t { DIRECTIONAL, POINT, SPOT };

	uint32_t componentTypeSize( ComponentType componentType );   ///< Bytes of one component
	uint32_t componentsNumber( AccessorType type );              ///< SCALAR = 1 ... MAT4 = 16

	/*
	  ===================================================
	  Binary data
	  ===================================================
	*/
	struct Buffer {
		std::string          name;
		std::string          uri;                                  ///< Empty for the GLB binary chunk
		uint64_t             byteLength = 0;
		std::vector<uint8_t> data;
	};

	struct BufferView {
		std::string name;
		uint32_t    buffer     = INVALID_INDEX;
		uint64_t    byteOffset = 0;
		uint64_t    byteLength = 0;
		uint32_t    byteStride = 0;                                ///< 0 - tightly packed
		uint32_t    target     = 0;                                ///< 34962 ARRAY_BUFFER, 34963 ELEMENT_ARRAY_BUFFER, 0 - not set
	};

	struct AccessorSparse {
		uint32_t      count             = 0;
		uint32_t      indicesBufferView = INVALID_INDEX;
		uint64_t      indicesByteOffset = 0;
		ComponentType indicesComponentType = ComponentType::UNSIGNED_INT;
		uint32_t      valuesBufferView  = INVALID_INDEX;
		uint64_t      valuesByteOffset  = 0;
	};

	struct Accessor {
		std::string         name;
		uint32_t            bufferView    = INVALID_INDEX;         ///< INVALID_INDEX - all zeros (sparse values may be applied)
		uint64_t            byteOffset    = 0;
		ComponentType       componentType = ComponentType::FLOAT;
		bool                normalized    = false;
		uint32_t            count         = 0;
		AccessorType        type          = AccessorType::SCALAR;
		std::vector<double> min;
		std::vector<double> max;
		bool                isSparse      = false;
		AccessorSparse      sparse;
	};

	/*
	  ===================================================
	  Materials and textures
	  ===================================================
	*/
	struct TextureTransform {                                      ///< KHR_texture_transform
		float    offset[2] = { 0.0f, 0.0f };
		float    rotation  = 0.0f;                                 ///< Radians, counter-clockwise
		float    scale[2]  = { 1.0f, 1.0f };
		uint32_t texCoord  = INVALID_INDEX;                        ///< Overrides TextureInfo::texCoord if set
	};

	struct TextureInfo {
		uint32_t         index    = INVALID_INDEX;                 ///< Texture
		uint32_t         texCoord = 0;                             ///< TEXCOORD_<n> set
		float            scale    = 1.0f;                          ///< normalTexture.scale or occlusionTexture.strength
		bool             hasTransform = false;
		TextureTransform transform;

		bool isSet() const { return index != INVALID_INDEX; }
		uint32_t usedTexCoord() const { return hasTransform && transform.texCoord != INVALID_INDEX ? transform.texCoord : texCoord; }
	};

	struct Material {
		std::string name;
		float       baseColorFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		TextureInfo baseColorTexture;
		float       metallicFactor  = 1.0f;
		float       roughnessFactor = 1.0f;
		TextureInfo metallicRoughnessTexture;
		TextureInfo normalTexture;
		TextureInfo occlusionTexture;
		TextureInfo emissiveTexture;
		float       emissiveFactor[3] = { 0.0f, 0.0f, 0.0f };
		float       emissiveStrength  = 1.0f;                      ///< KHR_materials_emissive_strength
		AlphaMode   alphaMode   = AlphaMode::ALPHA_OPAQUE;
		float       alphaCutoff = 0.5f;
		bool        doubleSided = false;
		bool        unlit       = false;                           ///< KHR_materials_unlit
	};

	struct Sampler {
		std::string name;
		uint32_t    magFilter = 0;                                 ///< 0 - not set, 9728 NEAREST, 9729 LINEAR
		uint32_t    minFilter = 0;                                 ///< 0 - not set, 9728 ... 9987 (mipmap modes)
		uint32_t    wrapS     = 10497;                             ///< 33071 CLAMP_TO_EDGE, 33648 MIRRORED_REPEAT, 10497 REPEAT
		uint32_t    wrapT     = 10497;
	};

	struct Image {
		std::string          name;
		std::string          uri;                                  ///< Relative path or data: URI, empty if bufferView is used
		uint32_t             bufferView = INVALID_INDEX;
		std::string          mimeType;
		bool                 isDecoded = false;                    ///< Pixels are filled (LoadOptions::decodeImages)
		uint32_t             width  = 0;
		uint32_t             height = 0;
		std::vector<uint8_t> pixels;                               ///< RGBA8, rows top to bottom
	};

	struct Texture {
		std::string name;
		uint32_t    sampler = INVALID_INDEX;                       ///< INVALID_INDEX - repeat wrapping, auto filtering
		uint32_t    source  = INVALID_INDEX;                       ///< Image
	};

	/*
	  ===================================================
	  Geometry
	  ===================================================
	*/
	struct Attribute {
		std::string name;                                          ///< POSITION, NORMAL, TANGENT, TEXCOORD_n, COLOR_n, JOINTS_n, WEIGHTS_n, _CUSTOM
		uint32_t    accessor = INVALID_INDEX;
	};

	struct MorphTarget {
		std::vector<Attribute> attributes;                         ///< Displacements of the base attributes

		uint32_t findAttribute( const std::string& attributeName ) const;   ///< Accessor or INVALID_INDEX
	};

	struct Primitive {
		std::vector<Attribute>   attributes;
		uint32_t                 indices  = INVALID_INDEX;         ///< INVALID_INDEX - non indexed geometry
		uint32_t                 material = INVALID_INDEX;         ///< INVALID_INDEX - default material
		PrimitiveMode            mode     = PrimitiveMode::TRIANGLES;
		std::vector<MorphTarget> targets;

		uint32_t findAttribute( const std::string& attributeName ) const;   ///< Accessor or INVALID_INDEX
	};

	struct Mesh {
		std::string              name;
		std::vector<Primitive>   primitives;
		std::vector<float>       weights;                          ///< Default morph target weights
		std::vector<std::string> targetNames;                      ///< extras.targetNames (exporters convention)
	};

	/*
	  ===================================================
	  Scene graph
	  ===================================================
	*/
	struct Node {
		std::string           name;
		std::vector<uint32_t> children;
		uint32_t              parent = INVALID_INDEX;              ///< Filled by the loader
		uint32_t              mesh   = INVALID_INDEX;
		uint32_t              skin   = INVALID_INDEX;
		uint32_t              camera = INVALID_INDEX;
		uint32_t              light  = INVALID_INDEX;              ///< KHR_lights_punctual
		bool                  hasMatrix = false;                   ///< matrix is used instead of TRS (such node can't be animated)
		float                 matrix[16]     = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };   ///< Column-major
		float                 translation[3] = { 0.0f, 0.0f, 0.0f };
		float                 rotation[4]    = { 0.0f, 0.0f, 0.0f, 1.0f };            ///< Quaternion x, y, z, w
		float                 scale[3]       = { 1.0f, 1.0f, 1.0f };
		std::vector<float>    weights;                             ///< Morph weights, override Mesh::weights
	};

	struct Scene {
		std::string           name;
		std::vector<uint32_t> nodes;                               ///< Root nodes
	};

	struct Skin {
		std::string           name;
		uint32_t              inverseBindMatrices = INVALID_INDEX; ///< INVALID_INDEX - identity matrices
		uint32_t              skeleton = INVALID_INDEX;
		std::vector<uint32_t> joints;                              ///< Nodes
	};

	struct Camera {
		std::string name;
		CameraType  type = CameraType::PERSPECTIVE;
		float       aspectRatio = 0.0f;                            ///< 0 - not set (use the viewport aspect ratio)
		float       yfov  = 0.0f;                                  ///< Radians
		float       znear = 0.0f;
		float       zfar  = 0.0f;                                  ///< 0 - infinite projection (perspective only)
		float       xmag  = 0.0f;                                  ///< Orthographic half width
		float       ymag  = 0.0f;                                  ///< Orthographic half height
	};

	struct Light {                                                 ///< KHR_lights_punctual, shines along the local -Z axis
		std::string name;
		LightType   type = LightType::POINT;
		float       color[3] = { 1.0f, 1.0f, 1.0f };
		float       intensity = 1.0f;                              ///< Candela (point, spot) or lux (directional)
		float       range = 0.0f;                                  ///< 0 - infinite
		float       innerConeAngle = 0.0f;
		float       outerConeAngle = 0.78539816339f;               ///< PI / 4
	};

	/*
	  ===================================================
	  Animation
	  ===================================================
	*/
	struct AnimationSampler {
		uint32_t           input  = INVALID_INDEX;                 ///< Accessor of key frame times
		uint32_t           output = INVALID_INDEX;                 ///< Accessor of values
		Interpolation      interpolation = Interpolation::LINEAR;
		std::vector<float> times;                                  ///< Decoded input, strictly increasing
		std::vector<float> values;                                 ///< Decoded output (normalized integers are converted)
		uint32_t           valueSize = 0;                          ///< Floats per value: 3, 4 or morph targets number
	};

	struct AnimationChannel {
		uint32_t   sampler = INVALID_INDEX;
		uint32_t   node    = INVALID_INDEX;                        ///< INVALID_INDEX - channel has no target and is ignored
		TargetPath path    = TargetPath::TRANSLATION;
	};

	struct Animation {
		std::string                   name;
		std::vector<AnimationChannel> channels;
		std::vector<AnimationSampler> samplers;
		float                         startTime = 0.0f;            ///< Smallest key frame time
		float                         endTime   = 0.0f;            ///< Largest key frame time
	};

	struct Asset {
		std::string version;
		std::string minVersion;
		std::string generator;
		std::string copyright;
	};

	struct Model {
		std::string                filePath;
		Asset                      asset;
		std::vector<std::string>   extensionsUsed;
		std::vector<std::string>   extensionsRequired;
		std::vector<Buffer>        buffers;
		std::vector<BufferView>    bufferViews;
		std::vector<Accessor>      accessors;
		std::vector<Mesh>          meshes;
		std::vector<Node>          nodes;
		std::vector<Scene>         scenes;
		uint32_t                   scene = INVALID_INDEX;          ///< Default scene
		std::vector<Skin>          skins;
		std::vector<Animation>     animations;
		std::vector<Material>      materials;
		std::vector<Texture>       textures;
		std::vector<Sampler>       samplers;
		std::vector<Image>         images;
		std::vector<Camera>        cameras;
		std::vector<Light>         lights;
		std::vector<std::string>   warnings;                       ///< Non fatal problems found while loading

		/// Root nodes of the scene to show: the default scene, else scene 0, else all nodes without a parent.
		std::vector<uint32_t> sceneRootNodes() const;
	};

	struct LoadOptions {
		bool decodeImages = true;                                  ///< Decode PNG/JPEG images into RGBA8 pixels
	};

	/// Loads a .gltf or .glb file (detected by content). Paths are UTF-8.
	Model loadModel( const std::string& filePath, const LoadOptions& options = {} );

	/// Loads a model from memory. baseDirectory resolves relative URIs, sourceName is used in messages.
	Model loadModelFromMemory( const uint8_t* data, uint64_t size, const std::string& baseDirectory,
							   const std::string& sourceName, const LoadOptions& options = {} );

	/*
	  ===================================================
	  Accessor data. Elements are returned in a flat array of count * componentsNumber values,
	  matrices are column-major without padding, sparse substitution is applied.
	  ===================================================
	*/
	std::vector<float>    readAccessorAsFloats( const Model& model, uint32_t accessor );   ///< Normalized integers -> [0, 1] / [-1, 1]
	std::vector<uint32_t> readAccessorAsUints( const Model& model, uint32_t accessor );    ///< Integer component types only

	/*
	  ===================================================
	  Transforms and animation. Matrices are column-major (glTF convention): element (row r, column c) is m[c * 4 + r].
	  ===================================================
	*/
	struct Mat4 {
		float m[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

		Mat4 operator*( const Mat4& other ) const;                 ///< Applies other first, then this
		void transformPoint( const float in[3], float out[3] ) const;
		void transformDirection( const float in[3], float out[3] ) const;   ///< Upper 3x3 only
		float determinant3x3() const;
		Mat4 inverse() const;                                      ///< General inverse, identity if singular
		static Mat4 fromTRS( const float translation[3], const float rotation[4], const float scale[3] );
	};

	/// Local transform of every node. Animated values override the node's TRS.
	struct Pose {
		std::vector<float>              translations;              ///< 3 per node
		std::vector<float>              rotations;                 ///< 4 per node
		std::vector<float>              scales;                    ///< 3 per node
		std::vector<std::vector<float>> weights;                   ///< Morph weights per node (empty - mesh default)
		std::vector<bool>               animated;                  ///< TRS of the node was set by an animation
	};

	Pose restPose( const Model& model );                           ///< Node TRS from the file
	/// Samples every channel of the animation at time (seconds, clamped to the key frame range) into the pose.
	void applyAnimation( const Model& model, uint32_t animation, float time, Pose& pose );
	/// Value of one sampler at time; output must hold sampler.valueSize floats. Rotations are normalized.
	void sampleAnimationSampler( const AnimationSampler& sampler, TargetPath path, float time, float* output );

	Mat4 localTransform( const Model& model, const Pose& pose, uint32_t node );
	std::vector<Mat4> worldTransforms( const Model& model, const Pose& pose );   ///< Per node, parents applied
	/// Skinning matrix of every joint: world(joint) * inverseBindMatrix. The skinned mesh node's own transform is ignored.
	std::vector<Mat4> jointMatrices( const Model& model, uint32_t skin, const std::vector<Mat4>& worldTransforms );
	std::vector<Mat4> inverseBindMatrices( const Model& model, uint32_t skin );
	/// Morph weights of a node's mesh in the pose: animated, else node weights, else mesh weights, else zeros.
	std::vector<float> morphWeights( const Model& model, const Pose& pose, uint32_t node );

	/// Attribute of a primitive with morph targets applied: base + sum(weight * target). Returns count * components floats.
	std::vector<float> morphedAttribute( const Model& model, const Primitive& primitive, const std::string& attributeName,
										 const std::vector<float>& weights );
}

#endif
