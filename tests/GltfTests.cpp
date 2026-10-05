// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/*
  Tests of the glTF loader (src/Gltf).

    gltfTests                     - synthetic models built in memory
    gltfTests <file or dir>...    - also loads every .gltf / .glb found (e.g. Khronos glTF-Sample-Assets/Models)
                                    and checks accessor bounds, key frame sampling and the engine bake
*/

#include "Gltf/Gltf.hpp"
#include "Gltf/GltfEngineAdapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace GLVM::gltf;

namespace
{
	int checksFailed = 0;
	int checksPassed = 0;
	std::string currentTest;

	void check( bool condition, const std::string& what ) {
		if ( condition ) {
			++checksPassed;
		} else {
			++checksFailed;
			std::cout << "  FAIL [" << currentTest << "] " << what << std::endl;
		}
	}

	void checkNear( double actual, double expected, const std::string& what, double tolerance = 1e-5 ) {
		check( std::fabs( actual - expected ) <= tolerance, what + ": " + std::to_string(actual) + " != " + std::to_string(expected) );
	}

	void runTest( const std::string& name, const std::function<void()>& test ) {
		currentTest = name;
		try {
			test();
		} catch ( const std::exception& error ) {
			check( false, std::string("unexpected exception: ") + error.what() );
		}
	}

	/// Binary buffer builder.
	struct Blob {
		std::vector<uint8_t> bytes;

		size_t add( const void* data, size_t size ) {
			while ( bytes.size() % 4 != 0 )
				bytes.push_back( 0 );
			const size_t offset = bytes.size();
			bytes.insert( bytes.end(), (const uint8_t*)data, (const uint8_t*)data + size );
			return offset;
		}
		template<typename T>
		size_t add( const std::vector<T>& values ) { return add( values.data(), values.size() * sizeof(T) ); }
	};

	std::string base64( const std::vector<uint8_t>& data ) {
		static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string output;
		for ( size_t i = 0; i < data.size(); i += 3 ) {
			const uint32_t chunk = (uint32_t)data[i] << 16 | (i + 1 < data.size() ? (uint32_t)data[i + 1] << 8 : 0) |
				(i + 2 < data.size() ? (uint32_t)data[i + 2] : 0);
			output += alphabet[(chunk >> 18) & 63];
			output += alphabet[(chunk >> 12) & 63];
			output += i + 1 < data.size() ? alphabet[(chunk >> 6) & 63] : '=';
			output += i + 2 < data.size() ? alphabet[chunk & 63] : '=';
		}
		return output;
	}

	std::string dataUri( const Blob& blob ) {
		return "data:application/octet-stream;base64," + base64( blob.bytes );
	}

	/// Replaces every "$BUFFER" with a buffer object for the blob.
	std::string withBuffer( std::string json, const Blob& blob ) {
		const std::string buffer = "{\"byteLength\":" + std::to_string(blob.bytes.size()) + ",\"uri\":\"" + dataUri(blob) + "\"}";
		for ( size_t at = json.find("$BUFFER"); at != std::string::npos; at = json.find("$BUFFER") )
			json.replace( at, 7, buffer );
		return json;
	}

	Model loadJson( const std::string& json, const std::string& baseDirectory = "." ) {
		return loadModelFromMemory( (const uint8_t*)json.data(), json.size(), baseDirectory, "test.gltf" );
	}

	void expectFailure( const std::string& json, const std::string& expectedMessagePart ) {
		try {
			loadJson( json );
			check( false, "no error, expected \"" + expectedMessagePart + "\"" );
		} catch ( const std::runtime_error& error ) {
			check( std::string(error.what()).find( expectedMessagePart ) != std::string::npos,
				   "error \"" + std::string(error.what()) + "\" doesn't contain \"" + expectedMessagePart + "\"" );
		}
	}

	std::vector<uint8_t> makeGlb( const std::string& json, const std::vector<uint8_t>& binary ) {
		std::string paddedJson = json;
		while ( paddedJson.size() % 4 != 0 )
			paddedJson += ' ';
		std::vector<uint8_t> paddedBinary = binary;
		while ( paddedBinary.size() % 4 != 0 )
			paddedBinary.push_back( 0 );

		std::vector<uint8_t> glb;
		auto u32 = [&glb]( uint32_t value ) {
			for ( int i = 0; i < 4; ++i )
				glb.push_back( (uint8_t)(value >> (8 * i)) );
		};
		const uint32_t total = 12 + 8 + (uint32_t)paddedJson.size() + (paddedBinary.empty() ? 0 : 8 + (uint32_t)paddedBinary.size());
		u32( 0x46546C67 ); u32( 2 ); u32( total );
		u32( (uint32_t)paddedJson.size() ); u32( 0x4E4F534A );
		glb.insert( glb.end(), paddedJson.begin(), paddedJson.end() );
		if ( !paddedBinary.empty() ) {
			u32( (uint32_t)paddedBinary.size() ); u32( 0x004E4942 );
			glb.insert( glb.end(), paddedBinary.begin(), paddedBinary.end() );
		}
		return glb;
	}

	/// Triangle (0,0,0) (1,0,0) (0,1,0) as POSITION accessor 0, optional extra JSON members are inserted.
	Blob triangleBlob() {
		Blob blob;
		blob.add( std::vector<float>{ 0, 0, 0,  1, 0, 0,  0, 1, 0 } );
		return blob;
	}
	const char* TRIANGLE_ACCESSORS =
		"\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]}],";

	/*
	  ===================================================
	  Synthetic tests
	  ===================================================
	*/
	void testMinimalTriangle() {
		const Model model = loadJson( withBuffer( std::string("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],") + TRIANGLE_ACCESSORS +
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}",
			triangleBlob() ) );
		check( model.meshes.size() == 1 && model.nodes.size() == 1, "one mesh and node" );
		const std::vector<float> positions = readAccessorAsFloats( model, 0 );
		check( positions.size() == 9 && positions[3] == 1.0f && positions[7] == 1.0f, "positions" );

		const EngineMesh mesh = bakeForEngine( model );
		check( !mesh.isAnimated && mesh.vertices.size() == 3 * 8 && mesh.indices.size() == 3, "baked static triangle" );
		checkNear( mesh.vertices[5], 1.0, "flat normal z of a CCW triangle" );
		checkNear( mesh.topY, 1.0, "topY" );
	}

	void testGlb() {
		const Blob blob = triangleBlob();
		const std::string json = std::string("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":36}],") + TRIANGLE_ACCESSORS +
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],\"nodes\":[{\"mesh\":0}]}";
		const std::vector<uint8_t> glb = makeGlb( json, blob.bytes );
		const Model model = loadModelFromMemory( glb.data(), glb.size(), ".", "test.glb" );
		check( readAccessorAsFloats( model, 0 )[3] == 1.0f, "GLB binary chunk is buffer 0" );
		check( model.sceneRootNodes().size() == 1, "root nodes without scenes" );

		std::vector<uint8_t> truncated = glb;
		truncated.resize( truncated.size() - 8 );
		try {
			loadModelFromMemory( truncated.data(), truncated.size(), ".", "test.glb" );
			check( false, "truncated GLB is rejected" );
		} catch ( const std::runtime_error& error ) {
			check( std::string(error.what()).find("larger than the file") != std::string::npos, "truncated GLB message" );
		}
	}

	void testSparseAccessors() {
		Blob blob;
		const size_t base    = blob.add( std::vector<float>{ 0, 1, 2, 3, 4, 5 } );
		const size_t indices = blob.add( std::vector<uint8_t>{ 1, 4 } );
		const size_t values  = blob.add( std::vector<float>{ 10, 40 } );
		const size_t indices2 = blob.add( std::vector<uint16_t>{ 2 } );
		const size_t values2  = blob.add( std::vector<float>{ 7 } );
		const std::string json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],\"bufferViews\":["
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(base) + ",\"byteLength\":24},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(indices) + ",\"byteLength\":2},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(values) + ",\"byteLength\":8},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(indices2) + ",\"byteLength\":2},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(values2) + ",\"byteLength\":4}],"
			"\"accessors\":["
			"{\"bufferView\":0,\"componentType\":5126,\"count\":6,\"type\":\"SCALAR\",\"sparse\":{\"count\":2,"
			"\"indices\":{\"bufferView\":1,\"componentType\":5121},\"values\":{\"bufferView\":2}}},"
			"{\"componentType\":5126,\"count\":4,\"type\":\"SCALAR\",\"sparse\":{\"count\":1,"
			"\"indices\":{\"bufferView\":3,\"componentType\":5123},\"values\":{\"bufferView\":4}}}]}";
		const Model model = loadJson( withBuffer( json, blob ) );
		const std::vector<float> first = readAccessorAsFloats( model, 0 );
		check( first == std::vector<float>{ 0, 10, 2, 3, 40, 5 }, "sparse values over a buffer view" );
		const std::vector<float> second = readAccessorAsFloats( model, 1 );
		check( second == std::vector<float>{ 0, 0, 7, 0 }, "sparse values over zeros" );
	}

	void testMatrixPaddingAndNormalization() {
		Blob blob;
		const size_t mat3 = blob.add( std::vector<int8_t>{ 127, 0, 0, 0,   0, 127, 0, 0,   0, 0, -127, 0 } );   ///< Columns padded to 4 bytes
		const size_t mat2 = blob.add( std::vector<uint8_t>{ 1, 2, 0, 0,   3, 4, 0, 0 } );
		const size_t uv   = blob.add( std::vector<uint16_t>{ 65535, 0 } );
		const std::string json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],\"bufferViews\":["
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(mat3) + ",\"byteLength\":12},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(mat2) + ",\"byteLength\":8},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(uv) + ",\"byteLength\":4}],"
			"\"accessors\":["
			"{\"bufferView\":0,\"componentType\":5120,\"normalized\":true,\"count\":1,\"type\":\"MAT3\"},"
			"{\"bufferView\":1,\"componentType\":5121,\"count\":1,\"type\":\"MAT2\"},"
			"{\"bufferView\":2,\"componentType\":5123,\"normalized\":true,\"count\":1,\"type\":\"VEC2\"}]}";
		const Model model = loadJson( withBuffer( json, blob ) );
		check( readAccessorAsFloats( model, 0 ) == std::vector<float>{ 1, 0, 0, 0, 1, 0, 0, 0, -1 }, "normalized BYTE MAT3 with column padding" );
		check( readAccessorAsFloats( model, 1 ) == std::vector<float>{ 1, 2, 3, 4 }, "UNSIGNED_BYTE MAT2 with column padding" );
		check( readAccessorAsFloats( model, 2 ) == std::vector<float>{ 1, 0 }, "normalized UNSIGNED_SHORT" );
		check( readAccessorAsUints( model, 1 ) == std::vector<uint32_t>{ 1, 2, 3, 4 }, "integers" );
	}

	void testInterleavedBufferView() {
		Blob blob;
		struct Vertex { float position[3]; uint16_t uv[2]; float pad; };
		const Vertex vertices[2] = { { { 1, 2, 3 }, { 0, 65535 }, 0 }, { { 4, 5, 6 }, { 65535, 0 }, 0 } };
		blob.add( vertices, sizeof(vertices) );
		const std::string json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],\"bufferViews\":["
			"{\"buffer\":0,\"byteLength\":40,\"byteStride\":20}],\"accessors\":["
			"{\"bufferView\":0,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"},"
			"{\"bufferView\":0,\"byteOffset\":12,\"componentType\":5123,\"normalized\":true,\"count\":2,\"type\":\"VEC2\"}]}";
		const Model model = loadJson( withBuffer( json, blob ) );
		check( readAccessorAsFloats( model, 0 ) == std::vector<float>{ 1, 2, 3, 4, 5, 6 }, "strided positions" );
		check( readAccessorAsFloats( model, 1 ) == std::vector<float>{ 0, 1, 1, 0 }, "strided normalized UVs" );
	}

	AnimationSampler makeSampler( Interpolation interpolation, std::vector<float> times, std::vector<float> values, uint32_t valueSize ) {
		AnimationSampler sampler;
		sampler.interpolation = interpolation;
		sampler.times = std::move(times);
		sampler.values = std::move(values);
		sampler.valueSize = valueSize;
		return sampler;
	}

	void testAnimationSampling() {
		float value[4];
		const AnimationSampler linear = makeSampler( Interpolation::LINEAR, { 0, 2 }, { 0, 0, 0,  2, 4, 6 }, 3 );
		sampleAnimationSampler( linear, TargetPath::TRANSLATION, 0.5f, value );
		check( value[0] == 0.5f && value[1] == 1.0f && value[2] == 1.5f, "LINEAR translation" );
		sampleAnimationSampler( linear, TargetPath::TRANSLATION, -1.0f, value );
		check( value[0] == 0.0f, "time before the first key is clamped" );
		sampleAnimationSampler( linear, TargetPath::TRANSLATION, 5.0f, value );
		check( value[0] == 2.0f, "time after the last key is clamped" );

		const AnimationSampler step = makeSampler( Interpolation::STEP, { 0, 1, 2 }, { 1, 0, 0,  2, 0, 0,  3, 0, 0 }, 3 );
		sampleAnimationSampler( step, TargetPath::SCALE, 1.5f, value );
		check( value[0] == 2.0f, "STEP between keys" );
		sampleAnimationSampler( step, TargetPath::SCALE, 1.0f, value );
		check( value[0] == 2.0f, "STEP at a key" );

		const float half = std::sqrt(0.5f);
		const AnimationSampler rotation = makeSampler( Interpolation::LINEAR, { 0, 1 }, { 0, 0, 0, 1,  0, half, 0, half }, 4 );
		sampleAnimationSampler( rotation, TargetPath::ROTATION, 0.5f, value );
		checkNear( value[1], std::sin( 3.14159265 / 8 ), "slerp y" );
		checkNear( value[3], std::cos( 3.14159265 / 8 ), "slerp w" );

		const AnimationSampler shortest = makeSampler( Interpolation::LINEAR, { 0, 1 }, { 0, 0, 0, 1,  0, 0, 0, -1 }, 4 );
		sampleAnimationSampler( shortest, TargetPath::ROTATION, 0.5f, value );
		checkNear( std::fabs(value[3]), 1.0, "slerp takes the shortest arc (q and -q)" );

		/// Keys (in tangent, value, out tangent): v0 = 0, b0 = 2, a1 = 0, v1 = 1 over 2 seconds: p(1) = 0.25 * 2 + 0.5 * 1 = 1
		const AnimationSampler cubic = makeSampler( Interpolation::CUBICSPLINE, { 0, 2 },
			{ 0, 0, 0,  0, 0, 0,  2, 0, 0,     0, 0, 0,  1, 0, 0,  0, 0, 0 }, 3 );
		sampleAnimationSampler( cubic, TargetPath::TRANSLATION, 1.0f, value );
		checkNear( value[0], 1.0, "CUBICSPLINE with tangents scaled by the key duration" );
		sampleAnimationSampler( cubic, TargetPath::TRANSLATION, 2.0f, value );
		checkNear( value[0], 1.0, "CUBICSPLINE last key" );

		const AnimationSampler weights = makeSampler( Interpolation::LINEAR, { 0, 1 }, { 0, 1,  1, 0 }, 2 );
		sampleAnimationSampler( weights, TargetPath::WEIGHTS, 0.25f, value );
		check( value[0] == 0.25f && value[1] == 0.75f, "morph weights" );
	}

	void testHierarchySkinAndMorph() {
		Blob blob;
		const size_t positions = blob.add( std::vector<float>{ 0, 0, 0,  1, 0, 0,  0, 1, 0 } );
		const size_t target    = blob.add( std::vector<float>{ 1, 2, 3,  0, 0, 0,  0, 0, 0 } );
		const size_t ibm       = blob.add( std::vector<float>{ 1,0,0,0, 0,1,0,0, 0,0,1,0, -5,0,0,1 } );
		const size_t joints    = blob.add( std::vector<uint8_t>{ 0,0,0,0, 0,0,0,0, 0,0,0,0 } );
		const size_t weights   = blob.add( std::vector<float>{ 1,0,0,0, 1,0,0,0, 1,0,0,0 } );
		const float s = std::sqrt(0.5f);
		const std::string json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],\"bufferViews\":["
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(positions) + ",\"byteLength\":36},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(target) + ",\"byteLength\":36},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(ibm) + ",\"byteLength\":64},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(joints) + ",\"byteLength\":12},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(weights) + ",\"byteLength\":48}],"
			"\"accessors\":["
			"{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
			"{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
			"{\"bufferView\":2,\"componentType\":5126,\"count\":1,\"type\":\"MAT4\"},"
			"{\"bufferView\":3,\"componentType\":5121,\"count\":3,\"type\":\"VEC4\"},"
			"{\"bufferView\":4,\"componentType\":5126,\"count\":3,\"type\":\"VEC4\"}],"
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":3,\"WEIGHTS_0\":4},\"targets\":[{\"POSITION\":1}]}],\"weights\":[0.5]}],"
			"\"skins\":[{\"joints\":[1],\"inverseBindMatrices\":2}],"
			"\"nodes\":[{\"translation\":[1,2,3],\"children\":[1]},"
			"{\"rotation\":[0,0," + std::to_string(s) + "," + std::to_string(s) + "],\"children\":[2]},"
			"{\"translation\":[1,0,0]},{\"mesh\":0,\"skin\":0}],\"scenes\":[{\"nodes\":[0,3]}]}";
		const Model model = loadJson( withBuffer( json, blob ) );
		check( model.nodes[2].parent == 1 && model.nodes[1].parent == 0, "parents" );

		const Pose pose = restPose( model );
		const std::vector<Mat4> world = worldTransforms( model, pose );
		const float origin[3] = { 0, 0, 0 };
		float point[3];
		world[2].transformPoint( origin, point );
		checkNear( point[0], 1.0, "world x" );
		checkNear( point[1], 3.0, "world y (child rotated 90 degrees around Z)" );
		checkNear( point[2], 3.0, "world z" );

		const std::vector<Mat4> skinning = jointMatrices( model, 0, world );
		const float bindPoint[3] = { 5, 0, 0 };
		skinning[0].transformPoint( bindPoint, point );
		checkNear( point[0], 1.0, "joint matrix = world * inverse bind (x)" );
		checkNear( point[1], 2.0, "joint matrix = world * inverse bind (y)" );

		const std::vector<float> morphed = morphedAttribute( model, model.meshes[0].primitives[0], "POSITION", morphWeights( model, pose, 3 ) );
		checkNear( morphed[0], 0.5, "morph target x" );
		checkNear( morphed[2], 1.5, "morph target z" );

		const EngineMesh mesh = bakeForEngine( model );
		check( mesh.isAnimated && mesh.jointMatrices.size() == 1 && mesh.frameTimes.size() == 1, "skinned bake without animation" );
		check( mesh.vertices.size() == 3 * 16 && mesh.vertices[8] == 0.0f && mesh.vertices[12] == 1.0f, "joint and weight of skinned vertices" );
	}

	void testTextureTransformAndLights() {
		Blob blob = triangleBlob();
		const size_t uvs = blob.add( std::vector<float>{ 1, 0,  1, 0,  1, 0 } );
		const std::string json = "{\"asset\":{\"version\":\"2.0\"},"
			"\"extensionsUsed\":[\"KHR_texture_transform\",\"KHR_lights_punctual\",\"KHR_materials_emissive_strength\"],"
			"\"extensionsRequired\":[\"KHR_texture_transform\"],"
			"\"extensions\":{\"KHR_lights_punctual\":{\"lights\":[{\"type\":\"spot\",\"intensity\":5,\"spot\":{\"outerConeAngle\":0.5}}]}},"
			"\"buffers\":[$BUFFER],\"bufferViews\":[{\"buffer\":0,\"byteLength\":36},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(uvs) + ",\"byteLength\":24}],"
			"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
			"{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"}],"
			"\"textures\":[{}],"
			"\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0,\"extensions\":{\"KHR_texture_transform\":"
			"{\"offset\":[0.5,0],\"rotation\":1.5707963,\"scale\":[2,1]}}}},"
			"\"emissiveFactor\":[1,0,0],\"extensions\":{\"KHR_materials_emissive_strength\":{\"emissiveStrength\":4}}}],"
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"material\":0}]}],"
			"\"nodes\":[{\"mesh\":0,\"extensions\":{\"KHR_lights_punctual\":{\"light\":0}}}]}";
		const Model model = loadJson( withBuffer( json, blob ) );
		check( model.lights.size() == 1 && model.lights[0].type == LightType::SPOT && model.nodes[0].light == 0, "KHR_lights_punctual" );
		checkNear( model.lights[0].outerConeAngle, 0.5, "spot outer cone" );
		checkNear( model.materials[0].emissiveStrength, 4.0, "KHR_materials_emissive_strength" );
		check( model.materials[0].baseColorTexture.hasTransform, "KHR_texture_transform parsed" );

		/// uv (1, 0): scale -> (2, 0), rotate 90 degrees -> (0, 2), offset -> (0.5, 2)
		const EngineMesh mesh = bakeForEngine( model );
		checkNear( mesh.vertices[6], 0.5, "transformed u", 1e-4 );
		checkNear( mesh.vertices[7], 2.0, "transformed v", 1e-4 );
	}

	void testStaticBakeWithNegativeScale() {
		const std::string json = withBuffer( std::string("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],") + TRIANGLE_ACCESSORS +
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
			"\"nodes\":[{\"mesh\":0,\"translation\":[10,0,0],\"scale\":[-1,1,1]}]}", triangleBlob() );
		const EngineMesh mesh = bakeForEngine( loadJson( json ) );
		const float* v = mesh.vertices.data();
		checkNear( v[0], 10.0, "baked translation" );
		/// The geometric normal of the baked triangle must agree with the vertex normal (winding flipped for the mirror).
		const float e1[3] = { v[8] - v[0], v[9] - v[1], v[10] - v[2] };
		const float e2[3] = { v[16] - v[0], v[17] - v[1], v[18] - v[2] };
		const float nz = e1[0] * e2[1] - e1[1] * e2[0];
		check( nz * v[5] > 0.0f, "winding is flipped for a negative scale" );
	}

	void testNodeAnimationWithoutSkin() {
		Blob blob = triangleBlob();
		const size_t times  = blob.add( std::vector<float>{ 0, 1 } );
		const size_t values = blob.add( std::vector<float>{ 0, 0, 0,  2, 0, 0 } );
		const std::string json = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],\"bufferViews\":["
			"{\"buffer\":0,\"byteLength\":36},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(times) + ",\"byteLength\":8},"
			"{\"buffer\":0,\"byteOffset\":" + std::to_string(values) + ",\"byteLength\":24}],"
			"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
			"{\"bufferView\":1,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\",\"min\":[0],\"max\":[1]},"
			"{\"bufferView\":2,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"}],"
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
			"\"nodes\":[{\"mesh\":0}],"
			"\"animations\":[{\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}],"
			"\"samplers\":[{\"input\":1,\"output\":2}]}]}";
		const Model model = loadJson( withBuffer( json, blob ) );
		checkNear( model.animations[0].endTime, 1.0, "animation end time" );

		const EngineMesh mesh = bakeForEngine( model );
		check( mesh.isAnimated && mesh.jointMatrices.size() == 1, "animated node becomes a rigid joint" );
		check( mesh.frameTimes.size() == 31, "1 second gap resampled at 30 frames per second, got " + std::to_string(mesh.frameTimes.size()) );
		checkNear( mesh.jointMatrices[0][15].m[12], 1.0, "joint translation at t = 0.5", 1e-4 );
		check( mesh.vertices[8] == 0.0f && mesh.vertices[12] == 1.0f, "vertices follow the rigid joint" );
	}

	void testStripsAndFans() {
		Blob blob;
		blob.add( std::vector<float>{ 0,0,0,  1,0,0,  0,1,0,  1,1,0,  0,2,0 } );
		const std::string base = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],\"bufferViews\":[{\"buffer\":0,\"byteLength\":60}],"
			"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":5,\"type\":\"VEC3\"}],"
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":MODE}]}],\"nodes\":[{\"mesh\":0}]}";
		for ( int mode : { 5, 6 } ) {
			std::string json = base;
			json.replace( json.find("MODE"), 4, std::to_string(mode) );
			const EngineMesh mesh = bakeForEngine( loadJson( withBuffer( json, blob ) ) );
			check( mesh.indices.size() == 9, (mode == 5 ? "strip" : "fan") + std::string(" gives 3 triangles") );
			if ( mode == 5 ) {
				/// Second strip triangle is (1, 3, 2): vertex 1 is (1,0,0), then (1,1,0)
				checkNear( mesh.vertices[3 * 8], 1.0, "strip triangle 2 first vertex x" );
				checkNear( mesh.vertices[4 * 8 + 1], 1.0, "strip triangle 2 second vertex y" );
			} else {
				checkNear( mesh.vertices[2 * 8], 0.0, "fan triangle ends with vertex 0" );
				checkNear( mesh.vertices[2 * 8 + 1], 0.0, "fan triangle ends with vertex 0" );
			}
		}
	}

	void testExternalFileWithPercentEncodedUri() {
		const std::filesystem::path directory = std::filesystem::temp_directory_path() / "glvm_gltf_test";
		std::filesystem::create_directories( directory );
		{
			const Blob blob = triangleBlob();
			std::ofstream file( directory / "my buffer.bin", std::ios::binary );
			file.write( (const char*)blob.bytes.data(), (std::streamsize)blob.bytes.size() );
		}
		const std::string json = std::string("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":36,\"uri\":\"my%20buffer.bin\"}],") +
			TRIANGLE_ACCESSORS + "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}";
		const Model model = loadJson( json, reinterpret_cast<const char*>(directory.u8string().c_str()) );
		check( readAccessorAsFloats( model, 0 )[3] == 1.0f, "buffer from a percent-encoded relative URI" );
		std::filesystem::remove_all( directory );
	}

	void testJsonUnicode() {
		const Model model = loadJson( "{\"asset\":{\"version\":\"2.0\",\"generator\":\"\\uD83D\\uDE00 \\u0436\"}}" );
		check( model.asset.generator == "\xF0\x9F\x98\x80 \xD0\xB6", "surrogate pairs and BMP escapes are UTF-8" );
	}

	void testErrors() {
		const Blob blob = triangleBlob();
		const std::string head = "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[$BUFFER],";
		expectFailure( "{\"asset\":{\"version\":\"1.0\"}}", "is not supported" );
		expectFailure( "{\"asset\":{\"version\":\"2.0\"},\"meshes\":[", "JSON parser" );
		expectFailure( "{\"asset\":{\"version\":\"2.0\"},\"extensionsRequired\":[\"KHR_draco_mesh_compression\"]}",
					   "required extensions are not supported: KHR_draco_mesh_compression" );
		expectFailure( withBuffer( head + "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":8,\"byteLength\":36}]}", blob ), "out of buffers[0]" );
		expectFailure( withBuffer( head + "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
								   "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"}]}", blob ), "out of bufferViews[0]" );
		expectFailure( withBuffer( head + "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
								   "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":1,\"type\":\"VEC5\"}]}", blob ), "is unknown" );
		expectFailure( "{\"asset\":{\"version\":\"2.0\"},\"nodes\":[{\"children\":[2]},{\"children\":[2]},{}]}", "more than one parent" );
		expectFailure( "{\"asset\":{\"version\":\"2.0\"},\"nodes\":[{\"children\":[1]},{\"children\":[0]}]}", "cycle" );
		expectFailure( "{\"asset\":{\"version\":\"2.0\"},\"nodes\":[{\"mesh\":0}]}", "references meshes[0], there are 0" );
		expectFailure( "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":4,\"uri\":\"http://example.com/a.bin\"}]}", "is not supported" );

		Blob indexed = triangleBlob();
		const size_t indices = indexed.add( std::vector<uint16_t>{ 0, 1, 5 } );
		expectFailure( withBuffer( head + "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36},{\"buffer\":0,\"byteOffset\":" +
								   std::to_string(indices) + ",\"byteLength\":6}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,"
								   "\"count\":3,\"type\":\"VEC3\"},{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
								   "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}]}", indexed ), "index 5 is out of range" );
	}

	/*
	  ===================================================
	  Files
	  ===================================================
	*/
	bool hasMessage( const std::vector<std::string>& messages, const std::string& part ) {
		for ( const std::string& message : messages ) {
			if ( message.find(part) != std::string::npos )
				return true;
		}
		return false;
	}

	void testFile( const std::filesystem::path& path ) {
		const std::string name = reinterpret_cast<const char*>(path.filename().u8string().c_str());
		currentTest = name;
		Model model;
		try {
			model = loadModel( reinterpret_cast<const char*>(path.u8string().c_str()) );
		} catch ( const std::exception& error ) {
			check( false, error.what() );
			return;
		}

		/// POSITION data must fit the accessor min / max.
		for ( const Mesh& mesh : model.meshes ) {
			for ( const Primitive& primitive : mesh.primitives ) {
				const uint32_t accessor = primitive.findAttribute( "POSITION" );
				if ( accessor == INVALID_INDEX || model.accessors[accessor].min.size() != 3 || model.accessors[accessor].max.size() != 3 )
					continue;
				const std::vector<float> positions = readAccessorAsFloats( model, accessor );
				double minimum[3] = { 1e30, 1e30, 1e30 }, maximum[3] = { -1e30, -1e30, -1e30 };
				for ( size_t i = 0; i < positions.size(); ++i ) {
					minimum[i % 3] = std::min( minimum[i % 3], (double)positions[i] );
					maximum[i % 3] = std::max( maximum[i % 3], (double)positions[i] );
				}
				for ( int c = 0; c < 3; ++c ) {
					const double tolerance = 1e-4 * std::max( 1.0, std::fabs( model.accessors[accessor].max[c] ) );
					checkNear( minimum[c], model.accessors[accessor].min[c], "POSITION min of accessor " + std::to_string(accessor), tolerance );
					checkNear( maximum[c], model.accessors[accessor].max[c], "POSITION max of accessor " + std::to_string(accessor), tolerance );
				}
			}
		}

		/// Sampling exactly at a key frame returns the key value.
		for ( const Animation& animation : model.animations ) {
			for ( const AnimationChannel& channel : animation.channels ) {
				const AnimationSampler& sampler = animation.samplers[channel.sampler];
				std::vector<float> value( sampler.valueSize );
				const uint32_t keyStride = sampler.valueSize * (sampler.interpolation == Interpolation::CUBICSPLINE ? 3 : 1);
				const uint32_t valueStart = sampler.interpolation == Interpolation::CUBICSPLINE ? sampler.valueSize : 0;
				bool isExact = true;
				for ( size_t k = 0; k < sampler.times.size(); ++k ) {
					sampleAnimationSampler( sampler, channel.path, sampler.times[k], value.data() );
					std::vector<float> expected( sampler.values.begin() + k * keyStride + valueStart,
												 sampler.values.begin() + k * keyStride + valueStart + sampler.valueSize );
					if ( channel.path == TargetPath::ROTATION ) {
						const float length = std::sqrt( expected[0] * expected[0] + expected[1] * expected[1] + expected[2] * expected[2] + expected[3] * expected[3] );
						for ( float& component : expected )
							component /= length;
					}
					for ( uint32_t c = 0; c < sampler.valueSize; ++c )
						isExact = isExact && std::fabs( value[c] - expected[c] ) <= 1e-4f * std::max( 1.0f, std::fabs(expected[c]) );
				}
				check( isExact, "key frame values of animation \"" + animation.name + "\"" );
			}
		}

		uint32_t decodedImages = 0;
		for ( const Image& image : model.images ) {
			if ( image.isDecoded ) {
				++decodedImages;
				check( image.pixels.size() == (size_t)image.width * image.height * 4, "RGBA pixel buffer size" );
			}
		}

		std::cout << "  " << name << ": meshes " << model.meshes.size() << ", nodes " << model.nodes.size() << ", skins " << model.skins.size()
				  << ", animations " << model.animations.size() << ", images " << decodedImages << "/" << model.images.size();
		try {
			const EngineMesh mesh = bakeForEngine( model );
			std::cout << " -> engine: " << mesh.indices.size() << " vertices, " << (mesh.isAnimated ? "animated" : "static")
					  << ", joints " << mesh.jointMatrices.size() << ", frames " << mesh.frameTimes.size() << ", topY " << mesh.topY << std::endl;
			check( mesh.indices.size() % 3 == 0 && mesh.vertices.size() == mesh.indices.size() * mesh.floatsPerVertex(), "engine vertex layout" );
			for ( float component : mesh.vertices ) {
				if ( !std::isfinite(component) ) {
					check( false, "non finite vertex data" );
					break;
				}
			}
			for ( const std::string& warning : mesh.warnings )
				std::cout << "      bake: " << warning << std::endl;
		} catch ( const std::exception& error ) {
			std::cout << " -> engine: " << error.what() << std::endl;
			check( hasMessage( { error.what() }, "no triangles" ) || hasMessage( { error.what() }, "no meshes" ), "unexpected bake error" );
		}
		for ( const std::string& warning : model.warnings )
			std::cout << "      load: " << warning << std::endl;
	}
}

int main( int argc, char** argv ) {
	std::cout << "glTF synthetic tests" << std::endl;
	runTest( "minimal triangle", testMinimalTriangle );
	runTest( "GLB", testGlb );
	runTest( "sparse accessors", testSparseAccessors );
	runTest( "matrix padding and normalization", testMatrixPaddingAndNormalization );
	runTest( "interleaved buffer view", testInterleavedBufferView );
	runTest( "animation sampling", testAnimationSampling );
	runTest( "hierarchy, skin, morph", testHierarchySkinAndMorph );
	runTest( "texture transform and lights", testTextureTransformAndLights );
	runTest( "static bake with negative scale", testStaticBakeWithNegativeScale );
	runTest( "node animation without skin", testNodeAnimationWithoutSkin );
	runTest( "strips and fans", testStripsAndFans );
	runTest( "percent-encoded URI", testExternalFileWithPercentEncodedUri );
	runTest( "JSON unicode", testJsonUnicode );
	runTest( "errors", testErrors );

	for ( int i = 1; i < argc; ++i ) {
		const std::filesystem::path argument = std::filesystem::path( argv[i] );
		std::cout << "glTF files in " << argv[i] << std::endl;
		std::vector<std::filesystem::path> files;
		if ( std::filesystem::is_directory( argument ) ) {
			for ( const auto& entry : std::filesystem::recursive_directory_iterator( argument ) ) {
				const std::string extension = entry.path().extension().string();
				if ( entry.is_regular_file() && (extension == ".gltf" || extension == ".glb") )
					files.push_back( entry.path() );
			}
		} else {
			files.push_back( argument );
		}
		std::sort( files.begin(), files.end() );
		for ( const std::filesystem::path& file : files )
			testFile( file );
	}

	std::cout << checksPassed << " checks passed, " << checksFailed << " failed" << std::endl;
	return checksFailed == 0 ? 0 : 1;
}
