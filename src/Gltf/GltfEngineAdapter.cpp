// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#include "Gltf/GltfEngineAdapter.hpp"
#include "GltfInternal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace GLVM::gltf
{
	namespace
	{
		struct UvTransform {
			bool  isEnabled = false;
			float m[6] = { 1, 0, 0, 1, 0, 0 };                                         ///< u' = m0 u + m2 v + m4, v' = m1 u + m3 v + m5

			/// KHR_texture_transform: translation * rotation * scale.
			static UvTransform from( const TextureInfo& info ) {
				UvTransform result;
				if ( !info.hasTransform )
					return result;
				const TextureTransform& transform = info.transform;
				const float cosine = std::cos( transform.rotation );
				const float sine   = std::sin( transform.rotation );
				result.isEnabled = true;
				result.m[0] =  cosine * transform.scale[0];
				result.m[1] =  sine   * transform.scale[0];
				result.m[2] = -sine   * transform.scale[1];
				result.m[3] =  cosine * transform.scale[1];
				result.m[4] =  transform.offset[0];
				result.m[5] =  transform.offset[1];
				return result;
			}

			void apply( float& u, float& v ) const {
				if ( !isEnabled )
					return;
				const float transformedU = m[0] * u + m[2] * v + m[4];
				const float transformedV = m[1] * u + m[3] * v + m[5];
				u = transformedU;
				v = transformedV;
			}
		};

		enum class JointMode { NONE, RIGID, SKIN };

		/// How vertices of one mesh node are written.
		struct NodeContext {
			uint32_t           node = INVALID_INDEX;
			JointMode          jointMode = JointMode::NONE;
			uint32_t           jointBase = 0;                                         ///< Rigid joint, or the first engine joint of the skin
			bool               bakeTransform = false;
			Mat4               transform;                                            ///< Baked world transform (static meshes)
			Mat4               normalMatrix;                                         ///< Inverse transpose of transform (column-major)
			bool               flipWinding = false;
			std::vector<float> morphWeights;
		};

		class Baker {
		public:
			Baker( const Model& model, const EngineBakeOptions& options, EngineMesh& result )
				: model_(model), options_(options), result_(result) {}

			void bake();

		private:
			const Model&             model_;
			const EngineBakeOptions& options_;
			EngineMesh&              result_;
			std::set<std::string>    reportedWarnings_;

			void warnOnce( const std::string& message ) {
				if ( reportedWarnings_.insert( message ).second )
					result_.warnings.push_back( message );
			}

			void emitPrimitive( const Primitive& primitive, const NodeContext& context, const std::string& where );
			std::vector<float> sampleTimes( uint32_t animation ) const;
		};

		void normalize3( float* vector ) {
			const float length = std::sqrt( vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2] );
			if ( length > 1e-20f ) {
				vector[0] /= length;
				vector[1] /= length;
				vector[2] /= length;
			} else {
				vector[0] = 0.0f;
				vector[1] = 1.0f;
				vector[2] = 0.0f;
			}
		}

		Mat4 normalMatrixOf( const Mat4& transform ) {
			const Mat4 inverse = transform.inverse();
			Mat4 result;
			for ( int column = 0; column < 4; ++column ) {
				for ( int row = 0; row < 4; ++row )
					result.m[column * 4 + row] = inverse.m[row * 4 + column];
			}
			return result;
		}

		void Baker::emitPrimitive( const Primitive& primitive, const NodeContext& context, const std::string& where ) {
			if ( primitive.findAttribute( "POSITION" ) == INVALID_INDEX ) {
				warnOnce( where + " has no POSITION and is skipped" );
				return;
			}
			if ( primitive.mode != PrimitiveMode::TRIANGLES && primitive.mode != PrimitiveMode::TRIANGLE_STRIP &&
				 primitive.mode != PrimitiveMode::TRIANGLE_FAN ) {
				warnOnce( where + " is points or lines, the engine draws triangles only, it is skipped" );
				return;
			}

			const std::vector<float> positions = morphedAttribute( model_, primitive, "POSITION", context.morphWeights );
			const std::vector<float> normals   = morphedAttribute( model_, primitive, "NORMAL", context.morphWeights );
			const uint32_t vertexCount = (uint32_t)(positions.size() / 3);

			/// UVs of the base color texture (its texCoord set and transform), TEXCOORD_0 otherwise.
			const Material* material = primitive.material != INVALID_INDEX ? &model_.materials[primitive.material] : nullptr;
			uint32_t uvSet = 0;
			UvTransform uvTransform;
			if ( material != nullptr && material->baseColorTexture.isSet() ) {
				uvSet       = material->baseColorTexture.usedTexCoord();
				uvTransform = UvTransform::from( material->baseColorTexture );
			}
			std::vector<float> uvs = morphedAttribute( model_, primitive, "TEXCOORD_" + std::to_string(uvSet), context.morphWeights );
			if ( uvs.empty() && uvSet != 0 ) {
				warnOnce( where + " has no TEXCOORD_" + std::to_string(uvSet) + ", TEXCOORD_0 is used" );
				uvs = morphedAttribute( model_, primitive, "TEXCOORD_0", context.morphWeights );
				uvTransform = UvTransform();
			}

			/// Skin influences of all JOINTS_n / WEIGHTS_n sets, the 4 strongest are kept.
			std::vector<std::vector<uint32_t>> jointSets;
			std::vector<std::vector<float>>    weightSets;
			if ( context.jointMode == JointMode::SKIN ) {
				for ( uint32_t set = 0; ; ++set ) {
					const uint32_t jointsAccessor  = primitive.findAttribute( "JOINTS_" + std::to_string(set) );
					const uint32_t weightsAccessor = primitive.findAttribute( "WEIGHTS_" + std::to_string(set) );
					if ( jointsAccessor == INVALID_INDEX || weightsAccessor == INVALID_INDEX )
						break;
					jointSets.push_back( readAccessorAsUints( model_, jointsAccessor ) );
					weightSets.push_back( readAccessorAsFloats( model_, weightsAccessor ) );
				}
				if ( jointSets.empty() )
					warnOnce( where + " is skinned but has no JOINTS_0 / WEIGHTS_0, it follows the first joint" );
				if ( jointSets.size() > 1 )
					warnOnce( where + " has more than 4 joint influences per vertex, the 4 strongest are used" );
			}

			std::vector<uint32_t> indices;
			if ( primitive.indices != INVALID_INDEX ) {
				indices = readAccessorAsUints( model_, primitive.indices );
			} else {
				indices.resize( vertexCount );
				for ( uint32_t i = 0; i < vertexCount; ++i )
					indices[i] = i;
			}

			std::vector<uint32_t> triangles;                                           ///< Vertex indices, 3 per triangle
			const size_t indicesNumber = indices.size();
			if ( primitive.mode == PrimitiveMode::TRIANGLES ) {
				for ( size_t i = 0; i + 2 < indicesNumber; i += 3 )
					triangles.insert( triangles.end(), { indices[i], indices[i + 1], indices[i + 2] } );
			} else if ( primitive.mode == PrimitiveMode::TRIANGLE_STRIP ) {
				for ( size_t i = 0; i + 2 < indicesNumber; ++i ) {                     ///< Every second triangle is reversed to keep the winding
					if ( i % 2 == 0 )
						triangles.insert( triangles.end(), { indices[i], indices[i + 1], indices[i + 2] } );
					else
						triangles.insert( triangles.end(), { indices[i], indices[i + 2], indices[i + 1] } );
				}
			} else {
				for ( size_t i = 0; i + 2 < indicesNumber; ++i )
					triangles.insert( triangles.end(), { indices[i + 1], indices[i + 2], indices[0] } );
			}

			for ( size_t t = 0; t + 2 < triangles.size(); t += 3 ) {
				const uint32_t corners[3] = { triangles[t], triangles[t + 1], triangles[t + 2] };

				/// Flat normal in local space from the original winding: missing normals and the skinned path use it as is.
				float flatNormal[3] = { 0.0f, 1.0f, 0.0f };
				if ( normals.empty() ) {
					const float* p0 = &positions[corners[0] * 3];
					const float* p1 = &positions[corners[1] * 3];
					const float* p2 = &positions[corners[2] * 3];
					const float edge1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
					const float edge2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
					flatNormal[0] = edge1[1] * edge2[2] - edge1[2] * edge2[1];
					flatNormal[1] = edge1[2] * edge2[0] - edge1[0] * edge2[2];
					flatNormal[2] = edge1[0] * edge2[1] - edge1[1] * edge2[0];
					normalize3( flatNormal );
				}

				const int order[3] = { 0, context.flipWinding ? 2 : 1, context.flipWinding ? 1 : 2 };
				for ( int corner : order ) {
					const uint32_t vertex = corners[corner];

					float position[3] = { positions[vertex * 3], positions[vertex * 3 + 1], positions[vertex * 3 + 2] };
					float normal[3]   = { flatNormal[0], flatNormal[1], flatNormal[2] };
					if ( !normals.empty() ) {
						normal[0] = normals[vertex * 3];
						normal[1] = normals[vertex * 3 + 1];
						normal[2] = normals[vertex * 3 + 2];
					}
					if ( context.bakeTransform ) {
						context.transform.transformPoint( position, position );
						context.normalMatrix.transformDirection( normal, normal );
					}
					normalize3( normal );

					float u = 0.0f;
					float v = 0.0f;
					if ( !uvs.empty() ) {
						u = uvs[vertex * 2];
						v = uvs[vertex * 2 + 1];
						uvTransform.apply( u, v );
					}

					result_.indices.push_back( (uint32_t)(result_.vertices.size() / result_.floatsPerVertex()) );
					result_.vertices.insert( result_.vertices.end(), { position[0], position[1], position[2],
																	   normal[0], normal[1], normal[2], u, v } );
					if ( !result_.isAnimated )
						continue;

					float joints[4]  = { (float)context.jointBase, (float)context.jointBase, (float)context.jointBase, (float)context.jointBase };
					float weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
					if ( context.jointMode == JointMode::SKIN && !jointSets.empty() ) {
						std::pair<float, uint32_t> influences[4] = {};                     ///< (weight, joint), strongest first
						for ( size_t set = 0; set < jointSets.size(); ++set ) {
							for ( int k = 0; k < 4; ++k ) {
								const float weight = weightSets[set][vertex * 4 + k];
								if ( !(weight > 0.0f) )
									continue;
								std::pair<float, uint32_t> candidate( weight, jointSets[set][vertex * 4 + k] );
								for ( int slot = 0; slot < 4; ++slot ) {
									if ( candidate.first > influences[slot].first )
										std::swap( candidate, influences[slot] );
								}
							}
						}
						const float sum = influences[0].first + influences[1].first + influences[2].first + influences[3].first;
						if ( sum > 0.0f ) {
							for ( int k = 0; k < 4; ++k ) {
								const bool isUsed = influences[k].first > 0.0f;
								joints[k]  = (float)(context.jointBase + (isUsed ? influences[k].second : influences[0].second));
								weights[k] = influences[k].first / sum;
							}
						}
					}
					/// The shader can't index joints beyond the engine limit: their influence is dropped.
					bool isDropped = false;
					for ( int k = 0; k < 4; ++k ) {
						if ( (uint32_t)joints[k] >= options_.maxJoints ) {
							joints[k]  = 0.0f;
							isDropped  = isDropped || weights[k] > 0.0f;
							weights[k] = 0.0f;
						}
					}
					if ( isDropped ) {
						const float kept = weights[0] + weights[1] + weights[2] + weights[3];
						for ( int k = 0; k < 4; ++k )
							weights[k] = kept > 0.0f ? weights[k] / kept : (k == 0 ? 1.0f : 0.0f);
					}
					result_.vertices.insert( result_.vertices.end(), { joints[0], joints[1], joints[2], joints[3],
																	   weights[0], weights[1], weights[2], weights[3] } );
				}
			}
		}

		/// Union of the key frame times of all channels; gaps longer than subdivideGap are resampled with maxFrameStep.
		std::vector<float> Baker::sampleTimes( uint32_t animation ) const {
			std::vector<float> keyTimes;
			const Animation& data = model_.animations[animation];
			for ( const AnimationChannel& channel : data.channels ) {
				const std::vector<float>& times = data.samplers[channel.sampler].times;
				keyTimes.insert( keyTimes.end(), times.begin(), times.end() );
			}
			std::sort( keyTimes.begin(), keyTimes.end() );

			std::vector<float> result;
			for ( float time : keyTimes ) {
				if ( !result.empty() && time - result.back() < 1e-5f )
					continue;
				if ( !result.empty() && time - result.back() > options_.subdivideGap && options_.maxFrameStep > 0.0f ) {
					const float start = result.back();
					const uint32_t steps = (uint32_t)std::ceil( (time - start) / options_.maxFrameStep );
					for ( uint32_t step = 1; step < steps; ++step )
						result.push_back( start + (time - start) * (float)step / (float)steps );
				}
				result.push_back( time );
			}
			if ( result.empty() )
				result.push_back( 0.0f );
			return result;
		}

		void Baker::bake() {
			/// Mesh nodes of the scene in hierarchy order.
			std::vector<uint32_t> drawNodes;
			std::vector<uint32_t> stack = model_.sceneRootNodes();
			std::reverse( stack.begin(), stack.end() );
			while ( !stack.empty() ) {
				const uint32_t node = stack.back();
				stack.pop_back();
				if ( model_.nodes[node].mesh != INVALID_INDEX )
					drawNodes.push_back( node );
				const std::vector<uint32_t>& children = model_.nodes[node].children;
				for ( auto child = children.rbegin(); child != children.rend(); ++child )
					stack.push_back( *child );
			}
			if ( drawNodes.empty() )
				detail::fail( model_.filePath, "the scene has no meshes" );

			uint32_t animation = INVALID_INDEX;
			if ( !model_.animations.empty() ) {
				animation = options_.animation;
				if ( animation >= model_.animations.size() ) {
					warnOnce( "animations[" + std::to_string(animation) + "] doesn't exist, animations[0] is used" );
					animation = 0;
				}
				if ( model_.animations.size() > 1 )
					warnOnce( "the model has " + std::to_string(model_.animations.size()) + " animations, animations[" +
							  std::to_string(animation) + "] \"" + model_.animations[animation].name + "\" is used" );
			}

			/// Nodes moved by the animation: animated TRS of the node or of any ancestor.
			std::vector<bool> isMoved( model_.nodes.size(), false );
			std::vector<bool> hasWeightsAnimation( model_.nodes.size(), false );
			if ( animation != INVALID_INDEX ) {
				for ( const AnimationChannel& channel : model_.animations[animation].channels ) {
					if ( channel.path == TargetPath::WEIGHTS )
						hasWeightsAnimation[channel.node] = true;
					else
						isMoved[channel.node] = true;
				}
			}
			std::vector<bool> isMovedWithAncestors( model_.nodes.size(), false );
			for ( uint32_t i = 0; i < model_.nodes.size(); ++i ) {
				for ( uint32_t current = i; current != INVALID_INDEX; current = model_.nodes[current].parent ) {
					if ( isMoved[current] ) {
						isMovedWithAncestors[i] = true;
						break;
					}
				}
			}

			bool isAnimated = false;
			for ( uint32_t node : drawNodes ) {
				isAnimated = isAnimated || model_.nodes[node].skin != INVALID_INDEX || isMovedWithAncestors[node];
				if ( hasWeightsAnimation[node] )
					warnOnce( "morph target animation of nodes[" + std::to_string(node) + "] can't be shown by the engine, default weights are used" );
			}
			result_.isAnimated = isAnimated;

			const Pose rest = restPose( model_ );
			const std::vector<Mat4> restWorld = worldTransforms( model_, rest );

			/// Engine joints: all joints of every used skin, then one rigid joint per mesh node without a skin.
			struct EngineJoint {
				uint32_t skin;                                                         ///< INVALID_INDEX - rigid joint of a mesh node
				uint32_t index;                                                        ///< Joint index in the skin
				uint32_t node;
			};
			std::vector<EngineJoint> joints;
			std::vector<uint32_t> skinBase( model_.skins.size(), INVALID_INDEX );
			std::vector<std::vector<Mat4>> inverseBinds( model_.skins.size() );
			std::vector<NodeContext> contexts;
			for ( uint32_t node : drawNodes ) {
				NodeContext context;
				context.node = node;
				context.morphWeights = morphWeights( model_, rest, node );
				const uint32_t skin = model_.nodes[node].skin;
				if ( !isAnimated ) {
					context.bakeTransform = true;
					context.transform     = restWorld[node];
					context.normalMatrix  = normalMatrixOf( restWorld[node] );
					context.flipWinding   = restWorld[node].determinant3x3() < 0.0f;
				} else if ( skin != INVALID_INDEX ) {
					if ( skinBase[skin] == INVALID_INDEX ) {
						skinBase[skin] = (uint32_t)joints.size();
						inverseBinds[skin] = inverseBindMatrices( model_, skin );
						for ( uint32_t j = 0; j < model_.skins[skin].joints.size(); ++j )
							joints.push_back( { skin, j, model_.skins[skin].joints[j] } );
					}
					context.jointMode = JointMode::SKIN;
					context.jointBase = skinBase[skin];
				} else {
					context.jointMode   = JointMode::RIGID;
					context.jointBase   = (uint32_t)joints.size();
					context.flipWinding = restWorld[node].determinant3x3() < 0.0f;
					joints.push_back( { INVALID_INDEX, 0, node } );
				}
				contexts.push_back( std::move(context) );
			}
			if ( joints.size() > options_.maxJoints )
				warnOnce( "the model needs " + std::to_string(joints.size()) + " joints, the engine supports " +
						  std::to_string(options_.maxJoints) + ", extra joints are not animated" );

			for ( const NodeContext& context : contexts ) {
				const Mesh& mesh = model_.meshes[model_.nodes[context.node].mesh];
				for ( uint32_t p = 0; p < mesh.primitives.size(); ++p )
					emitPrimitive( mesh.primitives[p], context, "meshes[" + std::to_string(model_.nodes[context.node].mesh) +
								   "].primitives[" + std::to_string(p) + "]" );
			}
			if ( result_.vertices.empty() )
				detail::fail( model_.filePath, "the scene has no triangles" );

			const uint32_t floatsPerVertex = result_.floatsPerVertex();
			if ( !isAnimated ) {
				result_.topY = -std::numeric_limits<float>::max();
				for ( size_t v = 0; v < result_.vertices.size(); v += floatsPerVertex )
					result_.topY = std::max( result_.topY, result_.vertices[v + 1] );
				return;
			}

			/// Joint matrices at every sample time.
			result_.frameTimes = animation != INVALID_INDEX ? sampleTimes( animation ) : std::vector<float>{ 0.0f };
			result_.jointMatrices.assign( joints.size(), {} );
			for ( std::vector<Mat4>& frames : result_.jointMatrices )
				frames.reserve( result_.frameTimes.size() );
			for ( float time : result_.frameTimes ) {
				Pose pose = rest;
				if ( animation != INVALID_INDEX )
					applyAnimation( model_, animation, time, pose );
				const std::vector<Mat4> world = worldTransforms( model_, pose );
				for ( size_t j = 0; j < joints.size(); ++j ) {
					const EngineJoint& joint = joints[j];
					result_.jointMatrices[j].push_back( joint.skin == INVALID_INDEX ? world[joint.node]
														: world[joint.node] * inverseBinds[joint.skin][joint.index] );
				}
			}

			/// Highest point in the bind pose: skinned vertices as stored, rigid joint vertices placed by their node.
			result_.topY = -std::numeric_limits<float>::max();
			for ( size_t v = 0; v < result_.vertices.size(); v += floatsPerVertex ) {
				const float* vertex = &result_.vertices[v];
				const uint32_t joint = (uint32_t)vertex[8];
				float position[3] = { vertex[0], vertex[1], vertex[2] };
				if ( joint < joints.size() && joints[joint].skin == INVALID_INDEX )
					restWorld[joints[joint].node].transformPoint( position, position );
				result_.topY = std::max( result_.topY, position[1] );
			}
		}
	}

	EngineMesh bakeForEngine( const Model& model, const EngineBakeOptions& options ) {
		EngineMesh result;
		Baker baker( model, options, result );
		baker.bake();
		return result;
	}
}
