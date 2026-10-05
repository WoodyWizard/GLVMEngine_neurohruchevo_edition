// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/// Scene graph evaluation for the glTF loader: transforms, animation sampling, skinning matrices, morph targets.

#include "Gltf/Gltf.hpp"
#include "GltfInternal.hpp"

#include <algorithm>
#include <cmath>

namespace GLVM::gltf
{
	/*
	  ===================================================
	  Matrices (column-major, element (row r, column c) is m[c * 4 + r])
	  ===================================================
	*/
	Mat4 Mat4::operator*( const Mat4& other ) const {
		Mat4 result;
		for ( int column = 0; column < 4; ++column ) {
			for ( int row = 0; row < 4; ++row ) {
				float sum = 0.0f;
				for ( int k = 0; k < 4; ++k )
					sum += m[k * 4 + row] * other.m[column * 4 + k];
				result.m[column * 4 + row] = sum;
			}
		}
		return result;
	}

	void Mat4::transformPoint( const float in[3], float out[3] ) const {
		float result[3];
		for ( int row = 0; row < 3; ++row )
			result[row] = m[row] * in[0] + m[4 + row] * in[1] + m[8 + row] * in[2] + m[12 + row];
		out[0] = result[0];
		out[1] = result[1];
		out[2] = result[2];
	}

	void Mat4::transformDirection( const float in[3], float out[3] ) const {
		float result[3];
		for ( int row = 0; row < 3; ++row )
			result[row] = m[row] * in[0] + m[4 + row] * in[1] + m[8 + row] * in[2];
		out[0] = result[0];
		out[1] = result[1];
		out[2] = result[2];
	}

	float Mat4::determinant3x3() const {
		return m[0] * (m[5] * m[10] - m[9] * m[6]) -
			   m[4] * (m[1] * m[10] - m[9] * m[2]) +
			   m[8] * (m[1] * m[6]  - m[5] * m[2]);
	}

	Mat4 Mat4::inverse() const {
		const float* a = m;
		float inv[16];
		inv[0]  =  a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
		inv[4]  = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
		inv[8]  =  a[4] * a[9]  * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
		inv[12] = -a[4] * a[9]  * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
		inv[1]  = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
		inv[5]  =  a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
		inv[9]  = -a[0] * a[9]  * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
		inv[13] =  a[0] * a[9]  * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
		inv[2]  =  a[1] * a[6]  * a[15] - a[1] * a[7]  * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7]  - a[13] * a[3] * a[6];
		inv[6]  = -a[0] * a[6]  * a[15] + a[0] * a[7]  * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7]  + a[12] * a[3] * a[6];
		inv[10] =  a[0] * a[5]  * a[15] - a[0] * a[7]  * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7]  - a[12] * a[3] * a[5];
		inv[14] = -a[0] * a[5]  * a[14] + a[0] * a[6]  * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6]  + a[12] * a[2] * a[5];
		inv[3]  = -a[1] * a[6]  * a[11] + a[1] * a[7]  * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9]  * a[2] * a[7]  + a[9]  * a[3] * a[6];
		inv[7]  =  a[0] * a[6]  * a[11] - a[0] * a[7]  * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8]  * a[2] * a[7]  - a[8]  * a[3] * a[6];
		inv[11] = -a[0] * a[5]  * a[11] + a[0] * a[7]  * a[9]  + a[4] * a[1] * a[11] - a[4] * a[3] * a[9]  - a[8]  * a[1] * a[7]  + a[8]  * a[3] * a[5];
		inv[15] =  a[0] * a[5]  * a[10] - a[0] * a[6]  * a[9]  - a[4] * a[1] * a[10] + a[4] * a[2] * a[9]  + a[8]  * a[1] * a[6]  - a[8]  * a[2] * a[5];

		const float determinant = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
		Mat4 result;
		if ( determinant == 0.0f || !std::isfinite(determinant) )
			return result;
		for ( int i = 0; i < 16; ++i )
			result.m[i] = inv[i] / determinant;
		return result;
	}

	Mat4 Mat4::fromTRS( const float translation[3], const float rotation[4], const float scale[3] ) {
		const float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
		const float rotationMatrix[3][3] = {                                          ///< [row][column]
			{ 1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y - z * w),        2.0f * (x * z + y * w) },
			{ 2.0f * (x * y + z * w),        1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z - x * w) },
			{ 2.0f * (x * z - y * w),        2.0f * (y * z + x * w),        1.0f - 2.0f * (x * x + y * y) }
		};
		Mat4 result;
		for ( int column = 0; column < 3; ++column ) {
			for ( int row = 0; row < 3; ++row )
				result.m[column * 4 + row] = rotationMatrix[row][column] * scale[column];   ///< T * R * S
			result.m[column * 4 + 3] = 0.0f;
		}
		result.m[12] = translation[0];
		result.m[13] = translation[1];
		result.m[14] = translation[2];
		result.m[15] = 1.0f;
		return result;
	}

	/*
	  ===================================================
	  Animation sampling
	  ===================================================
	*/
	namespace
	{
		void normalizeQuaternion( float* quaternion ) {
			const float length = std::sqrt( quaternion[0] * quaternion[0] + quaternion[1] * quaternion[1] +
											quaternion[2] * quaternion[2] + quaternion[3] * quaternion[3] );
			if ( length > 0.0f ) {
				for ( int i = 0; i < 4; ++i )
					quaternion[i] /= length;
			} else {
				quaternion[0] = quaternion[1] = quaternion[2] = 0.0f;
				quaternion[3] = 1.0f;
			}
		}

		/// Spherical linear interpolation along the shortest arc.
		void slerp( const float* from, const float* to, float t, float* output ) {
			float target[4] = { to[0], to[1], to[2], to[3] };
			float cosine = from[0] * to[0] + from[1] * to[1] + from[2] * to[2] + from[3] * to[3];
			if ( cosine < 0.0f ) {
				cosine = -cosine;
				for ( float& component : target )
					component = -component;
			}

			float fromWeight = 1.0f - t;
			float toWeight   = t;
			if ( cosine < 0.9995f ) {                                                  ///< Nearly parallel quaternions are lerped
				const float angle = std::acos( std::min( cosine, 1.0f ) );
				const float sine  = std::sin( angle );
				fromWeight = std::sin( (1.0f - t) * angle ) / sine;
				toWeight   = std::sin( t * angle ) / sine;
			}
			for ( int i = 0; i < 4; ++i )
				output[i] = fromWeight * from[i] + toWeight * target[i];
			normalizeQuaternion( output );
		}
	}

	void sampleAnimationSampler( const AnimationSampler& sampler, TargetPath path, float time, float* output ) {
		const uint32_t valueSize = sampler.valueSize;
		const size_t keys = sampler.times.size();
		if ( valueSize == 0 || keys == 0 )
			return;

		const bool isCubic = sampler.interpolation == Interpolation::CUBICSPLINE;
		const size_t keyStride  = isCubic ? 3 * (size_t)valueSize : valueSize;         ///< Cubic keys are (in tangent, value, out tangent)
		const size_t valueStart = isCubic ? valueSize : 0;
		const float* values = sampler.values.data();
		const bool isRotation = path == TargetPath::ROTATION;

		auto copyKey = [&]( size_t key ) {
			for ( uint32_t i = 0; i < valueSize; ++i )
				output[i] = values[key * keyStride + valueStart + i];
			if ( isRotation )
				normalizeQuaternion( output );
		};

		if ( keys == 1 || time <= sampler.times.front() ) {
			copyKey( 0 );
			return;
		}
		if ( time >= sampler.times.back() ) {
			copyKey( keys - 1 );
			return;
		}

		const size_t next = (size_t)(std::upper_bound( sampler.times.begin(), sampler.times.end(), time ) - sampler.times.begin());
		const size_t key = next - 1;
		const float keyDuration = sampler.times[next] - sampler.times[key];
		const float t = (time - sampler.times[key]) / keyDuration;

		switch ( sampler.interpolation ) {
		case Interpolation::STEP:
			copyKey( key );
			return;

		case Interpolation::LINEAR:
			if ( isRotation ) {
				slerp( values + key * keyStride, values + next * keyStride, t, output );
			} else {
				for ( uint32_t i = 0; i < valueSize; ++i )
					output[i] = values[key * keyStride + i] * (1.0f - t) + values[next * keyStride + i] * t;
			}
			return;

		case Interpolation::CUBICSPLINE: {
			/// Hermite spline: p(t) = (2t^3 - 3t^2 + 1) v0 + (t^3 - 2t^2 + t) d b0 + (-2t^3 + 3t^2) v1 + (t^3 - t^2) d a1
			const float t2 = t * t;
			const float t3 = t2 * t;
			const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
			const float h10 = (t3 - 2.0f * t2 + t) * keyDuration;
			const float h01 = -2.0f * t3 + 3.0f * t2;
			const float h11 = (t3 - t2) * keyDuration;
			for ( uint32_t i = 0; i < valueSize; ++i ) {
				const float value0      = values[key * keyStride + valueSize + i];
				const float outTangent0 = values[key * keyStride + 2 * valueSize + i];
				const float inTangent1  = values[next * keyStride + i];
				const float value1      = values[next * keyStride + valueSize + i];
				output[i] = h00 * value0 + h10 * outTangent0 + h01 * value1 + h11 * inTangent1;
			}
			if ( isRotation )
				normalizeQuaternion( output );
			return;
		}
		}
	}

	/*
	  ===================================================
	  Poses and transforms
	  ===================================================
	*/
	Pose restPose( const Model& model ) {
		Pose pose;
		const size_t nodesNumber = model.nodes.size();
		pose.translations.resize( nodesNumber * 3 );
		pose.rotations.resize( nodesNumber * 4 );
		pose.scales.resize( nodesNumber * 3 );
		pose.weights.resize( nodesNumber );
		pose.animated.assign( nodesNumber, false );
		for ( size_t i = 0; i < nodesNumber; ++i ) {
			const Node& node = model.nodes[i];
			std::copy( node.translation, node.translation + 3, pose.translations.begin() + i * 3 );
			std::copy( node.rotation, node.rotation + 4, pose.rotations.begin() + i * 4 );
			std::copy( node.scale, node.scale + 3, pose.scales.begin() + i * 3 );
		}
		return pose;
	}

	void applyAnimation( const Model& model, uint32_t animationIndex, float time, Pose& pose ) {
		if ( animationIndex >= model.animations.size() )
			detail::fail( model.filePath, "animations[" + std::to_string(animationIndex) + "] doesn't exist" );
		const Animation& animation = model.animations[animationIndex];
		for ( const AnimationChannel& channel : animation.channels ) {
			if ( channel.node >= model.nodes.size() || channel.sampler >= animation.samplers.size() )
				continue;
			const AnimationSampler& sampler = animation.samplers[channel.sampler];
			switch ( channel.path ) {
			case TargetPath::TRANSLATION:
				sampleAnimationSampler( sampler, channel.path, time, &pose.translations[channel.node * 3] );
				pose.animated[channel.node] = true;
				break;
			case TargetPath::ROTATION:
				sampleAnimationSampler( sampler, channel.path, time, &pose.rotations[channel.node * 4] );
				pose.animated[channel.node] = true;
				break;
			case TargetPath::SCALE:
				sampleAnimationSampler( sampler, channel.path, time, &pose.scales[channel.node * 3] );
				pose.animated[channel.node] = true;
				break;
			case TargetPath::WEIGHTS:
				pose.weights[channel.node].resize( sampler.valueSize );
				sampleAnimationSampler( sampler, channel.path, time, pose.weights[channel.node].data() );
				break;
			}
		}
	}

	Mat4 localTransform( const Model& model, const Pose& pose, uint32_t node ) {
		if ( model.nodes[node].hasMatrix ) {
			Mat4 result;
			std::copy( model.nodes[node].matrix, model.nodes[node].matrix + 16, result.m );
			return result;
		}
		return Mat4::fromTRS( &pose.translations[node * 3], &pose.rotations[node * 4], &pose.scales[node * 3] );
	}

	std::vector<Mat4> worldTransforms( const Model& model, const Pose& pose ) {
		std::vector<Mat4> world( model.nodes.size() );
		std::vector<uint32_t> stack;
		for ( uint32_t i = 0; i < model.nodes.size(); ++i ) {
			if ( model.nodes[i].parent == INVALID_INDEX ) {
				world[i] = localTransform( model, pose, i );
				stack.push_back( i );
			}
		}
		while ( !stack.empty() ) {                                                   ///< Parents are always computed before children
			const uint32_t parent = stack.back();
			stack.pop_back();
			for ( uint32_t child : model.nodes[parent].children ) {
				world[child] = world[parent] * localTransform( model, pose, child );
				stack.push_back( child );
			}
		}
		return world;
	}

	std::vector<Mat4> inverseBindMatrices( const Model& model, uint32_t skin ) {
		if ( skin >= model.skins.size() )
			detail::fail( model.filePath, "skins[" + std::to_string(skin) + "] doesn't exist" );
		const Skin& data = model.skins[skin];
		std::vector<Mat4> matrices( data.joints.size() );
		if ( data.inverseBindMatrices == INVALID_INDEX )
			return matrices;
		const std::vector<float> values = readAccessorAsFloats( model, data.inverseBindMatrices );
		for ( size_t i = 0; i < matrices.size() && (i + 1) * 16 <= values.size(); ++i )
			std::copy( values.begin() + i * 16, values.begin() + (i + 1) * 16, matrices[i].m );
		return matrices;
	}

	std::vector<Mat4> jointMatrices( const Model& model, uint32_t skin, const std::vector<Mat4>& worldTransforms ) {
		std::vector<Mat4> matrices = inverseBindMatrices( model, skin );
		const Skin& data = model.skins[skin];
		for ( size_t i = 0; i < data.joints.size(); ++i )
			matrices[i] = worldTransforms[data.joints[i]] * matrices[i];
		return matrices;
	}

	std::vector<float> morphWeights( const Model& model, const Pose& pose, uint32_t node ) {
		const Node& data = model.nodes[node];
		if ( data.mesh == INVALID_INDEX )
			return {};
		const Mesh& mesh = model.meshes[data.mesh];
		const size_t targetsNumber = mesh.primitives.empty() ? 0 : mesh.primitives[0].targets.size();
		std::vector<float> weights;
		if ( node < pose.weights.size() && !pose.weights[node].empty() )
			weights = pose.weights[node];
		else if ( !data.weights.empty() )
			weights = data.weights;
		else
			weights = mesh.weights;
		weights.resize( targetsNumber, 0.0f );
		return weights;
	}

	std::vector<float> morphedAttribute( const Model& model, const Primitive& primitive, const std::string& attributeName,
										 const std::vector<float>& weights ) {
		const uint32_t accessor = primitive.findAttribute( attributeName );
		if ( accessor == INVALID_INDEX )
			return {};
		std::vector<float> values = readAccessorAsFloats( model, accessor );
		for ( size_t t = 0; t < primitive.targets.size() && t < weights.size(); ++t ) {
			if ( weights[t] == 0.0f )
				continue;
			const uint32_t targetAccessor = primitive.targets[t].findAttribute( attributeName );
			if ( targetAccessor == INVALID_INDEX )
				continue;
			const std::vector<float> displacements = readAccessorAsFloats( model, targetAccessor );
			if ( displacements.size() != values.size() )
				detail::fail( model.filePath, "morph target " + std::to_string(t) + " attribute " + attributeName +
							  " has a different size than the base attribute" );
			for ( size_t i = 0; i < values.size(); ++i )
				values[i] += weights[t] * displacements[i];
		}
		return values;
	}
}
