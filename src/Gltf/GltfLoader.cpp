// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#include "Gltf/Gltf.hpp"
#include "GltfInternal.hpp"
#include "JsonParser.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace GLVM::gltf
{
	using Core::JsonValue;

	namespace detail
	{
		void fail( const std::string& source, const std::string& message ) {
			throw std::runtime_error( "glTF loader: " + source + ": " + message );
		}

		uint32_t elementByteSize( ComponentType componentType, AccessorType type ) {
			const uint32_t componentSize = componentTypeSize( componentType );
			uint32_t rows = 0;
			switch ( type ) {
			case AccessorType::MAT2: rows = 2; break;
			case AccessorType::MAT3: rows = 3; break;
			case AccessorType::MAT4: rows = 4; break;
			default:
				return componentsNumber( type ) * componentSize;
			}
			const uint32_t columnStride = (rows * componentSize + 3u) & ~3u;                ///< Matrix columns start on 4 byte boundaries
			return rows * columnStride;
		}

		uint32_t componentByteOffset( ComponentType componentType, AccessorType type, uint32_t component ) {
			const uint32_t componentSize = componentTypeSize( componentType );
			uint32_t rows = 0;
			switch ( type ) {
			case AccessorType::MAT2: rows = 2; break;
			case AccessorType::MAT3: rows = 3; break;
			case AccessorType::MAT4: rows = 4; break;
			default:
				return component * componentSize;
			}
			const uint32_t columnStride = (rows * componentSize + 3u) & ~3u;
			return (component / rows) * columnStride + (component % rows) * componentSize;
		}
	}

	uint32_t componentTypeSize( ComponentType componentType ) {
		switch ( componentType ) {
		case ComponentType::BYTE:
		case ComponentType::UNSIGNED_BYTE:  return 1;
		case ComponentType::SHORT:
		case ComponentType::UNSIGNED_SHORT: return 2;
		case ComponentType::UNSIGNED_INT:
		case ComponentType::FLOAT:          return 4;
		}
		return 0;
	}

	uint32_t componentsNumber( AccessorType type ) {
		switch ( type ) {
		case AccessorType::SCALAR: return 1;
		case AccessorType::VEC2:   return 2;
		case AccessorType::VEC3:   return 3;
		case AccessorType::VEC4:   return 4;
		case AccessorType::MAT2:   return 4;
		case AccessorType::MAT3:   return 9;
		case AccessorType::MAT4:   return 16;
		}
		return 0;
	}

	namespace
	{
		uint32_t findAttributeIn( const std::vector<Attribute>& attributes, const std::string& attributeName ) {
			for ( const Attribute& attribute : attributes ) {
				if ( attribute.name == attributeName )
					return attribute.accessor;
			}
			return INVALID_INDEX;
		}
	}

	uint32_t Primitive::findAttribute( const std::string& attributeName ) const {
		return findAttributeIn( attributes, attributeName );
	}

	uint32_t MorphTarget::findAttribute( const std::string& attributeName ) const {
		return findAttributeIn( attributes, attributeName );
	}

	std::vector<uint32_t> Model::sceneRootNodes() const {
		if ( scene != INVALID_INDEX && scene < scenes.size() )
			return scenes[scene].nodes;
		if ( !scenes.empty() )
			return scenes[0].nodes;

		std::vector<uint32_t> roots;
		for ( uint32_t i = 0; i < nodes.size(); ++i ) {
			if ( nodes[i].parent == INVALID_INDEX )
				roots.push_back(i);
		}
		return roots;
	}

	namespace
	{
		constexpr uint32_t GLB_MAGIC       = 0x46546C67;                                   ///< "glTF"
		constexpr uint32_t GLB_CHUNK_JSON  = 0x4E4F534A;                                   ///< "JSON"
		constexpr uint32_t GLB_CHUNK_BIN   = 0x004E4942;                                   ///< "BIN\0"
		constexpr double   MAX_SAFE_INTEGER = 9007199254740991.0;                          ///< 2^53 - 1

		const char* const SUPPORTED_EXTENSIONS[] = {
			"KHR_mesh_quantization",
			"KHR_texture_transform",
			"KHR_lights_punctual",
			"KHR_materials_emissive_strength",
			"KHR_materials_unlit",
		};

		uint32_t readU32LE( const uint8_t* data ) {
			return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
		}

		/// UTF-8 string to a filesystem path (std::filesystem would use the ANSI code page on Windows).
		std::filesystem::path utf8Path( const std::string& utf8 ) {
			return std::filesystem::path( std::u8string( utf8.begin(), utf8.end() ) );
		}

		bool readFileBytes( const std::filesystem::path& path, std::vector<uint8_t>& output ) {
			std::ifstream file( path, std::ios::binary | std::ios::ate );
			if ( !file.is_open() )
				return false;
			const std::streamoff size = file.tellg();
			if ( size < 0 )
				return false;
			output.resize( (size_t)size );
			file.seekg( 0 );
			if ( size > 0 && !file.read( reinterpret_cast<char*>(output.data()), size ) )
				return false;
			return true;
		}

		int hexValue( char symbol ) {
			if ( symbol >= '0' && symbol <= '9' ) return symbol - '0';
			if ( symbol >= 'a' && symbol <= 'f' ) return symbol - 'a' + 10;
			if ( symbol >= 'A' && symbol <= 'F' ) return symbol - 'A' + 10;
			return -1;
		}

		/// "%20" -> ' '. Malformed escapes are kept as they are.
		std::string percentDecode( const std::string& text ) {
			std::string decoded;
			decoded.reserve( text.size() );
			for ( size_t i = 0; i < text.size(); ++i ) {
				if ( text[i] == '%' && i + 2 < text.size() && hexValue(text[i + 1]) >= 0 && hexValue(text[i + 2]) >= 0 ) {
					decoded.push_back( (char)(hexValue(text[i + 1]) * 16 + hexValue(text[i + 2])) );
					i += 2;
				} else {
					decoded.push_back( text[i] );
				}
			}
			return decoded;
		}

		bool base64Decode( const char* text, size_t length, std::vector<uint8_t>& output ) {
			auto value = []( char symbol ) -> int {
				if ( symbol >= 'A' && symbol <= 'Z' ) return symbol - 'A';
				if ( symbol >= 'a' && symbol <= 'z' ) return symbol - 'a' + 26;
				if ( symbol >= '0' && symbol <= '9' ) return symbol - '0' + 52;
				if ( symbol == '+' || symbol == '-' ) return 62;                            ///< Standard and URL safe alphabets
				if ( symbol == '/' || symbol == '_' ) return 63;
				return -1;
			};

			output.clear();
			output.reserve( length / 4 * 3 );
			uint32_t accumulator = 0;
			int bits = 0;
			bool paddingStarted = false;
			for ( size_t i = 0; i < length; ++i ) {
				const char symbol = text[i];
				if ( symbol == ' ' || symbol == '\n' || symbol == '\r' || symbol == '\t' )
					continue;
				if ( symbol == '=' ) {
					paddingStarted = true;
					continue;
				}
				const int sextet = value( symbol );
				if ( sextet < 0 || paddingStarted )
					return false;
				accumulator = (accumulator << 6) | (uint32_t)sextet;
				bits += 6;
				if ( bits >= 8 ) {
					bits -= 8;
					output.push_back( (uint8_t)((accumulator >> bits) & 0xFF) );
				}
			}
			return true;
		}

		/// True for "http:", "https:", "file:" ...: a scheme is letters followed by ':' before any '/'.
		bool hasUriScheme( const std::string& uri ) {
			for ( size_t i = 0; i < uri.size(); ++i ) {
				const char symbol = uri[i];
				if ( symbol == ':' )
					return i > 1;                                                          ///< "C:" is a Windows drive, not a scheme
				if ( !((symbol >= 'a' && symbol <= 'z') || (symbol >= 'A' && symbol <= 'Z') ||
					   (i > 0 && ((symbol >= '0' && symbol <= '9') || symbol == '+' || symbol == '-' || symbol == '.'))) )
					return false;
			}
			return false;
		}

		/*
		  ===================================================
		  Loading context and checked JSON access
		  ===================================================
		*/
		class Loader {
		public:
			Loader( Model& model, const std::string& source, const std::filesystem::path& baseDirectory, const LoadOptions& options )
				: model_(model), source_(source), baseDirectory_(baseDirectory), options_(options) {}

			void load( const uint8_t* data, uint64_t size );

		private:
			Model&                       model_;
			std::string                  source_;
			std::filesystem::path        baseDirectory_;
			const LoadOptions&           options_;
			const uint8_t*               glbBinary_ = nullptr;
			uint64_t                     glbBinarySize_ = 0;
			bool                         hasGlbBinary_ = false;

			[[noreturn]] void fail( const std::string& message ) const { detail::fail( source_, message ); }
			void warn( const std::string& message ) { model_.warnings.push_back( message ); }

			/// Reference to element `index` of a root array, validated against its size.
			template<typename T>
			uint32_t checkReference( uint32_t index, const std::vector<T>& array, const char* arrayName, const std::string& where ) const {
				if ( index >= array.size() )
					fail( where + " references " + arrayName + "[" + std::to_string(index) + "], there are " +
						  std::to_string(array.size()) );
				return index;
			}

			const JsonValue* member( const JsonValue& object, const char* key ) const { return object.find(key); }

			const JsonValue& requireMember( const JsonValue& object, const char* key, const std::string& where ) const {
				const JsonValue* value = object.find(key);
				if ( value == nullptr )
					fail( where + "." + key + " is required" );
				return *value;
			}

			void requireObject( const JsonValue& value, const std::string& where ) const {
				if ( !value.isObject() )
					fail( where + " must be an object" );
			}

			/// Array member or nullptr if absent. Present but not an array is an error.
			const JsonValue* arrayMember( const JsonValue& object, const char* key, const std::string& where ) const {
				const JsonValue* value = object.find(key);
				if ( value != nullptr && !value->isArray() )
					fail( where + "." + key + " must be an array" );
				return value;
			}

			uint64_t toInteger( const JsonValue& value, const std::string& where, uint64_t maximum ) const {
				if ( !value.isNumber() )
					fail( where + " must be a number" );
				const double number = value.asNumber();
				if ( !(number >= 0.0) || number != std::floor(number) || number > (double)maximum )
					fail( where + " must be an integer in [0, " + std::to_string(maximum) + "]" );
				return (uint64_t)number;
			}

			uint32_t toIndex( const JsonValue& value, const std::string& where ) const {
				return (uint32_t)toInteger( value, where, (uint64_t)INVALID_INDEX - 1 );
			}

			uint32_t requireIndex( const JsonValue& object, const char* key, const std::string& where ) const {
				return toIndex( requireMember(object, key, where), where + "." + key );
			}

			uint32_t optionalIndex( const JsonValue& object, const char* key, const std::string& where, uint32_t fallback = INVALID_INDEX ) const {
				const JsonValue* value = object.find(key);
				return value ? toIndex( *value, where + "." + key ) : fallback;
			}

			uint64_t optionalSize( const JsonValue& object, const char* key, const std::string& where, uint64_t fallback ) const {
				const JsonValue* value = object.find(key);
				return value ? toInteger( *value, where + "." + key, (uint64_t)MAX_SAFE_INTEGER ) : fallback;
			}

			float toFloat( const JsonValue& value, const std::string& where ) const {
				if ( !value.isNumber() )
					fail( where + " must be a number" );
				const double number = value.asNumber();
				if ( !std::isfinite(number) )
					fail( where + " must be finite" );
				return (float)number;
			}

			float optionalFloat( const JsonValue& object, const char* key, const std::string& where, float fallback ) const {
				const JsonValue* value = object.find(key);
				return value ? toFloat( *value, where + "." + key ) : fallback;
			}

			bool optionalBool( const JsonValue& object, const char* key, const std::string& where, bool fallback ) const {
				const JsonValue* value = object.find(key);
				if ( value == nullptr )
					return fallback;
				if ( !value->isBoolean() )
					fail( where + "." + key + " must be a boolean" );
				return value->value.boolean;
			}

			std::string optionalString( const JsonValue& object, const char* key, const std::string& where ) const {
				const JsonValue* value = object.find(key);
				if ( value == nullptr )
					return "";
				if ( !value->isString() )
					fail( where + "." + key + " must be a string" );
				return *value->value.string;
			}

			std::string requireString( const JsonValue& object, const char* key, const std::string& where ) const {
				const JsonValue& value = requireMember( object, key, where );
				if ( !value.isString() )
					fail( where + "." + key + " must be a string" );
				return *value.value.string;
			}

			/// Fixed size number array: absent leaves output untouched.
			void optionalFloatArray( const JsonValue& object, const char* key, const std::string& where, float* output, uint32_t count ) const {
				const JsonValue* array = arrayMember( object, key, where );
				if ( array == nullptr )
					return;
				if ( array->size() != count )
					fail( where + "." + key + " must have " + std::to_string(count) + " elements" );
				for ( uint32_t i = 0; i < count; ++i )
					output[i] = toFloat( *array->at(i), where + "." + key + "[" + std::to_string(i) + "]" );
			}

			std::vector<float> optionalFloatVector( const JsonValue& object, const char* key, const std::string& where ) const {
				std::vector<float> result;
				const JsonValue* array = arrayMember( object, key, where );
				if ( array == nullptr )
					return result;
				for ( uint32_t i = 0; i < array->size(); ++i )
					result.push_back( toFloat( *array->at(i), where + "." + key + "[" + std::to_string(i) + "]" ) );
				return result;
			}

			std::vector<uint32_t> optionalIndexVector( const JsonValue& object, const char* key, const std::string& where ) const {
				std::vector<uint32_t> result;
				const JsonValue* array = arrayMember( object, key, where );
				if ( array == nullptr )
					return result;
				for ( uint32_t i = 0; i < array->size(); ++i )
					result.push_back( toIndex( *array->at(i), where + "." + key + "[" + std::to_string(i) + "]" ) );
				return result;
			}

			/// Calls function(element, index, where) for every object of a root level array.
			template<typename Function>
			void forEachObject( const JsonValue& root, const char* key, Function&& function ) {
				const JsonValue* array = arrayMember( root, key, "root" );
				if ( array == nullptr )
					return;
				for ( uint32_t i = 0; i < array->size(); ++i ) {
					const std::string where = std::string(key) + "[" + std::to_string(i) + "]";
					const JsonValue& element = *array->at(i);
					requireObject( element, where );
					function( element, i, where );
				}
			}

			const JsonValue* extension( const JsonValue& object, const char* name ) const {
				const JsonValue* extensions = object.find("extensions");
				if ( extensions == nullptr || !extensions->isObject() )
					return nullptr;
				const JsonValue* value = extensions->find(name);
				return value != nullptr && value->isObject() ? value : nullptr;
			}

			bool isExtensionUsed( const std::string& name ) const {
				return std::find( model_.extensionsUsed.begin(), model_.extensionsUsed.end(), name ) != model_.extensionsUsed.end();
			}

			void parseGlb( const uint8_t* data, uint64_t size, std::string& jsonText );
			void parseAsset( const JsonValue& root );
			void parseExtensionLists( const JsonValue& root );
			void loadBuffers( const JsonValue& root );
			bool loadUri( const std::string& uri, std::vector<uint8_t>& output, const std::string& where, std::string& error );
			void parseBufferViews( const JsonValue& root );
			void parseAccessors( const JsonValue& root );
			void parseSamplers( const JsonValue& root );
			void parseImages( const JsonValue& root );
			void parseTextures( const JsonValue& root );
			TextureInfo parseTextureInfo( const JsonValue& object, const char* key, const char* scaleKey, const std::string& where );
			void parseMaterials( const JsonValue& root );
			void parseMeshes( const JsonValue& root );
			void parseCameras( const JsonValue& root );
			void parseLights( const JsonValue& root );
			void parseNodes( const JsonValue& root );
			void parseSkins( const JsonValue& root );
			void parseScenes( const JsonValue& root );
			void parseAnimations( const JsonValue& root );
			void validateMeshData();
			void decodeImages();

			AccessorType accessorTypeOf( const std::string& type, const std::string& where ) const;
			void checkAccessorFormat( uint32_t accessor, const std::string& where, std::initializer_list<AccessorType> types,
									  std::initializer_list<ComponentType> componentTypes, bool allowNormalized ) const;
		};

		void Loader::parseGlb( const uint8_t* data, uint64_t size, std::string& jsonText ) {
			if ( size < 20 )
				fail( "GLB file is too short" );
			const uint32_t version = readU32LE( data + 4 );
			const uint32_t length  = readU32LE( data + 8 );
			if ( version != 2 )
				fail( "GLB version " + std::to_string(version) + " is not supported, only 2" );
			if ( length > size )
				fail( "GLB header length " + std::to_string(length) + " is larger than the file (" + std::to_string(size) + " bytes)" );
			if ( length < size )
				warn( "GLB file has " + std::to_string(size - length) + " bytes after the declared length, they are ignored" );

			uint64_t offset = 12;
			bool isFirstChunk = true;
			while ( offset + 8 <= length ) {
				const uint32_t chunkLength = readU32LE( data + offset );
				const uint32_t chunkType   = readU32LE( data + offset + 4 );
				offset += 8;
				if ( offset + chunkLength > length )
					fail( "GLB chunk at offset " + std::to_string(offset - 8) + " is out of the file bounds" );

				if ( isFirstChunk ) {
					if ( chunkType != GLB_CHUNK_JSON )
						fail( "the first GLB chunk must be JSON" );
					jsonText.assign( reinterpret_cast<const char*>(data + offset), chunkLength );
					isFirstChunk = false;
				} else if ( chunkType == GLB_CHUNK_BIN ) {
					if ( hasGlbBinary_ )
						fail( "GLB file has more than one BIN chunk" );
					glbBinary_     = data + offset;
					glbBinarySize_ = chunkLength;
					hasGlbBinary_  = true;
				} else if ( chunkType == GLB_CHUNK_JSON ) {
					fail( "GLB file has more than one JSON chunk" );
				}                                                                          ///< Unknown chunk types are skipped
				offset += chunkLength;
				offset = (offset + 3) & ~(uint64_t)3;                                     ///< Chunks are 4 byte aligned
			}
			if ( isFirstChunk )
				fail( "GLB file has no JSON chunk" );
		}

		void Loader::parseAsset( const JsonValue& root ) {
			const JsonValue& asset = requireMember( root, "asset", "root" );
			requireObject( asset, "asset" );
			model_.asset.version    = requireString( asset, "version", "asset" );
			model_.asset.minVersion = optionalString( asset, "minVersion", "asset" );
			model_.asset.generator  = optionalString( asset, "generator", "asset" );
			model_.asset.copyright  = optionalString( asset, "copyright", "asset" );

			/// Major version must be 2. minVersion, if present, must not be newer than 2.0.
			const std::string& version = model_.asset.minVersion.empty() ? model_.asset.version : model_.asset.minVersion;
			const size_t dot = version.find('.');
			const std::string major = version.substr( 0, dot );
			const std::string minor = dot == std::string::npos ? "0" : version.substr( dot + 1 );
			if ( model_.asset.version.substr( 0, model_.asset.version.find('.') ) != "2" )
				fail( "glTF version " + model_.asset.version + " is not supported, only 2.x" );
			if ( !model_.asset.minVersion.empty() && (major != "2" || minor != "0") )
				fail( "asset.minVersion " + model_.asset.minVersion + " is newer than the supported 2.0" );
		}

		void Loader::parseExtensionLists( const JsonValue& root ) {
			auto readList = [this, &root]( const char* key, std::vector<std::string>& output ) {
				const JsonValue* list = arrayMember( root, key, "root" );
				if ( list == nullptr )
					return;
				for ( uint32_t i = 0; i < list->size(); ++i ) {
					if ( !list->at(i)->isString() )
						fail( std::string(key) + "[" + std::to_string(i) + "] must be a string" );
					output.push_back( *list->at(i)->value.string );
				}
			};
			readList( "extensionsUsed", model_.extensionsUsed );
			readList( "extensionsRequired", model_.extensionsRequired );

			std::string unsupported;
			for ( const std::string& name : model_.extensionsRequired ) {
				bool isSupported = false;
				for ( const char* supported : SUPPORTED_EXTENSIONS )
					isSupported = isSupported || name == supported;
				if ( !isSupported )
					unsupported += (unsupported.empty() ? "" : ", ") + name;
			}
			if ( !unsupported.empty() )
				fail( "required extensions are not supported: " + unsupported );

			for ( const std::string& name : model_.extensionsUsed ) {
				bool isSupported = false;
				for ( const char* supported : SUPPORTED_EXTENSIONS )
					isSupported = isSupported || name == supported;
				if ( !isSupported )
					warn( "extension " + name + " is not supported and is ignored" );
			}
		}

		/// Data of a data: URI or a file relative to the model. Returns false with error filled on failure.
		bool Loader::loadUri( const std::string& uri, std::vector<uint8_t>& output, const std::string& where, std::string& error ) {
			if ( uri.rfind( "data:", 0 ) == 0 ) {
				const size_t comma = uri.find(',');
				if ( comma == std::string::npos ) {
					error = where + ": malformed data URI";
					return false;
				}
				const std::string header = uri.substr( 0, comma );
				if ( header.size() >= 7 && header.compare( header.size() - 7, 7, ";base64" ) == 0 ) {
					if ( !base64Decode( uri.data() + comma + 1, uri.size() - comma - 1, output ) ) {
						error = where + ": invalid base64 data in the data URI";
						return false;
					}
				} else {
					const std::string decoded = percentDecode( uri.substr( comma + 1 ) );
					output.assign( decoded.begin(), decoded.end() );
				}
				return true;
			}

			if ( hasUriScheme(uri) ) {
				error = where + ": URI \"" + uri + "\" is not supported (only relative paths and data: URIs)";
				return false;
			}

			const std::filesystem::path path = baseDirectory_ / utf8Path( percentDecode(uri) );
			if ( !readFileBytes( path, output ) ) {
				error = where + ": can't read file " + reinterpret_cast<const char*>(path.u8string().c_str());
				return false;
			}
			return true;
		}

		void Loader::loadBuffers( const JsonValue& root ) {
			forEachObject( root, "buffers", [this]( const JsonValue& object, uint32_t index, const std::string& where ) {
				Buffer buffer;
				buffer.name       = optionalString( object, "name", where );
				buffer.uri        = optionalString( object, "uri", where );
				buffer.byteLength = toInteger( requireMember( object, "byteLength", where ), where + ".byteLength", (uint64_t)MAX_SAFE_INTEGER );
				if ( buffer.byteLength == 0 )
					fail( where + ".byteLength must be at least 1" );

				if ( object.find("uri") == nullptr ) {
					if ( index != 0 || !hasGlbBinary_ )
						fail( where + " has no uri and is not the GLB binary chunk" );
					if ( buffer.byteLength > glbBinarySize_ )
						fail( where + ".byteLength " + std::to_string(buffer.byteLength) + " is larger than the GLB binary chunk (" +
							  std::to_string(glbBinarySize_) + " bytes)" );
					buffer.data.assign( glbBinary_, glbBinary_ + buffer.byteLength );
				} else {
					std::string error;
					if ( !loadUri( buffer.uri, buffer.data, where, error ) )
						fail( error );
					if ( buffer.data.size() < buffer.byteLength )
						fail( where + " has " + std::to_string(buffer.data.size()) + " bytes, byteLength is " + std::to_string(buffer.byteLength) );
					buffer.data.resize( buffer.byteLength );
				}
				model_.buffers.push_back( std::move(buffer) );
			});
		}

		void Loader::parseBufferViews( const JsonValue& root ) {
			forEachObject( root, "bufferViews", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				BufferView view;
				view.name       = optionalString( object, "name", where );
				view.buffer     = checkReference( requireIndex( object, "buffer", where ), model_.buffers, "buffers", where + ".buffer" );
				view.byteOffset = optionalSize( object, "byteOffset", where, 0 );
				view.byteLength = toInteger( requireMember( object, "byteLength", where ), where + ".byteLength", (uint64_t)MAX_SAFE_INTEGER );
				view.byteStride = (uint32_t)optionalSize( object, "byteStride", where, 0 );
				view.target     = (uint32_t)optionalSize( object, "target", where, 0 );

				if ( view.byteLength == 0 )
					fail( where + ".byteLength must be at least 1" );
				if ( view.byteOffset + view.byteLength > model_.buffers[view.buffer].byteLength )
					fail( where + " (offset " + std::to_string(view.byteOffset) + ", length " + std::to_string(view.byteLength) +
						  ") is out of buffers[" + std::to_string(view.buffer) + "] (" + std::to_string(model_.buffers[view.buffer].byteLength) + " bytes)" );
				if ( object.find("byteStride") != nullptr && (view.byteStride < 4 || view.byteStride > 252) )
					fail( where + ".byteStride must be in [4, 252]" );
				if ( view.byteStride % 4 != 0 )
					warn( where + ".byteStride is not a multiple of 4" );
				if ( view.target != 0 && view.target != 34962 && view.target != 34963 )
					warn( where + ".target " + std::to_string(view.target) + " is unknown" );
				model_.bufferViews.push_back( view );
			});
		}

		AccessorType Loader::accessorTypeOf( const std::string& type, const std::string& where ) const {
			if ( type == "SCALAR" ) return AccessorType::SCALAR;
			if ( type == "VEC2" )   return AccessorType::VEC2;
			if ( type == "VEC3" )   return AccessorType::VEC3;
			if ( type == "VEC4" )   return AccessorType::VEC4;
			if ( type == "MAT2" )   return AccessorType::MAT2;
			if ( type == "MAT3" )   return AccessorType::MAT3;
			if ( type == "MAT4" )   return AccessorType::MAT4;
			fail( where + ".type \"" + type + "\" is unknown" );
		}

		void Loader::parseAccessors( const JsonValue& root ) {
			forEachObject( root, "accessors", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Accessor accessor;
				accessor.name          = optionalString( object, "name", where );
				accessor.bufferView    = optionalIndex( object, "bufferView", where );
				accessor.byteOffset    = optionalSize( object, "byteOffset", where, 0 );
				const uint32_t componentType = requireIndex( object, "componentType", where );
				accessor.componentType = (ComponentType)componentType;
				if ( componentTypeSize( accessor.componentType ) == 0 )
					fail( where + ".componentType " + std::to_string(componentType) + " is unknown" );
				accessor.normalized    = optionalBool( object, "normalized", where, false );
				accessor.count         = requireIndex( object, "count", where );
				accessor.type          = accessorTypeOf( requireString( object, "type", where ), where );

				if ( accessor.count == 0 )
					fail( where + ".count must be at least 1" );
				if ( accessor.normalized && (accessor.componentType == ComponentType::FLOAT || accessor.componentType == ComponentType::UNSIGNED_INT) )
					fail( where + ": FLOAT and UNSIGNED_INT accessors can't be normalized" );

				const uint32_t components = componentsNumber( accessor.type );
				for ( const char* key : { "min", "max" } ) {
					const JsonValue* bound = arrayMember( object, key, where );
					if ( bound == nullptr )
						continue;
					if ( bound->size() != components )
						fail( where + "." + key + " must have " + std::to_string(components) + " elements" );
					std::vector<double>& output = key[1] == 'i' ? accessor.min : accessor.max;
					for ( uint32_t i = 0; i < components; ++i ) {
						if ( !bound->at(i)->isNumber() )
							fail( where + "." + key + " must contain numbers" );
						output.push_back( bound->at(i)->asNumber() );
					}
				}

				const uint64_t elementSize = detail::elementByteSize( accessor.componentType, accessor.type );
				if ( accessor.bufferView != INVALID_INDEX ) {
					checkReference( accessor.bufferView, model_.bufferViews, "bufferViews", where + ".bufferView" );
					const BufferView& view = model_.bufferViews[accessor.bufferView];
					const uint64_t stride = view.byteStride ? view.byteStride : elementSize;
					if ( stride < elementSize )
						fail( where + ": bufferViews[" + std::to_string(accessor.bufferView) + "].byteStride " + std::to_string(stride) +
							  " is smaller than the element size " + std::to_string(elementSize) );
					if ( accessor.byteOffset + stride * (accessor.count - 1) + elementSize > view.byteLength )
						fail( where + " (offset " + std::to_string(accessor.byteOffset) + ", " + std::to_string(accessor.count) +
							  " elements) is out of bufferViews[" + std::to_string(accessor.bufferView) + "] (" + std::to_string(view.byteLength) + " bytes)" );
					if ( (view.byteOffset + accessor.byteOffset) % componentTypeSize( accessor.componentType ) != 0 )
						warn( where + " data is not aligned to its component size" );
				} else if ( object.find("byteOffset") != nullptr ) {
					fail( where + ".byteOffset requires a bufferView" );
				}

				const JsonValue* sparse = member( object, "sparse" );
				if ( sparse != nullptr ) {
					const std::string sparseWhere = where + ".sparse";
					requireObject( *sparse, sparseWhere );
					accessor.isSparse     = true;
					accessor.sparse.count = requireIndex( *sparse, "count", sparseWhere );
					if ( accessor.sparse.count == 0 || accessor.sparse.count > accessor.count )
						fail( sparseWhere + ".count must be in [1, " + std::to_string(accessor.count) + "]" );

					const JsonValue& indices = requireMember( *sparse, "indices", sparseWhere );
					requireObject( indices, sparseWhere + ".indices" );
					accessor.sparse.indicesBufferView = checkReference( requireIndex( indices, "bufferView", sparseWhere + ".indices" ),
																		model_.bufferViews, "bufferViews", sparseWhere + ".indices.bufferView" );
					accessor.sparse.indicesByteOffset = optionalSize( indices, "byteOffset", sparseWhere + ".indices", 0 );
					const uint32_t indicesType = requireIndex( indices, "componentType", sparseWhere + ".indices" );
					accessor.sparse.indicesComponentType = (ComponentType)indicesType;
					if ( indicesType != (uint32_t)ComponentType::UNSIGNED_BYTE && indicesType != (uint32_t)ComponentType::UNSIGNED_SHORT &&
						 indicesType != (uint32_t)ComponentType::UNSIGNED_INT )
						fail( sparseWhere + ".indices.componentType must be UNSIGNED_BYTE, UNSIGNED_SHORT or UNSIGNED_INT" );

					const JsonValue& values = requireMember( *sparse, "values", sparseWhere );
					requireObject( values, sparseWhere + ".values" );
					accessor.sparse.valuesBufferView = checkReference( requireIndex( values, "bufferView", sparseWhere + ".values" ),
																	   model_.bufferViews, "bufferViews", sparseWhere + ".values.bufferView" );
					accessor.sparse.valuesByteOffset = optionalSize( values, "byteOffset", sparseWhere + ".values", 0 );

					const BufferView& indicesView = model_.bufferViews[accessor.sparse.indicesBufferView];
					const BufferView& valuesView  = model_.bufferViews[accessor.sparse.valuesBufferView];
					if ( accessor.sparse.indicesByteOffset + (uint64_t)accessor.sparse.count * componentTypeSize( accessor.sparse.indicesComponentType ) > indicesView.byteLength )
						fail( sparseWhere + ".indices is out of its bufferView" );
					if ( accessor.sparse.valuesByteOffset + (uint64_t)accessor.sparse.count * elementSize > valuesView.byteLength )
						fail( sparseWhere + ".values is out of its bufferView" );
				}
				model_.accessors.push_back( accessor );
			});
		}

		void Loader::checkAccessorFormat( uint32_t accessor, const std::string& where, std::initializer_list<AccessorType> types,
										  std::initializer_list<ComponentType> componentTypes, bool allowNormalized ) const {
			checkReference( accessor, model_.accessors, "accessors", where );
			const Accessor& data = model_.accessors[accessor];
			if ( std::find( types.begin(), types.end(), data.type ) == types.end() )
				fail( where + ": accessors[" + std::to_string(accessor) + "] has a wrong type" );
			if ( std::find( componentTypes.begin(), componentTypes.end(), data.componentType ) == componentTypes.end() )
				fail( where + ": accessors[" + std::to_string(accessor) + "] has a wrong componentType " +
					  std::to_string((uint32_t)data.componentType) );
			if ( data.normalized && !allowNormalized )
				fail( where + ": accessors[" + std::to_string(accessor) + "] must not be normalized" );
		}

		void Loader::parseSamplers( const JsonValue& root ) {
			forEachObject( root, "samplers", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Sampler sampler;
				sampler.name      = optionalString( object, "name", where );
				sampler.magFilter = (uint32_t)optionalSize( object, "magFilter", where, 0 );
				sampler.minFilter = (uint32_t)optionalSize( object, "minFilter", where, 0 );
				sampler.wrapS     = (uint32_t)optionalSize( object, "wrapS", where, 10497 );
				sampler.wrapT     = (uint32_t)optionalSize( object, "wrapT", where, 10497 );
				model_.samplers.push_back( sampler );
			});
		}

		void Loader::parseImages( const JsonValue& root ) {
			forEachObject( root, "images", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Image image;
				image.name       = optionalString( object, "name", where );
				image.uri        = optionalString( object, "uri", where );
				image.mimeType   = optionalString( object, "mimeType", where );
				image.bufferView = optionalIndex( object, "bufferView", where );
				const bool hasUri = object.find("uri") != nullptr;
				if ( hasUri == (image.bufferView != INVALID_INDEX) )
					fail( where + " must have either uri or bufferView" );
				if ( image.bufferView != INVALID_INDEX ) {
					checkReference( image.bufferView, model_.bufferViews, "bufferViews", where + ".bufferView" );
					if ( image.mimeType.empty() )
						fail( where + ".mimeType is required with bufferView" );
				}
				model_.images.push_back( std::move(image) );
			});
		}

		void Loader::parseTextures( const JsonValue& root ) {
			forEachObject( root, "textures", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Texture texture;
				texture.name    = optionalString( object, "name", where );
				texture.sampler = optionalIndex( object, "sampler", where );
				texture.source  = optionalIndex( object, "source", where );
				if ( texture.sampler != INVALID_INDEX )
					checkReference( texture.sampler, model_.samplers, "samplers", where + ".sampler" );
				if ( texture.source != INVALID_INDEX )
					checkReference( texture.source, model_.images, "images", where + ".source" );
				else
					warn( where + " has no source image (it may be provided by an unsupported extension)" );
				model_.textures.push_back( texture );
			});
		}

		TextureInfo Loader::parseTextureInfo( const JsonValue& object, const char* key, const char* scaleKey, const std::string& where ) {
			TextureInfo info;
			const JsonValue* value = object.find(key);
			if ( value == nullptr )
				return info;
			const std::string infoWhere = where + "." + key;
			requireObject( *value, infoWhere );
			info.index    = checkReference( requireIndex( *value, "index", infoWhere ), model_.textures, "textures", infoWhere + ".index" );
			info.texCoord = optionalIndex( *value, "texCoord", infoWhere, 0 );
			if ( scaleKey != nullptr )
				info.scale = optionalFloat( *value, scaleKey, infoWhere, 1.0f );

			const JsonValue* transform = extension( *value, "KHR_texture_transform" );
			if ( transform != nullptr ) {
				const std::string transformWhere = infoWhere + ".extensions.KHR_texture_transform";
				info.hasTransform = true;
				optionalFloatArray( *transform, "offset", transformWhere, info.transform.offset, 2 );
				optionalFloatArray( *transform, "scale", transformWhere, info.transform.scale, 2 );
				info.transform.rotation = optionalFloat( *transform, "rotation", transformWhere, 0.0f );
				info.transform.texCoord = optionalIndex( *transform, "texCoord", transformWhere );
			}
			return info;
		}

		void Loader::parseMaterials( const JsonValue& root ) {
			forEachObject( root, "materials", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Material material;
				material.name = optionalString( object, "name", where );

				const JsonValue* pbr = member( object, "pbrMetallicRoughness" );
				if ( pbr != nullptr ) {
					const std::string pbrWhere = where + ".pbrMetallicRoughness";
					requireObject( *pbr, pbrWhere );
					optionalFloatArray( *pbr, "baseColorFactor", pbrWhere, material.baseColorFactor, 4 );
					material.baseColorTexture         = parseTextureInfo( *pbr, "baseColorTexture", nullptr, pbrWhere );
					material.metallicFactor           = optionalFloat( *pbr, "metallicFactor", pbrWhere, 1.0f );
					material.roughnessFactor          = optionalFloat( *pbr, "roughnessFactor", pbrWhere, 1.0f );
					material.metallicRoughnessTexture = parseTextureInfo( *pbr, "metallicRoughnessTexture", nullptr, pbrWhere );
				}
				material.normalTexture    = parseTextureInfo( object, "normalTexture", "scale", where );
				material.occlusionTexture = parseTextureInfo( object, "occlusionTexture", "strength", where );
				material.emissiveTexture  = parseTextureInfo( object, "emissiveTexture", nullptr, where );
				optionalFloatArray( object, "emissiveFactor", where, material.emissiveFactor, 3 );

				const std::string alphaMode = optionalString( object, "alphaMode", where );
				if ( alphaMode == "MASK" )
					material.alphaMode = AlphaMode::ALPHA_MASK;
				else if ( alphaMode == "BLEND" )
					material.alphaMode = AlphaMode::ALPHA_BLEND;
				else if ( !alphaMode.empty() && alphaMode != "OPAQUE" )
					fail( where + ".alphaMode \"" + alphaMode + "\" is unknown" );
				material.alphaCutoff = optionalFloat( object, "alphaCutoff", where, 0.5f );
				material.doubleSided = optionalBool( object, "doubleSided", where, false );

				const JsonValue* emissiveStrength = extension( object, "KHR_materials_emissive_strength" );
				if ( emissiveStrength != nullptr )
					material.emissiveStrength = optionalFloat( *emissiveStrength, "emissiveStrength", where + ".extensions.KHR_materials_emissive_strength", 1.0f );
				material.unlit = extension( object, "KHR_materials_unlit" ) != nullptr;
				model_.materials.push_back( material );
			});
		}

		void Loader::parseMeshes( const JsonValue& root ) {
			forEachObject( root, "meshes", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Mesh mesh;
				mesh.name    = optionalString( object, "name", where );
				mesh.weights = optionalFloatVector( object, "weights", where );

				const JsonValue& primitives = requireMember( object, "primitives", where );
				if ( !primitives.isArray() || primitives.size() == 0 )
					fail( where + ".primitives must be a non-empty array" );

				for ( uint32_t p = 0; p < primitives.size(); ++p ) {
					const std::string primitiveWhere = where + ".primitives[" + std::to_string(p) + "]";
					const JsonValue& primitiveObject = *primitives.at(p);
					requireObject( primitiveObject, primitiveWhere );

					Primitive primitive;
					const JsonValue& attributes = requireMember( primitiveObject, "attributes", primitiveWhere );
					requireObject( attributes, primitiveWhere + ".attributes" );
					attributes.forEachMember( [&]( const std::string& name, const JsonValue& value ) {
						primitive.attributes.push_back( { name, checkReference( toIndex( value, primitiveWhere + ".attributes." + name ), model_.accessors,
																				"accessors", primitiveWhere + ".attributes." + name ) } );
					});
					std::sort( primitive.attributes.begin(), primitive.attributes.end(),
							   []( const Attribute& a, const Attribute& b ) { return a.name < b.name; } );
					if ( primitive.attributes.empty() )
						fail( primitiveWhere + ".attributes must not be empty" );

					primitive.indices  = optionalIndex( primitiveObject, "indices", primitiveWhere );
					primitive.material = optionalIndex( primitiveObject, "material", primitiveWhere );
					const uint32_t mode = (uint32_t)optionalSize( primitiveObject, "mode", primitiveWhere, 4 );
					if ( mode > 6 )
						fail( primitiveWhere + ".mode " + std::to_string(mode) + " is unknown" );
					primitive.mode = (PrimitiveMode)mode;
					if ( primitive.material != INVALID_INDEX )
						checkReference( primitive.material, model_.materials, "materials", primitiveWhere + ".material" );
					if ( primitive.indices != INVALID_INDEX )
						checkAccessorFormat( primitive.indices, primitiveWhere + ".indices", { AccessorType::SCALAR },
											 { ComponentType::UNSIGNED_BYTE, ComponentType::UNSIGNED_SHORT, ComponentType::UNSIGNED_INT }, false );

					/// Attribute formats (KHR_mesh_quantization widens the allowed component types, so they are all accepted for vectors).
					for ( const Attribute& attribute : primitive.attributes ) {
						const std::string attributeWhere = primitiveWhere + ".attributes." + attribute.name;
						if ( attribute.name == "POSITION" || attribute.name == "NORMAL" )
							checkAccessorFormat( attribute.accessor, attributeWhere, { AccessorType::VEC3 },
												 { ComponentType::FLOAT, ComponentType::BYTE, ComponentType::UNSIGNED_BYTE, ComponentType::SHORT, ComponentType::UNSIGNED_SHORT }, true );
						else if ( attribute.name == "TANGENT" )
							checkAccessorFormat( attribute.accessor, attributeWhere, { AccessorType::VEC4 },
												 { ComponentType::FLOAT, ComponentType::BYTE, ComponentType::SHORT }, true );
						else if ( attribute.name.rfind( "TEXCOORD_", 0 ) == 0 )
							checkAccessorFormat( attribute.accessor, attributeWhere, { AccessorType::VEC2 },
												 { ComponentType::FLOAT, ComponentType::BYTE, ComponentType::UNSIGNED_BYTE, ComponentType::SHORT, ComponentType::UNSIGNED_SHORT }, true );
						else if ( attribute.name.rfind( "COLOR_", 0 ) == 0 )
							checkAccessorFormat( attribute.accessor, attributeWhere, { AccessorType::VEC3, AccessorType::VEC4 },
												 { ComponentType::FLOAT, ComponentType::UNSIGNED_BYTE, ComponentType::UNSIGNED_SHORT }, true );
						else if ( attribute.name.rfind( "JOINTS_", 0 ) == 0 )
							checkAccessorFormat( attribute.accessor, attributeWhere, { AccessorType::VEC4 },
												 { ComponentType::UNSIGNED_BYTE, ComponentType::UNSIGNED_SHORT }, false );
						else if ( attribute.name.rfind( "WEIGHTS_", 0 ) == 0 )
							checkAccessorFormat( attribute.accessor, attributeWhere, { AccessorType::VEC4 },
												 { ComponentType::FLOAT, ComponentType::UNSIGNED_BYTE, ComponentType::UNSIGNED_SHORT }, true );
					}

					const JsonValue* targets = arrayMember( primitiveObject, "targets", primitiveWhere );
					if ( targets != nullptr ) {
						for ( uint32_t t = 0; t < targets->size(); ++t ) {
							const std::string targetWhere = primitiveWhere + ".targets[" + std::to_string(t) + "]";
							requireObject( *targets->at(t), targetWhere );
							MorphTarget target;
							targets->at(t)->forEachMember( [&]( const std::string& name, const JsonValue& value ) {
								const uint32_t accessor = checkReference( toIndex( value, targetWhere + "." + name ), model_.accessors,
																		  "accessors", targetWhere + "." + name );
								if ( primitive.findAttribute( name ) == INVALID_INDEX )
									warn( targetWhere + "." + name + " has no base attribute and is ignored" );
								target.attributes.push_back( { name, accessor } );
							});
							std::sort( target.attributes.begin(), target.attributes.end(),
									   []( const Attribute& a, const Attribute& b ) { return a.name < b.name; } );
							primitive.targets.push_back( std::move(target) );
						}
					}

					if ( !mesh.primitives.empty() && mesh.primitives[0].targets.size() != primitive.targets.size() )
						fail( where + ": all primitives must have the same number of morph targets" );
					mesh.primitives.push_back( std::move(primitive) );
				}

				if ( !mesh.weights.empty() && mesh.weights.size() != mesh.primitives[0].targets.size() )
					fail( where + ".weights has " + std::to_string(mesh.weights.size()) + " values for " +
						  std::to_string(mesh.primitives[0].targets.size()) + " morph targets" );

				const JsonValue* extras = member( object, "extras" );
				if ( extras != nullptr && extras->isObject() ) {
					const JsonValue* targetNames = extras->find("targetNames");
					if ( targetNames != nullptr && targetNames->isArray() ) {
						for ( uint32_t i = 0; i < targetNames->size(); ++i )
							mesh.targetNames.push_back( targetNames->at(i)->isString() ? *targetNames->at(i)->value.string : "" );
					}
				}
				model_.meshes.push_back( std::move(mesh) );
			});
		}

		void Loader::parseCameras( const JsonValue& root ) {
			forEachObject( root, "cameras", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Camera camera;
				camera.name = optionalString( object, "name", where );
				const std::string type = requireString( object, "type", where );
				if ( type == "perspective" ) {
					camera.type = CameraType::PERSPECTIVE;
					const JsonValue& perspective = requireMember( object, "perspective", where );
					requireObject( perspective, where + ".perspective" );
					camera.aspectRatio = optionalFloat( perspective, "aspectRatio", where + ".perspective", 0.0f );
					camera.yfov  = toFloat( requireMember( perspective, "yfov", where + ".perspective" ), where + ".perspective.yfov" );
					camera.znear = toFloat( requireMember( perspective, "znear", where + ".perspective" ), where + ".perspective.znear" );
					camera.zfar  = optionalFloat( perspective, "zfar", where + ".perspective", 0.0f );
					if ( camera.yfov <= 0.0f || camera.znear <= 0.0f || (camera.zfar != 0.0f && camera.zfar <= camera.znear) )
						fail( where + ".perspective has invalid yfov, znear or zfar" );
				} else if ( type == "orthographic" ) {
					camera.type = CameraType::ORTHOGRAPHIC;
					const JsonValue& orthographic = requireMember( object, "orthographic", where );
					requireObject( orthographic, where + ".orthographic" );
					camera.xmag  = toFloat( requireMember( orthographic, "xmag", where + ".orthographic" ), where + ".orthographic.xmag" );
					camera.ymag  = toFloat( requireMember( orthographic, "ymag", where + ".orthographic" ), where + ".orthographic.ymag" );
					camera.znear = toFloat( requireMember( orthographic, "znear", where + ".orthographic" ), where + ".orthographic.znear" );
					camera.zfar  = toFloat( requireMember( orthographic, "zfar", where + ".orthographic" ), where + ".orthographic.zfar" );
					if ( camera.zfar <= camera.znear || camera.znear < 0.0f )
						fail( where + ".orthographic has invalid znear or zfar" );
				} else {
					fail( where + ".type \"" + type + "\" is unknown" );
				}
				model_.cameras.push_back( camera );
			});
		}

		void Loader::parseLights( const JsonValue& root ) {
			const JsonValue* lights = extension( root, "KHR_lights_punctual" );
			if ( lights == nullptr )
				return;
			forEachObject( *lights, "lights", [this]( const JsonValue& object, uint32_t, const std::string& index ) {
				const std::string where = "extensions.KHR_lights_punctual." + index;
				Light light;
				light.name = optionalString( object, "name", where );
				const std::string type = requireString( object, "type", where );
				if ( type == "directional" )
					light.type = LightType::DIRECTIONAL;
				else if ( type == "point" )
					light.type = LightType::POINT;
				else if ( type == "spot" )
					light.type = LightType::SPOT;
				else
					fail( where + ".type \"" + type + "\" is unknown" );
				optionalFloatArray( object, "color", where, light.color, 3 );
				light.intensity = optionalFloat( object, "intensity", where, 1.0f );
				light.range     = optionalFloat( object, "range", where, 0.0f );
				if ( light.range < 0.0f )
					fail( where + ".range must be positive" );
				if ( light.type == LightType::SPOT ) {
					const JsonValue& spot = requireMember( object, "spot", where );
					requireObject( spot, where + ".spot" );
					light.innerConeAngle = optionalFloat( spot, "innerConeAngle", where + ".spot", 0.0f );
					light.outerConeAngle = optionalFloat( spot, "outerConeAngle", where + ".spot", 0.78539816339f );
					if ( light.innerConeAngle < 0.0f || light.innerConeAngle >= light.outerConeAngle || light.outerConeAngle > 1.5707963268f )
						fail( where + ".spot cone angles must satisfy 0 <= inner < outer <= PI / 2" );
				}
				model_.lights.push_back( light );
			});
		}

		void Loader::parseNodes( const JsonValue& root ) {
			forEachObject( root, "nodes", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Node node;
				node.name     = optionalString( object, "name", where );
				node.children = optionalIndexVector( object, "children", where );
				node.mesh     = optionalIndex( object, "mesh", where );
				node.skin     = optionalIndex( object, "skin", where );
				node.camera   = optionalIndex( object, "camera", where );
				node.weights  = optionalFloatVector( object, "weights", where );
				node.hasMatrix = object.find("matrix") != nullptr;
				optionalFloatArray( object, "matrix", where, node.matrix, 16 );
				optionalFloatArray( object, "translation", where, node.translation, 3 );
				optionalFloatArray( object, "rotation", where, node.rotation, 4 );
				optionalFloatArray( object, "scale", where, node.scale, 3 );
				if ( node.hasMatrix && (object.find("translation") || object.find("rotation") || object.find("scale")) )
					warn( where + " has both matrix and TRS, matrix is used" );

				/// Rotations must be unit quaternions, exporters sometimes write slightly denormalized ones.
				const float length = std::sqrt( node.rotation[0] * node.rotation[0] + node.rotation[1] * node.rotation[1] +
												node.rotation[2] * node.rotation[2] + node.rotation[3] * node.rotation[3] );
				if ( !(length > 0.0f) )
					fail( where + ".rotation is a zero quaternion" );
				for ( float& component : node.rotation )
					component /= length;

				const JsonValue* light = extension( object, "KHR_lights_punctual" );
				if ( light != nullptr )
					node.light = requireIndex( *light, "light", where + ".extensions.KHR_lights_punctual" );
				model_.nodes.push_back( std::move(node) );
			});

			/// References and the hierarchy: every node has at most one parent and there are no cycles.
			for ( uint32_t i = 0; i < model_.nodes.size(); ++i ) {
				Node& node = model_.nodes[i];
				const std::string where = "nodes[" + std::to_string(i) + "]";
				if ( node.mesh != INVALID_INDEX )
					checkReference( node.mesh, model_.meshes, "meshes", where + ".mesh" );
				if ( node.camera != INVALID_INDEX )
					checkReference( node.camera, model_.cameras, "cameras", where + ".camera" );
				if ( node.light != INVALID_INDEX )
					checkReference( node.light, model_.lights, "lights", where + ".extensions.KHR_lights_punctual.light" );
				if ( node.mesh != INVALID_INDEX && !node.weights.empty() &&
					 node.weights.size() != model_.meshes[node.mesh].primitives[0].targets.size() )
					fail( where + ".weights size differs from the number of morph targets of its mesh" );
				for ( uint32_t child : node.children ) {
					checkReference( child, model_.nodes, "nodes", where + ".children" );
					if ( child == i )
						fail( where + " is its own child" );
					if ( model_.nodes[child].parent != INVALID_INDEX )
						fail( "nodes[" + std::to_string(child) + "] has more than one parent" );
					model_.nodes[child].parent = i;
				}
			}
			for ( uint32_t i = 0; i < model_.nodes.size(); ++i ) {
				uint32_t current = i;
				for ( uint32_t depth = 0; current != INVALID_INDEX; ++depth ) {
					if ( depth > model_.nodes.size() )
						fail( "node hierarchy has a cycle through nodes[" + std::to_string(i) + "]" );
					current = model_.nodes[current].parent;
				}
			}
		}

		void Loader::parseSkins( const JsonValue& root ) {
			forEachObject( root, "skins", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Skin skin;
				skin.name   = optionalString( object, "name", where );
				skin.joints = optionalIndexVector( object, "joints", where );
				if ( skin.joints.empty() )
					fail( where + ".joints must be a non-empty array" );
				for ( uint32_t joint : skin.joints )
					checkReference( joint, model_.nodes, "nodes", where + ".joints" );
				skin.skeleton = optionalIndex( object, "skeleton", where );
				if ( skin.skeleton != INVALID_INDEX )
					checkReference( skin.skeleton, model_.nodes, "nodes", where + ".skeleton" );
				skin.inverseBindMatrices = optionalIndex( object, "inverseBindMatrices", where );
				if ( skin.inverseBindMatrices != INVALID_INDEX ) {
					checkAccessorFormat( skin.inverseBindMatrices, where + ".inverseBindMatrices", { AccessorType::MAT4 }, { ComponentType::FLOAT }, false );
					if ( model_.accessors[skin.inverseBindMatrices].count < skin.joints.size() )
						fail( where + ".inverseBindMatrices has fewer matrices than joints" );
				}
				model_.skins.push_back( std::move(skin) );
			});

			for ( uint32_t i = 0; i < model_.nodes.size(); ++i ) {
				const Node& node = model_.nodes[i];
				if ( node.skin == INVALID_INDEX )
					continue;
				const std::string where = "nodes[" + std::to_string(i) + "]";
				checkReference( node.skin, model_.skins, "skins", where + ".skin" );
				if ( node.mesh == INVALID_INDEX )
					warn( where + " has a skin but no mesh" );
			}
		}

		void Loader::parseScenes( const JsonValue& root ) {
			forEachObject( root, "scenes", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Scene scene;
				scene.name  = optionalString( object, "name", where );
				scene.nodes = optionalIndexVector( object, "nodes", where );
				for ( uint32_t node : scene.nodes ) {
					checkReference( node, model_.nodes, "nodes", where + ".nodes" );
					if ( model_.nodes[node].parent != INVALID_INDEX )
						warn( where + ".nodes contains nodes[" + std::to_string(node) + "], which is not a root node" );
				}
				model_.scenes.push_back( std::move(scene) );
			});
			model_.scene = optionalIndex( root, "scene", "root" );
			if ( model_.scene != INVALID_INDEX )
				checkReference( model_.scene, model_.scenes, "scenes", "scene" );
		}

		void Loader::parseAnimations( const JsonValue& root ) {
			forEachObject( root, "animations", [this]( const JsonValue& object, uint32_t, const std::string& where ) {
				Animation animation;
				animation.name = optionalString( object, "name", where );

				const JsonValue& samplers = requireMember( object, "samplers", where );
				if ( !samplers.isArray() || samplers.size() == 0 )
					fail( where + ".samplers must be a non-empty array" );
				for ( uint32_t s = 0; s < samplers.size(); ++s ) {
					const std::string samplerWhere = where + ".samplers[" + std::to_string(s) + "]";
					requireObject( *samplers.at(s), samplerWhere );
					AnimationSampler sampler;
					sampler.input  = requireIndex( *samplers.at(s), "input", samplerWhere );
					sampler.output = requireIndex( *samplers.at(s), "output", samplerWhere );
					checkAccessorFormat( sampler.input, samplerWhere + ".input", { AccessorType::SCALAR }, { ComponentType::FLOAT }, false );
					checkReference( sampler.output, model_.accessors, "accessors", samplerWhere + ".output" );
					const std::string interpolation = optionalString( *samplers.at(s), "interpolation", samplerWhere );
					if ( interpolation == "STEP" )
						sampler.interpolation = Interpolation::STEP;
					else if ( interpolation == "CUBICSPLINE" )
						sampler.interpolation = Interpolation::CUBICSPLINE;
					else if ( !interpolation.empty() && interpolation != "LINEAR" )
						fail( samplerWhere + ".interpolation \"" + interpolation + "\" is unknown" );

					sampler.times = readAccessorAsFloats( model_, sampler.input );
					for ( size_t k = 0; k < sampler.times.size(); ++k ) {
						if ( !std::isfinite(sampler.times[k]) || sampler.times[k] < 0.0f || (k > 0 && sampler.times[k] <= sampler.times[k - 1]) )
							fail( samplerWhere + ".input key frame times must be non-negative and strictly increasing" );
					}
					if ( sampler.interpolation == Interpolation::CUBICSPLINE && sampler.times.size() < 2 )
						fail( samplerWhere + ": CUBICSPLINE needs at least 2 key frames" );
					animation.samplers.push_back( std::move(sampler) );
				}

				const JsonValue& channels = requireMember( object, "channels", where );
				if ( !channels.isArray() || channels.size() == 0 )
					fail( where + ".channels must be a non-empty array" );
				for ( uint32_t c = 0; c < channels.size(); ++c ) {
					const std::string channelWhere = where + ".channels[" + std::to_string(c) + "]";
					requireObject( *channels.at(c), channelWhere );
					AnimationChannel channel;
					channel.sampler = checkReference( requireIndex( *channels.at(c), "sampler", channelWhere ), animation.samplers,
													  "samplers", channelWhere + ".sampler" );
					const JsonValue& target = requireMember( *channels.at(c), "target", channelWhere );
					requireObject( target, channelWhere + ".target" );
					channel.node = optionalIndex( target, "node", channelWhere + ".target" );
					const std::string path = requireString( target, "path", channelWhere + ".target" );
					if ( path == "translation" )      channel.path = TargetPath::TRANSLATION;
					else if ( path == "rotation" )    channel.path = TargetPath::ROTATION;
					else if ( path == "scale" )       channel.path = TargetPath::SCALE;
					else if ( path == "weights" )     channel.path = TargetPath::WEIGHTS;
					else {
						warn( channelWhere + ".target.path \"" + path + "\" is not supported, the channel is ignored" );
						continue;
					}
					if ( channel.node == INVALID_INDEX ) {
						warn( channelWhere + " has no target node (an extension target?), the channel is ignored" );
						continue;
					}
					checkReference( channel.node, model_.nodes, "nodes", channelWhere + ".target.node" );
					const Node& node = model_.nodes[channel.node];
					if ( node.hasMatrix && channel.path != TargetPath::WEIGHTS ) {
						warn( channelWhere + " animates nodes[" + std::to_string(channel.node) + "] that has a matrix, the channel is ignored" );
						continue;
					}

					/// Output format and size depend on the target path.
					AnimationSampler& sampler = animation.samplers[channel.sampler];
					const Accessor& output = model_.accessors[sampler.output];
					uint32_t valueSize = 0;
					switch ( channel.path ) {
					case TargetPath::TRANSLATION:
					case TargetPath::SCALE:
						checkAccessorFormat( sampler.output, channelWhere + " output", { AccessorType::VEC3 }, { ComponentType::FLOAT,
											 ComponentType::BYTE, ComponentType::UNSIGNED_BYTE, ComponentType::SHORT, ComponentType::UNSIGNED_SHORT }, true );
						valueSize = 3;
						break;
					case TargetPath::ROTATION:
						checkAccessorFormat( sampler.output, channelWhere + " output", { AccessorType::VEC4 },
											 { ComponentType::FLOAT, ComponentType::BYTE, ComponentType::SHORT }, true );
						valueSize = 4;
						break;
					case TargetPath::WEIGHTS:
						checkAccessorFormat( sampler.output, channelWhere + " output", { AccessorType::SCALAR }, { ComponentType::FLOAT,
											 ComponentType::BYTE, ComponentType::UNSIGNED_BYTE, ComponentType::SHORT, ComponentType::UNSIGNED_SHORT }, true );
						if ( node.mesh == INVALID_INDEX || model_.meshes[node.mesh].primitives[0].targets.empty() )
							fail( channelWhere + " animates weights of nodes[" + std::to_string(channel.node) + "] that has no morph targets" );
						valueSize = (uint32_t)model_.meshes[node.mesh].primitives[0].targets.size();
						break;
					}
					if ( sampler.valueSize != 0 && sampler.valueSize != valueSize )
						fail( channelWhere + ": samplers[" + std::to_string(channel.sampler) + "] is shared by channels with different value sizes" );
					sampler.valueSize = valueSize;

					const uint64_t keys = sampler.times.size();
					const uint64_t expectedValues = keys * valueSize * (sampler.interpolation == Interpolation::CUBICSPLINE ? 3 : 1);
					if ( (uint64_t)output.count * componentsNumber( output.type ) != expectedValues )
						fail( channelWhere + ": output has " + std::to_string( (uint64_t)output.count * componentsNumber(output.type) ) +
							  " values, " + std::to_string(expectedValues) + " expected for " + std::to_string(keys) + " key frames" );
					if ( sampler.values.empty() )
						sampler.values = readAccessorAsFloats( model_, sampler.output );

					for ( const AnimationChannel& previous : animation.channels ) {
						if ( previous.node == channel.node && previous.path == channel.path )
							warn( channelWhere + " targets the same node and path as another channel, the last one wins" );
					}
					animation.channels.push_back( channel );
				}

				bool hasTimes = false;
				for ( const AnimationChannel& channel : animation.channels ) {
					const AnimationSampler& sampler = animation.samplers[channel.sampler];
					if ( sampler.times.empty() )
						continue;
					animation.startTime = hasTimes ? std::min( animation.startTime, sampler.times.front() ) : sampler.times.front();
					animation.endTime   = hasTimes ? std::max( animation.endTime, sampler.times.back() ) : sampler.times.back();
					hasTimes = true;
				}
				model_.animations.push_back( std::move(animation) );
			});
		}

		/// Index values, attribute counts and joint indices are checked so later code can index without bounds checks.
		void Loader::validateMeshData() {
			for ( uint32_t m = 0; m < model_.meshes.size(); ++m ) {
				for ( uint32_t p = 0; p < model_.meshes[m].primitives.size(); ++p ) {
					const Primitive& primitive = model_.meshes[m].primitives[p];
					const std::string where = "meshes[" + std::to_string(m) + "].primitives[" + std::to_string(p) + "]";

					const uint32_t vertexCount = model_.accessors[primitive.attributes[0].accessor].count;
					for ( const Attribute& attribute : primitive.attributes ) {
						if ( model_.accessors[attribute.accessor].count != vertexCount )
							fail( where + ": attributes have different counts (" + attribute.name + " has " +
								  std::to_string(model_.accessors[attribute.accessor].count) + ", " + primitive.attributes[0].name +
								  " has " + std::to_string(vertexCount) + ")" );
					}
					for ( const MorphTarget& target : primitive.targets ) {
						for ( const Attribute& attribute : target.attributes ) {
							if ( model_.accessors[attribute.accessor].count != vertexCount )
								fail( where + ": morph target attribute " + attribute.name + " count differs from the vertex count" );
						}
					}
					if ( primitive.indices != INVALID_INDEX ) {
						const std::vector<uint32_t> indices = readAccessorAsUints( model_, primitive.indices );
						for ( uint32_t index : indices ) {
							if ( index >= vertexCount )
								fail( where + ": index " + std::to_string(index) + " is out of range (" + std::to_string(vertexCount) + " vertices)" );
						}
					}
				}
			}

			/// JOINTS_n of a skinned mesh must reference joints of the node's skin.
			for ( uint32_t i = 0; i < model_.nodes.size(); ++i ) {
				const Node& node = model_.nodes[i];
				if ( node.skin == INVALID_INDEX || node.mesh == INVALID_INDEX )
					continue;
				const uint32_t jointsNumber = (uint32_t)model_.skins[node.skin].joints.size();
				for ( const Primitive& primitive : model_.meshes[node.mesh].primitives ) {
					for ( const Attribute& attribute : primitive.attributes ) {
						if ( attribute.name.rfind( "JOINTS_", 0 ) != 0 )
							continue;
						const std::vector<uint32_t> joints = readAccessorAsUints( model_, attribute.accessor );
						const std::string weightsName = "WEIGHTS_" + attribute.name.substr(7);
						const uint32_t weightsAccessor = primitive.findAttribute( weightsName );
						const std::vector<float> weights = weightsAccessor != INVALID_INDEX ? readAccessorAsFloats( model_, weightsAccessor ) : std::vector<float>();
						for ( size_t k = 0; k < joints.size(); ++k ) {
							const bool hasWeight = k < weights.size() && weights[k] != 0.0f;
							if ( joints[k] >= jointsNumber && hasWeight )
								fail( "nodes[" + std::to_string(i) + "]: " + attribute.name + " references joint " + std::to_string(joints[k]) +
									  ", skins[" + std::to_string(node.skin) + "] has " + std::to_string(jointsNumber) );
						}
					}
				}
			}
		}

		void Loader::decodeImages() {
			for ( uint32_t i = 0; i < model_.images.size(); ++i ) {
				Image& image = model_.images[i];
				const std::string where = "images[" + std::to_string(i) + "]";
				std::vector<uint8_t> uriData;
				const uint8_t* data = nullptr;
				uint64_t size = 0;
				if ( image.bufferView != INVALID_INDEX ) {
					const BufferView& view = model_.bufferViews[image.bufferView];
					data = model_.buffers[view.buffer].data.data() + view.byteOffset;
					size = view.byteLength;
				} else {
					std::string error;
					if ( !loadUri( image.uri, uriData, where, error ) ) {
						warn( error + ", the image is not decoded" );
						continue;
					}
					data = uriData.data();
					size = uriData.size();
				}

				std::string error;
				if ( !detail::decodeImage( data, size, image.width, image.height, image.pixels, error ) ) {
					warn( where + " is not decoded: " + error );
					continue;
				}
				image.isDecoded = true;
			}
		}

		void Loader::load( const uint8_t* data, uint64_t size ) {
			std::string jsonText;
			if ( size >= 4 && readU32LE(data) == GLB_MAGIC ) {
				parseGlb( data, size, jsonText );
			} else {
				uint64_t start = 0;
				if ( size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF ) {
					warn( "JSON starts with a UTF-8 byte order mark" );
					start = 3;
				}
				jsonText.assign( reinterpret_cast<const char*>(data + start), size - start );
			}
			if ( jsonText.find('\0') != std::string::npos )
				jsonText.resize( jsonText.find('\0') );                                   ///< The JSON parser stops at '\0' anyway

			Core::CJsonParser parser;
			parser.ReadText( jsonText, source_ );
			try {
				parser.Parse();
			} catch ( const std::runtime_error& error ) {
				fail( error.what() );
			}
			const JsonValue& root = *parser.GetRoot();
			requireObject( root, "root" );

			/// Order matters: every array is parsed after the arrays it references.
			parseAsset( root );
			parseExtensionLists( root );
			loadBuffers( root );
			parseBufferViews( root );
			parseAccessors( root );
			parseSamplers( root );
			parseImages( root );
			parseTextures( root );
			parseMaterials( root );
			parseMeshes( root );
			parseCameras( root );
			parseLights( root );
			parseNodes( root );
			parseSkins( root );
			parseScenes( root );
			parseAnimations( root );
			validateMeshData();
			if ( options_.decodeImages )
				decodeImages();
		}
	}

	Model loadModelFromMemory( const uint8_t* data, uint64_t size, const std::string& baseDirectory,
							   const std::string& sourceName, const LoadOptions& options ) {
		Model model;
		model.filePath = sourceName;
		if ( data == nullptr || size == 0 )
			detail::fail( sourceName, "no data" );
		Loader loader( model, sourceName, utf8Path(baseDirectory), options );
		loader.load( data, size );
		return model;
	}

	Model loadModel( const std::string& filePath, const LoadOptions& options ) {
		const std::filesystem::path path = utf8Path( filePath );
		std::vector<uint8_t> data;
		if ( !readFileBytes( path, data ) )
			detail::fail( filePath, "can't open the file" );
		if ( data.empty() )
			detail::fail( filePath, "the file is empty" );

		Model model;
		model.filePath = filePath;
		Loader loader( model, filePath, path.parent_path(), options );
		loader.load( data.data(), data.size() );
		return model;
	}

	/*
	  ===================================================
	  Accessor data
	  ===================================================
	*/
	namespace
	{
		double readComponent( const uint8_t* source, ComponentType componentType, bool normalized ) {
			switch ( componentType ) {
			case ComponentType::BYTE: {
				int8_t value; std::memcpy( &value, source, sizeof(value) );
				return normalized ? std::max( value / 127.0, -1.0 ) : value;
			}
			case ComponentType::UNSIGNED_BYTE: {
				uint8_t value; std::memcpy( &value, source, sizeof(value) );
				return normalized ? value / 255.0 : value;
			}
			case ComponentType::SHORT: {
				int16_t value; std::memcpy( &value, source, sizeof(value) );
				return normalized ? std::max( value / 32767.0, -1.0 ) : value;
			}
			case ComponentType::UNSIGNED_SHORT: {
				uint16_t value; std::memcpy( &value, source, sizeof(value) );
				return normalized ? value / 65535.0 : value;
			}
			case ComponentType::UNSIGNED_INT: {
				uint32_t value; std::memcpy( &value, source, sizeof(value) );
				return value;
			}
			case ComponentType::FLOAT: {
				float value; std::memcpy( &value, source, sizeof(value) );
				return value;
			}
			}
			return 0.0;
		}

		/// Pointer to the bytes of a buffer view range, with bounds checked against the buffer.
		const uint8_t* bufferViewData( const Model& model, uint32_t bufferView, uint64_t byteOffset, uint64_t byteLength, const std::string& where ) {
			if ( bufferView >= model.bufferViews.size() )
				detail::fail( model.filePath, where + " references a missing buffer view" );
			const BufferView& view = model.bufferViews[bufferView];
			if ( view.buffer >= model.buffers.size() || view.byteOffset + view.byteLength > model.buffers[view.buffer].data.size() ||
				 byteOffset + byteLength > view.byteLength )
				detail::fail( model.filePath, where + " is out of its buffer bounds" );
			return model.buffers[view.buffer].data.data() + view.byteOffset + byteOffset;
		}

		template<typename T, typename Convert>
		std::vector<T> readAccessor( const Model& model, uint32_t accessorIndex, Convert convert ) {
			const std::string where = "accessors[" + std::to_string(accessorIndex) + "]";
			if ( accessorIndex >= model.accessors.size() )
				detail::fail( model.filePath, where + " doesn't exist" );
			const Accessor& accessor = model.accessors[accessorIndex];
			const uint32_t components = componentsNumber( accessor.type );
			const uint32_t elementSize = detail::elementByteSize( accessor.componentType, accessor.type );
			std::vector<T> output( (size_t)accessor.count * components, T(0) );

			if ( accessor.bufferView != INVALID_INDEX && accessor.count > 0 ) {
				const uint32_t viewStride = accessor.bufferView < model.bufferViews.size() ? model.bufferViews[accessor.bufferView].byteStride : 0;
				const uint64_t stride = viewStride ? viewStride : elementSize;
				const uint8_t* base = bufferViewData( model, accessor.bufferView, accessor.byteOffset,
													  stride * (accessor.count - 1) + elementSize, where );
				for ( uint32_t element = 0; element < accessor.count; ++element ) {
					for ( uint32_t component = 0; component < components; ++component ) {
						const uint8_t* source = base + element * stride +
							detail::componentByteOffset( accessor.componentType, accessor.type, component );
						output[(size_t)element * components + component] = convert( source, accessor.componentType, accessor.normalized );
					}
				}
			}

			if ( accessor.isSparse ) {
				const AccessorSparse& sparse = accessor.sparse;
				const uint32_t indexSize = componentTypeSize( sparse.indicesComponentType );
				const uint8_t* indices = bufferViewData( model, sparse.indicesBufferView, sparse.indicesByteOffset,
														 (uint64_t)sparse.count * indexSize, where + ".sparse.indices" );
				const uint8_t* values = bufferViewData( model, sparse.valuesBufferView, sparse.valuesByteOffset,
														(uint64_t)sparse.count * elementSize, where + ".sparse.values" );
				uint64_t previous = 0;
				for ( uint32_t i = 0; i < sparse.count; ++i ) {
					const uint64_t index = (uint64_t)readComponent( indices + (uint64_t)i * indexSize, sparse.indicesComponentType, false );
					if ( index >= accessor.count || (i > 0 && index <= previous) )
						detail::fail( model.filePath, where + ".sparse.indices must be strictly increasing and less than count" );
					previous = index;
					for ( uint32_t component = 0; component < components; ++component ) {
						const uint8_t* source = values + (uint64_t)i * elementSize +
							detail::componentByteOffset( accessor.componentType, accessor.type, component );
						output[index * components + component] = convert( source, accessor.componentType, accessor.normalized );
					}
				}
			}
			return output;
		}
	}

	std::vector<float> readAccessorAsFloats( const Model& model, uint32_t accessor ) {
		return readAccessor<float>( model, accessor, []( const uint8_t* source, ComponentType type, bool normalized ) {
			return (float)readComponent( source, type, normalized );
		});
	}

	std::vector<uint32_t> readAccessorAsUints( const Model& model, uint32_t accessor ) {
		if ( accessor < model.accessors.size() ) {
			const ComponentType type = model.accessors[accessor].componentType;
			if ( type == ComponentType::FLOAT || model.accessors[accessor].normalized )
				detail::fail( model.filePath, "accessors[" + std::to_string(accessor) + "] must be an integer accessor" );
		}
		return readAccessor<uint32_t>( model, accessor, []( const uint8_t* source, ComponentType type, bool ) {
			const double value = readComponent( source, type, false );
			return value < 0.0 ? 0u : (uint32_t)value;
		});
	}
}
