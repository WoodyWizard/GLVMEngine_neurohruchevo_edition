// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "JsonParser.hpp"
#include "Vector.hpp"
#include "stack.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <filesystem>
#include <ostream>
#include <cassert>
#include <limits>

namespace GLVM::Core
{
	namespace
	{
		bool isJsonWhitespace(const char symbol) {
			return symbol == ' ' || symbol == '\t' || symbol == '\n' || symbol == '\r';
		}

		void appendUtf8(std::string& output, uint32_t codePoint) {
			if ( codePoint < 0x80 ) {
				output.push_back((char)codePoint);
			} else if ( codePoint < 0x800 ) {
				output.push_back((char)(0xC0 | (codePoint >> 6)));
				output.push_back((char)(0x80 | (codePoint & 0x3F)));
			} else if ( codePoint < 0x10000 ) {
				output.push_back((char)(0xE0 | (codePoint >> 12)));
				output.push_back((char)(0x80 | ((codePoint >> 6) & 0x3F)));
				output.push_back((char)(0x80 | (codePoint & 0x3F)));
			} else {
				output.push_back((char)(0xF0 | (codePoint >> 18)));
				output.push_back((char)(0x80 | ((codePoint >> 12) & 0x3F)));
				output.push_back((char)(0x80 | ((codePoint >> 6) & 0x3F)));
				output.push_back((char)(0x80 | (codePoint & 0x3F)));
			}
		}
	}

    bool CJsonParser::ReadFile(const char* _filePath) {
        std::ifstream jsonFileInputStream;
        std::stringstream jsonFileOutputStream;

		filePath_ = _filePath ? _filePath : "";
		pJsonFileData_ = nullptr;

        jsonFileInputStream.open(filePath_, std::ios::binary);
        if(jsonFileInputStream.good()) {
            jsonFileOutputStream << jsonFileInputStream.rdbuf();
            jsonFileInputStream.close();
            sJsonFileData_ = jsonFileOutputStream.str();
        } else {
            std::cerr << "Error of reading json file: " << filePath_ << std::endl;
            return false;
        }

        pJsonFileData_ = sJsonFileData_.c_str();
		return true;
    }

	void CJsonParser::ParseError(const std::string& message) const {
		throw std::runtime_error("JSON parser: " + filePath_ + ": " + message + " (offset " +
								 std::to_string(globalFileCounter_) + ")");
	}

	void CJsonParser::SkipWhitespace() {
		while ( isJsonWhitespace(pJsonFileData_[globalFileCounter_]) )
			++globalFileCounter_;
	}

	void CJsonParser::AddValue(const JsonValue& jsonValue) {
		if ( stackOfJsonValues_.GetSize() == 0 ) {
			if ( root_ != nullptr )
				ParseError("unexpected value after the root element");
			root_ = new JsonValue(jsonValue);
			return;
		}

		JsonValue* head = stackOfJsonValues_.GetHead();
		if ( head->type == JSON_OBJECT )
			(*head->value.object)[lastKey_.c_str()] = jsonValue;
		else
			head->value.array->Push(jsonValue);
	}

	void CJsonParser::OpenContainer(const JsonValue& container) {
		if ( stackOfJsonValues_.GetSize() == 0 ) {
			if ( root_ != nullptr )
				ParseError("unexpected value after the root element");
			root_ = new JsonValue;
			*root_ = container;
			stackOfJsonValues_.Push(root_);
			return;
		}

		JsonValue* head = stackOfJsonValues_.GetHead();
		if ( head->type == JSON_OBJECT ) {
			JsonValue& slot = (*head->value.object)[lastKey_.c_str()];
			slot = container;
			stackOfJsonValues_.Push(&slot);
		} else {
			head->value.array->Push(container);
			stackOfJsonValues_.Push(&head->value.array->GetHead());
		}
	}

	void CJsonParser::CloseContainer(JsonType expectedType) {
		if ( stackOfJsonValues_.GetSize() == 0 || stackOfJsonValues_.GetHead()->type != expectedType )
			ParseError(expectedType == JSON_OBJECT ? "unexpected '}'" : "unexpected ']'");
		stackOfJsonValues_.Pop();
	}

    void CJsonParser::Parse() {
		if ( pJsonFileData_ == nullptr )
			ParseError("no data to parse (the file was not read)");

		globalFileCounter_ = 0;

		while ( true ) {
			char currentChar = pJsonFileData_[globalFileCounter_];
			if ( isJsonWhitespace(currentChar) || currentChar == ',' ) {               ///< Separators between values
				++globalFileCounter_;
				continue;
			}
			if ( currentChar == '\0' )
				break;

			const bool insideObject = stackOfJsonValues_.GetSize() > 0 &&
				stackOfJsonValues_.GetHead()->type == JSON_OBJECT;

			if ( insideObject && currentChar != '}' ) {                                ///< Every object member starts with a key
				if ( currentChar != '"' )
					ParseError("object key expected");
				lastKey_ = StringParse();
				SkipWhitespace();
				if ( pJsonFileData_[globalFileCounter_] != ':' )
					ParseError("':' expected after object key \"" + lastKey_ + "\"");
				++globalFileCounter_;
				SkipWhitespace();
				currentChar = pJsonFileData_[globalFileCounter_];
				if ( currentChar == '\0' || currentChar == ',' || currentChar == '}' || currentChar == ']' )
					ParseError("value expected for key \"" + lastKey_ + "\"");
			}

			/// Token parsers leave the counter on the first character after the token, so no extra increment here
			if ( currentChar == '"' ) {
				AddValue(JsonValue(StringParse()));
			} else if ( (currentChar >= '0' && currentChar <= '9') || currentChar == '+' || currentChar == '-' ) {
				AddValue(NumberParse());
			} else if ( currentChar == 't' || currentChar == 'f' || currentChar == 'n' ) {
				const std::string boolOrNullString = BoolOrNullParse();

				if ( boolOrNullString == "true" ) {
					AddValue(JsonValue(true));
				} else if ( boolOrNullString == "false" ) {
					AddValue(JsonValue(false));
				} else if ( boolOrNullString == "null" ) {
					JsonValue jsonNull;
					jsonNull.type = JSON_NULL;
					jsonNull.value.null = NULL;
					AddValue(jsonNull);
				} else {
					ParseError("unknown literal \"" + boolOrNullString + "\"");
				}
			} else if ( currentChar == '{' ) {
				OpenContainer(CreateJsonHashMap());
				++globalFileCounter_;
			} else if ( currentChar == '[' ) {
				OpenContainer(CreateJsonArray());
				++globalFileCounter_;
			} else if ( currentChar == '}' ) {
				CloseContainer(JSON_OBJECT);
				++globalFileCounter_;
			} else if ( currentChar == ']' ) {
				CloseContainer(JSON_ARRAY);
				++globalFileCounter_;
			} else {
				ParseError(std::string("unexpected character '") + currentChar + "'");
			}
		}

		if ( stackOfJsonValues_.GetSize() != 0 )
			ParseError("unexpected end of file (unclosed object or array)");
		if ( root_ == nullptr )
			ParseError("empty document");
	}

	JsonValue CJsonParser::CreateJsonHashMap() {
		JsonValue jsonObject;
		jsonObject.type = JSON_OBJECT;
		jsonObject.value.object = new HashMap<JsonValue>;
		return jsonObject;
	}

	JsonValue CJsonParser::CreateJsonArray() {
		JsonValue jsonArray;
		jsonArray.type = JSON_ARRAY;
		jsonArray.value.array = new core::vector<JsonValue>;
		return jsonArray;
	}

	std::string CJsonParser::BoolOrNullParse() {
		std::string boolOrNullString = "";
		while (1) {
			const char currentChar = pJsonFileData_[globalFileCounter_];
			if (currentChar >= 'a' && currentChar <= 'z') {
				boolOrNullString.push_back(currentChar);
				++globalFileCounter_;
			} else
				return boolOrNullString;
		}
	}

	JsonValue CJsonParser::NumberParse() {
		const unsigned int start = globalFileCounter_;
		bool isInteger = true;
		while (1) {
			const char currentChar = pJsonFileData_[globalFileCounter_];
			if ((currentChar >= '0' && currentChar <= '9') || currentChar == '+' || currentChar == '-') {
				++globalFileCounter_;
			} else if (currentChar == '.' || currentChar == 'e' || currentChar == 'E') {
				isInteger = false;
				++globalFileCounter_;
			} else
				break;
		}

		const std::string numberAsString(pJsonFileData_ + start, globalFileCounter_ - start);
		const char* begin = numberAsString.c_str();
		char* end = nullptr;
		const double number = std::strtod(begin, &end);
		if ( end != begin + numberAsString.size() || numberAsString.empty() )
			ParseError("invalid number \"" + numberAsString + "\"");

		if ( isInteger && number >= (double)INT_MIN && number <= (double)INT_MAX )
			return JsonValue((int)number);

		return JsonValue(number);
	}

 	std::string CJsonParser::StringParse() {
		++globalFileCounter_;                                                           ///< Skip opening quote
		std::string localBuffer = "";
		while (1) {
			const char currentChar = pJsonFileData_[globalFileCounter_];
			if (currentChar == '\0') {
				ParseError("unterminated string");
			} else if (currentChar == '"') {
				++globalFileCounter_;
				return localBuffer;
			} else if (currentChar == '\\') {
				const char escaped = pJsonFileData_[globalFileCounter_ + 1];
				globalFileCounter_ += 2;
				switch (escaped) {
				case '"':  localBuffer.push_back('"');  break;
				case '\\': localBuffer.push_back('\\'); break;
				case '/':  localBuffer.push_back('/');  break;
				case 'b':  localBuffer.push_back('\b'); break;
				case 'f':  localBuffer.push_back('\f'); break;
				case 'n':  localBuffer.push_back('\n'); break;
				case 'r':  localBuffer.push_back('\r'); break;
				case 't':  localBuffer.push_back('\t'); break;
				case 'u': {
					uint32_t codePoint = 0;
					for ( int i = 0; i < 4; ++i ) {
						const char hex = pJsonFileData_[globalFileCounter_];
						codePoint <<= 4;
						if ( hex >= '0' && hex <= '9' )      codePoint |= (uint32_t)(hex - '0');
						else if ( hex >= 'a' && hex <= 'f' ) codePoint |= (uint32_t)(hex - 'a' + 10);
						else if ( hex >= 'A' && hex <= 'F' ) codePoint |= (uint32_t)(hex - 'A' + 10);
						else ParseError("invalid \\u escape");
						++globalFileCounter_;
					}
					appendUtf8(localBuffer, codePoint);
					break;
				}
				default:
					--globalFileCounter_;                                               ///< Don't step over a terminating '\0'
					ParseError("invalid escape sequence");
				}
			} else {
				localBuffer.push_back(currentChar);
				++globalFileCounter_;
			}
		}
	}

	void CJsonParser::SearchInJsonArray(core::vector<JsonValue>* arrayValue, const char* key_,
										core::vector<JsonValue>& resultVector) const {
		for ( unsigned int i = 0; i < arrayValue->GetSize(); ++i ) {
			if ( (*arrayValue)[i].type == JSON_OBJECT )
				SearchInJsonObject((*arrayValue)[i].value.object, key_, resultVector);

			if ( (*arrayValue)[i].type == JSON_ARRAY )
				SearchInJsonArray((*arrayValue)[i].value.array, key_, resultVector);
		}
	}

	void CJsonParser::SearchInJsonObject(HashMap<JsonValue>* mapValue, const char* key_,
										 core::vector<JsonValue>& resultVector) const {
		for ( unsigned int i = 0; i < mapValue->GetCapacity(); ++i ) {
			if ( mapValue->hashMap_[i] != nullptr ) {
				Node<JsonValue>* current = mapValue->hashMap_[i];
				while ( current != nullptr ) {
					std::string searchKey = key_;
					std::string currentKey = current->key_;
					if ( currentKey == searchKey ) {
						resultVector.Push(current->value_);
					}

					if ( current->value_.type == JSON_OBJECT )
						SearchInJsonObject(current->value_.value.object, key_, resultVector);

					if ( current->value_.type == JSON_ARRAY )
						SearchInJsonArray(current->value_.value.array, key_, resultVector);

					current = current->next_;
				}
			}
		}
	}

	core::vector<JsonValue> CJsonParser::Search(const char* key_) const {
		core::vector<JsonValue> resultVector;
		if ( root_ != nullptr && root_->type == JSON_OBJECT )
			SearchInJsonObject(root_->value.object, key_, resultVector);
		return resultVector;
	}

	/*
	  ===================================================
	  glTF helpers. Everything read from the file is validated: a broken or unsupported
	  file produces a std::runtime_error instead of out-of-bounds reads.
	  ===================================================
	*/
	namespace
	{
		[[noreturn]] void gltfError( const std::string& path, const std::string& message ) {
			throw std::runtime_error( "glTF loader: " + path + ": " + message );
		}

		const JsonValue& requireMember( const JsonValue& object, const char* key, const std::string& path, const std::string& context ) {
			const JsonValue* member = object.find(key);
			if ( member == nullptr )
				gltfError( path, context + "." + key + " is missing" );
			return *member;
		}

		const JsonValue& requireElement( const JsonValue& array, u32 index, const std::string& path, const std::string& context ) {
			const JsonValue* element = array.at(index);
			if ( element == nullptr )
				gltfError( path, context + "[" + std::to_string(index) + "] is missing (array size " +
						   std::to_string(array.size()) + ")" );
			return *element;
		}

		/// Non-negative integer that fits u32 (indices, counts, offsets).
		u32 toIndex( const JsonValue& value, const std::string& path, const std::string& context ) {
			if ( !value.isNumber() )
				gltfError( path, context + " must be a number" );
			const double number = value.asNumber();
			if ( number < 0.0 || number > (double)UINT32_MAX || number != std::floor(number) )
				gltfError( path, context + " must be a non-negative integer" );
			return (u32)number;
		}

		u32 requireIndex( const JsonValue& object, const char* key, const std::string& path, const std::string& context ) {
			return toIndex( requireMember(object, key, path, context), path, context + "." + key );
		}

		u32 optionalIndex( const JsonValue& object, const char* key, u32 fallback, const std::string& path, const std::string& context ) {
			const JsonValue* member = object.find(key);
			return member ? toIndex( *member, path, context + "." + key ) : fallback;
		}

		/// Reads up to `count` numbers of a node TRS array, ints and floats alike.
		bool readNumberArray( const JsonValue& object, const char* key, float* output, u32 count ) {
			const JsonValue* array = object.find(key);
			if ( array == nullptr || !array->isArray() )
				return false;
			for ( u32 i = 0; i < count && i < array->size(); ++i ) {
				if ( array->at(i)->isNumber() )
					output[i] = (float)array->at(i)->asNumber();
			}
			return true;
		}
	}

	template<typename T>
	bool isElementExist( const T element, const core::vector<T>& array ) {
		for( u32 n = 0; n < array.GetSize(); ++n ) {
			if( element == array[n] ) {
				return true;
			}
		}

		return false;
	}

	template<typename T>
	T getElementIndex( const T element, const core::vector<T>& array ) {
		for( u32 n = 0; n < array.GetSize(); ++n ) {
			if( element == array[n] ) {
				return n;
			}
		}

		return std::numeric_limits<T>::max();
	}

	struct ComponentType {
		enum Type {
			I8  = 5120,
			U8  = 5121,
			I16 = 5122,
			U16 = 5123,
			U32 = 5125,
			F32 = 5126
		};
	};

	u32 componentTypeSize( u32 componetType ) {
		switch ( componetType ) {
		case ComponentType::I8:
		case ComponentType::U8:
			return 1;
		case ComponentType::I16:
		case ComponentType::U16:
			return 2;
		case ComponentType::U32:
		case ComponentType::F32:
			return 4;
		default:
			return 0;
		}
	}

	u32 componentsNumberOfType( const std::string& type ) {
		if ( type == "SCALAR" ) return 1;
		if ( type == "VEC2" )   return 2;
		if ( type == "VEC3" )   return 3;
		if ( type == "VEC4" )   return 4;
		if ( type == "MAT4" )   return 16;
		return 0;                                                                       ///< MAT2/MAT3 are not used by the engine
	}

	/*
	  ===================================================
	  Meta data structs to binary buffer with actual data
	  ===================================================
	*/
	struct AccessorMetaData {
		u32 bufferView = 0;
		u32 byteOffset = 0;
		u32 componentType = 0;
		u32 count = 0;
		u32 componentsNumber = 0;                                                      ///< Components per element: SCALAR = 1 ... MAT4 = 16
		bool normalized = false;
		std::string type;
	};

	struct BufferViewMetaData {
        u32 byteLength = 0;
        u32 byteOffset = 0;
		u32 byteStride = 0;                                                            ///< 0 means tightly packed
	};

	[[nodiscard]] AccessorMetaData readAccessorMetaData( const Core::JsonValue& gltf, const u32 accessorIndex, const std::string& path ) {
		const std::string context = "accessors[" + std::to_string(accessorIndex) + "]";
		const JsonValue& accessor = requireElement( requireMember(gltf, "accessors", path, "root"), accessorIndex, path, "accessors" );
		if ( !accessor.isObject() )
			gltfError( path, context + " is not an object" );
		if ( accessor.find("sparse") != nullptr )
			gltfError( path, context + ": sparse accessors are not supported" );

		AccessorMetaData bufferMetaData;
		bufferMetaData.bufferView      = requireIndex( accessor, "bufferView", path, context );
		bufferMetaData.count           = requireIndex( accessor, "count", path, context );
		bufferMetaData.componentType   = requireIndex( accessor, "componentType", path, context );
		bufferMetaData.byteOffset      = optionalIndex( accessor, "byteOffset", 0, path, context );

		const JsonValue& type = requireMember( accessor, "type", path, context );
		if ( !type.isString() )
			gltfError( path, context + ".type must be a string" );
		bufferMetaData.type             = *type.value.string;
		bufferMetaData.componentsNumber = componentsNumberOfType( bufferMetaData.type );
		if ( bufferMetaData.componentsNumber == 0 )
			gltfError( path, context + ": unsupported accessor type " + bufferMetaData.type );
		if ( componentTypeSize( bufferMetaData.componentType ) == 0 )
			gltfError( path, context + ": unsupported componentType " + std::to_string(bufferMetaData.componentType) );

		const JsonValue* normalized = accessor.find("normalized");
		bufferMetaData.normalized = normalized != nullptr && normalized->isBoolean() && normalized->value.boolean;

		return bufferMetaData;
	}

	[[nodiscard]] BufferViewMetaData readBufferViewMetaData( const Core::JsonValue& gltf, const u32 bufferViewIndex, const std::string& path ) {
		const std::string context = "bufferViews[" + std::to_string(bufferViewIndex) + "]";
		const JsonValue& bufferView = requireElement( requireMember(gltf, "bufferViews", path, "root"), bufferViewIndex, path, "bufferViews" );
		if ( !bufferView.isObject() )
			gltfError( path, context + " is not an object" );

		BufferViewMetaData bufferViewMetaData;
		bufferViewMetaData.byteLength = requireIndex( bufferView, "byteLength", path, context );
		bufferViewMetaData.byteOffset = optionalIndex( bufferView, "byteOffset", 0, path, context );
		bufferViewMetaData.byteStride = optionalIndex( bufferView, "byteStride", 0, path, context );
		if ( requireIndex( bufferView, "buffer", path, context ) != 0 )
			gltfError( path, context + ": only buffers[0] is supported" );

		return bufferViewMetaData;
	}

	/// Reads one accessor into a flat array (count * componentsNumber values). Bounds and stride are validated.
	template< typename T >
	void readBinaryBufferData( const char* buffer, const u64 bufferSize, const AccessorMetaData& accessorMetaData,
							   const BufferViewMetaData& bufferViewMetaData, core::vector<T>& outputData, const std::string& path ) {
		const u32 componentSize = componentTypeSize( accessorMetaData.componentType );
		const u64 elementSize   = (u64)componentSize * accessorMetaData.componentsNumber;
		const u64 stride        = bufferViewMetaData.byteStride ? bufferViewMetaData.byteStride : elementSize;

		if ( stride < elementSize )
			gltfError( path, "bufferView byteStride is smaller than the accessor element size" );
		if ( (u64)bufferViewMetaData.byteOffset + bufferViewMetaData.byteLength > bufferSize )
			gltfError( path, "bufferView is out of the binary buffer bounds" );
		if ( accessorMetaData.count > 0 &&
			 (u64)accessorMetaData.byteOffset + (u64)(accessorMetaData.count - 1) * stride + elementSize > bufferViewMetaData.byteLength )
			gltfError( path, "accessor data is out of its bufferView bounds" );

		for ( u32 element = 0; element < accessorMetaData.count; ++element ) {
			const u64 elementOffset = (u64)bufferViewMetaData.byteOffset + accessorMetaData.byteOffset + element * stride;
			for ( u32 component = 0; component < accessorMetaData.componentsNumber; ++component ) {
				const char* source = buffer + elementOffset + (u64)component * componentSize;
				double value = 0.0;
				switch( accessorMetaData.componentType ) {
				case ComponentType::I8: {
					int8_t data; memcpy(&data, source, sizeof(data));
					value = accessorMetaData.normalized ? std::max(data / 127.0, -1.0) : data;
					break;
				}
				case ComponentType::U8: {
					uint8_t data; memcpy(&data, source, sizeof(data));
					value = accessorMetaData.normalized ? data / 255.0 : data;
					break;
				}
				case ComponentType::I16: {
					int16_t data; memcpy(&data, source, sizeof(data));
					value = accessorMetaData.normalized ? std::max(data / 32767.0, -1.0) : data;
					break;
				}
				case ComponentType::U16: {
					uint16_t data; memcpy(&data, source, sizeof(data));
					value = accessorMetaData.normalized ? data / 65535.0 : data;
					break;
				}
				case ComponentType::U32: {
					uint32_t data; memcpy(&data, source, sizeof(data));
					value = data;
					break;
				}
				case ComponentType::F32: {
					float data; memcpy(&data, source, sizeof(data));
					value = data;
					break;
				}
				}
				outputData.Push(static_cast<T>(value));
			}
		}
	}

	/// Validates accessor shape and reads it.
	template< typename T >
	void readAccessor( const JsonValue& gltf, const char* buffer, const u64 bufferSize, const u32 accessorIndex,
					   const u32 expectedComponents, core::vector<T>& outputData, const std::string& path,
					   const std::string& what, u32* elementsCount = nullptr ) {
		const AccessorMetaData   accessorMetaData   = readAccessorMetaData( gltf, accessorIndex, path );
		if ( expectedComponents != 0 && accessorMetaData.componentsNumber != expectedComponents )
			gltfError( path, what + ": accessor " + std::to_string(accessorIndex) + " has type " + accessorMetaData.type +
					   ", " + std::to_string(expectedComponents) + " components expected" );
		const BufferViewMetaData bufferViewMetaData = readBufferViewMetaData( gltf, accessorMetaData.bufferView, path );
		readBinaryBufferData( buffer, bufferSize, accessorMetaData, bufferViewMetaData, outputData, path );
		if ( elementsCount )
			*elementsCount = accessorMetaData.count;
	}

	void CJsonParser::LoadGLTF(const char* pathsGLTF_,
							   std::vector<float>& aVertexes_,
							   std::vector<uint32_t>& aIndices_,
							   core::vector<core::vector<mat4>>& jointMatricesPerMesh,
							   core::vector<float>& frames,
							   bool& noAnimations,
							   float& topY) {
		const std::string path = pathsGLTF_ ? pathsGLTF_ : "";
		std::cout << "path: " << path << std::endl;
		if ( !ReadFile(pathsGLTF_) )
			gltfError( path, "can't open the file (resources are looked up relative to the build directory)" );
		Parse();

		const Core::JsonValue& gltf = *GetRoot();
		if ( !gltf.isObject() )
			gltfError( path, "root is not a JSON object" );

		/*
		  ================================================================
		  Binary buffer. Its uri is resolved relative to the .gltf file.
		  ================================================================
		*/
		const JsonValue& buffer0 = requireElement( requireMember(gltf, "buffers", path, "root"), 0, path, "buffers" );
		const JsonValue& uri = requireMember( buffer0, "uri", path, "buffers[0]" );
		if ( !uri.isString() )
			gltfError( path, "buffers[0].uri must be a string (GLB files are not supported)" );
		const std::string binary_path = *uri.value.string;
		if ( binary_path.rfind("data:", 0) == 0 )
			gltfError( path, "embedded data: URIs are not supported, export with a separate .bin file" );
		const u32 full_byte_size = requireIndex( buffer0, "byteLength", path, "buffers[0]" );
		if ( full_byte_size == 0 )
			gltfError( path, "buffers[0].byteLength is 0" );

		const std::filesystem::path binaryFilePath = std::filesystem::path(path).parent_path() / binary_path;
		std::ifstream in_stream;
		in_stream.open(binaryFilePath, std::ios::binary);
		if ( !in_stream.is_open() )
			gltfError( path, "can't open binary buffer " + binaryFilePath.string() );
		std::vector<char> binaryBuffer(full_byte_size);
		in_stream.read(binaryBuffer.data(), full_byte_size);
		if ( (u64)in_stream.gcount() != full_byte_size )
			gltfError( path, "binary buffer " + binaryFilePath.string() + " is shorter than buffers[0].byteLength" );
		in_stream.close();
		const char* buffer   = binaryBuffer.data();
		const u64 bufferSize = binaryBuffer.size();

		/*
		  ================================================================
		  Geometry of meshes[0].primitives[0]
		  ================================================================
		*/
		const JsonValue& meshes    = requireMember( gltf, "meshes", path, "root" );
		const JsonValue& primitives = requireMember( requireElement(meshes, 0, path, "meshes"), "primitives", path, "meshes[0]" );
		const JsonValue& primitive = requireElement( primitives, 0, path, "meshes[0].primitives" );
		if ( meshes.size() > 1 || primitives.size() > 1 )
			std::cerr << "glTF loader: " << path << ": only meshes[0].primitives[0] is loaded, other meshes/primitives are ignored" << std::endl;
		const JsonValue& attributes = requireMember( primitive, "attributes", path, "meshes[0].primitives[0]" );

		core::vector<float> verticesPosition;
		u32 vertexCount = 0;
		readAccessor( gltf, buffer, bufferSize, requireIndex(attributes, "POSITION", path, "attributes"), 3,
					  verticesPosition, path, "POSITION", &vertexCount );

		core::vector<u32> indices;
		if ( primitive.find("indices") != nullptr ) {
			readAccessor( gltf, buffer, bufferSize, requireIndex(primitive, "indices", path, "meshes[0].primitives[0]"), 1,
						  indices, path, "indices" );
		} else {
			for ( u32 i = 0; i < vertexCount; ++i )                                          ///< Non-indexed geometry
				indices.Push(i);
		}
		for ( u32 i = 0; i < indices.GetSize(); ++i ) {
			if ( indices[i] >= vertexCount )
				gltfError( path, "index " + std::to_string(indices[i]) + " is out of range (vertex count " + std::to_string(vertexCount) + ")" );
		}

		core::vector<float> textureCoordinates;
		if ( attributes.find("TEXCOORD_0") != nullptr ) {
			u32 count = 0;
			readAccessor( gltf, buffer, bufferSize, requireIndex(attributes, "TEXCOORD_0", path, "attributes"), 2,
						  textureCoordinates, path, "TEXCOORD_0", &count );
			if ( count != vertexCount )
				gltfError( path, "TEXCOORD_0 count differs from POSITION count" );
		}

		core::vector<float> normals;
		if ( attributes.find("NORMAL") != nullptr ) {
			u32 count = 0;
			readAccessor( gltf, buffer, bufferSize, requireIndex(attributes, "NORMAL", path, "attributes"), 3,
						  normals, path, "NORMAL", &count );
			if ( count != vertexCount )
				gltfError( path, "NORMAL count differs from POSITION count" );
		}

		/*
		  ================================================================
		  Skin
		  ================================================================
		*/
		const JsonValue* skins = gltf.find("skins");
		const JsonValue* joints = nullptr;
		u32 numJoints = 0;
		core::vector<mat4> inverseBindMatrixSet;
		core::vector<core::vector<mat4>> jointMatrices;
		core::vector<float> weightsContainer;
		core::vector<int> jointsIndices;
		core::vector<core::vector<int>> children;

		if ( skins != nullptr && skins->size() > 0 ) {
			noAnimations = false;
			const JsonValue& skin = *skins->at(0);
			joints = &requireMember( skin, "joints", path, "skins[0]" );
			if ( !joints->isArray() || joints->size() == 0 )
				gltfError( path, "skins[0].joints must be a non-empty array" );
			numJoints = joints->size();

			const JsonValue& nodes = requireMember( gltf, "nodes", path, "root" );
			for ( unsigned int i = 0; i < numJoints; ++i ) {                                ///< Loop on joints
				const u32 jointIndexMapToNode = toIndex( *joints->at(i), path, "skins[0].joints[" + std::to_string(i) + "]" );
				const JsonValue& node = requireElement( nodes, jointIndexMapToNode, path, "nodes" );

				core::vector<int> local_children;
				const JsonValue* nodeChildren = node.find("children");
				if ( nodeChildren != nullptr && nodeChildren->isArray() ) {                  ///< Collect children indices
					for ( unsigned int c = 0; c < nodeChildren->size(); ++c ) {
						local_children.Push( (int)toIndex( *nodeChildren->at(c), path, "nodes[].children" ) );
					}
				}
				children.Push(local_children);                                              ///< Linearly put all children to every joint (may be empty)
			}

			if ( skin.find("inverseBindMatrices") != nullptr ) {
				core::vector<float> inverseBindMatricesData;
				u32 count = 0;
				readAccessor( gltf, buffer, bufferSize, requireIndex(skin, "inverseBindMatrices", path, "skins[0]"), 16,
							  inverseBindMatricesData, path, "inverseBindMatrices", &count );
				if ( count < numJoints )
					gltfError( path, "skins[0].inverseBindMatrices has fewer matrices than joints" );

				mat4 inverseBindMatrix(0.0f);
				for ( unsigned int n = 0; n < numJoints; ++n ) {
					for ( unsigned int g = 0; g < 4; ++g )
						for ( unsigned int j = 0; j < 4; ++j ) {
							inverseBindMatrix[g][j] = inverseBindMatricesData[n * 16 + g * 4 + j];            ///< Put row float data into mat4
						}
					inverseBindMatrixSet.Push(inverseBindMatrix);
				}
			} else {
				for ( unsigned int n = 0; n < numJoints; ++n )                              ///< glTF default: identity matrices
					inverseBindMatrixSet.Push(mat4(1.0f));
			}

			if ( attributes.find("JOINTS_0") != nullptr ) {
				u32 count = 0;
				readAccessor( gltf, buffer, bufferSize, requireIndex(attributes, "JOINTS_0", path, "attributes"), 4,
							  jointsIndices, path, "JOINTS_0", &count );
				if ( count != vertexCount )
					gltfError( path, "JOINTS_0 count differs from POSITION count" );
				for ( u32 i = 0; i < jointsIndices.GetSize(); ++i ) {
					if ( jointsIndices[i] < 0 || (u32)jointsIndices[i] >= numJoints )
						gltfError( path, "JOINTS_0 references joint " + std::to_string(jointsIndices[i]) +
								   ", skin has " + std::to_string(numJoints) );
				}
			}

			if ( attributes.find("WEIGHTS_0") != nullptr ) {
				u32 count = 0;
				readAccessor( gltf, buffer, bufferSize, requireIndex(attributes, "WEIGHTS_0", path, "attributes"), 4,
							  weightsContainer, path, "WEIGHTS_0", &count );
				if ( count != vertexCount )
					gltfError( path, "WEIGHTS_0 count differs from POSITION count" );
			}
		} else {
			noAnimations = true;
		}

		/*
		  ================================================================
		  Animation (animations[0], applied to the joints of skins[0])
		  ================================================================
		*/
		const JsonValue* animations = gltf.find("animations");

		if ( animations != nullptr && animations->size() > 0 && joints == nullptr ) {
			std::cerr << "glTF loader: " << path << ": animations without a skin are not supported and are ignored" << std::endl;
		}

		if ( animations != nullptr && animations->size() > 0 && joints != nullptr ) {
			const JsonValue& animation = *animations->at(0);
			const JsonValue& channels  = requireMember( animation, "channels", path, "animations[0]" );
			const JsonValue& samplers  = requireMember( animation, "samplers", path, "animations[0]" );

			core::vector<core::vector<float>> frameInputsTranslation;
			core::vector<core::vector<float>> translations;
			core::vector<core::vector<float>> frameInputsRotation;
			core::vector<core::vector<float>> rotations;
			core::vector<core::vector<float>> frameInputsScale;
			core::vector<core::vector<float>> scales;
			core::vector<u32> nodesMapTranslations;
			core::vector<u32> nodesMapRotations;
			core::vector<u32> nodesMapScales;

			for ( unsigned int i = 0; i < channels.size(); ++i ) {
				const std::string context = "animations[0].channels[" + std::to_string(i) + "]";
				const JsonValue& channel = *channels.at(i);
				const JsonValue& target  = requireMember( channel, "target", path, context );
				const JsonValue* targetPath = target.find("path");
				if ( target.find("node") == nullptr || targetPath == nullptr || !targetPath->isString() )
					continue;                                                               ///< Channel without a target node is allowed and ignored

				u32 outputComponents = 0;
				if ( *targetPath->value.string == "translation" || *targetPath->value.string == "scale" )
					outputComponents = 3;
				else if ( *targetPath->value.string == "rotation" )
					outputComponents = 4;
				else
					continue;                                                               ///< Morph target weights are not supported

				const u32 targetNode = requireIndex( target, "node", path, context + ".target" );
				const JsonValue& sampler = requireElement( samplers, requireIndex(channel, "sampler", path, context), path, "animations[0].samplers" );
				const JsonValue* interpolation = sampler.find("interpolation");
				if ( interpolation != nullptr && interpolation->isString() && *interpolation->value.string == "CUBICSPLINE" )
					std::cerr << "glTF loader: " << path << ": CUBICSPLINE interpolation is not supported, " << context << " is read as LINEAR" << std::endl;

				core::vector<float> input;
				core::vector<float> output;
				u32 inputCount = 0;
				u32 outputCount = 0;
				readAccessor( gltf, buffer, bufferSize, requireIndex(sampler, "input", path, "sampler"), 1, input, path, context + " input", &inputCount );
				readAccessor( gltf, buffer, bufferSize, requireIndex(sampler, "output", path, "sampler"), outputComponents, output, path, context + " output", &outputCount );
				if ( inputCount == 0 || outputCount == 0 ) {
					std::cerr << "glTF loader: " << path << ": " << context << " has no keyframes and is ignored" << std::endl;
					continue;
				}

				if ( outputComponents == 4 ) {
					frameInputsRotation.Push(input);
					rotations.Push(output);
					nodesMapRotations.Push(targetNode);
				} else if ( *targetPath->value.string == "translation" ) {
					frameInputsTranslation.Push(input);
					translations.Push(output);
					nodesMapTranslations.Push(targetNode);
				} else {
					frameInputsScale.Push(input);
					scales.Push(output);
					nodesMapScales.Push(targetNode);
				}
			}

			/*
			  ================================================================
			  Joint hierarchy: parent joint of every joint (children store node indices)
			  ================================================================
			*/
			core::vector<u32> parentJoint;
			for ( u32 j = 0; j < numJoints; ++j )
				parentJoint.Push(UINT32_MAX);
			for ( u32 p = 0; p < numJoints; ++p ) {
				for ( u32 c = 0; c < children[p].GetSize(); ++c ) {
					const u32 childJoint = getJointIndex( *joints, children[p][c] );
					if ( childJoint != UINT32_MAX && childJoint != p )
						parentJoint[childJoint] = p;
				}
			}

			for ( u32 j = 0; j < numJoints; ++j ) {
				if ( parentJoint[j] == UINT32_MAX )
					std::cout << "root joint: " << (*joints->value.array)[j].asNumber() << std::endl;
			}

			/// Path root -> ... -> joint for every joint; depth is limited to break cycles in broken files
			core::vector<core::vector<u32>> hierarchyByJoint;
			for ( u32 j = 0; j < numJoints; ++j ) {
				core::vector<u32> reversedPath;
				u32 current = j;
				reversedPath.Push(current);
				while ( parentJoint[current] != UINT32_MAX && reversedPath.GetSize() <= numJoints ) {
					current = parentJoint[current];
					reversedPath.Push(current);
				}
				if ( reversedPath.GetSize() > numJoints )
					gltfError( path, "joint hierarchy contains a cycle" );

				core::vector<u32> rootToJoint;
				for ( u32 k = reversedPath.GetSize(); k > 0; --k )
					rootToJoint.Push(reversedPath[k - 1]);
				hierarchyByJoint.Push(rootToJoint);
			}

			u32 translationFramesNumber = 0;
			for (u32 k = 0; k < frameInputsTranslation.GetSize(); ++k) {
				if (frameInputsTranslation[k].GetSize() > translationFramesNumber) {
					translationFramesNumber = frameInputsTranslation[k].GetSize();
				}
			}
			u32 rotationFramesNumber = 0;
			for (u32 k = 0; k < frameInputsRotation.GetSize(); ++k) {
				if (frameInputsRotation[k].GetSize() > rotationFramesNumber) {
					rotationFramesNumber = frameInputsRotation[k].GetSize();
				}
			}
			u32 scaleFramesNumber = 0;
			for (u32 k = 0; k < frameInputsScale.GetSize(); ++k) {
				if (frameInputsScale[k].GetSize() > scaleFramesNumber) {
					scaleFramesNumber = frameInputsScale[k].GetSize();
				}
			}

			const u32 framesMax = translationFramesNumber > scaleFramesNumber ?
				(translationFramesNumber > rotationFramesNumber
				 ? translationFramesNumber : rotationFramesNumber) :
				(scaleFramesNumber > rotationFramesNumber ? scaleFramesNumber : rotationFramesNumber);

			for (u32 k = 0; k < frameInputsTranslation.GetSize(); ++k) {
				if (frameInputsTranslation[k].GetSize() > frames.GetSize())
					frames = frameInputsTranslation[k];
			}
			for (u32 k = 0; k < frameInputsRotation.GetSize(); ++k) {
				if (frameInputsRotation[k].GetSize() > frames.GetSize())
					frames = frameInputsRotation[k];
			}
			for (u32 k = 0; k < frameInputsScale.GetSize(); ++k) {
				if (frameInputsScale[k].GetSize() > frames.GetSize())
					frames = frameInputsScale[k];
			}

			/*
			  ================================================================
			  Mapping joint_index -> channel_index for every TRS-type.
			  ================================================================
			*/
			core::vector<int> jointToTranslationCh;
			core::vector<int> jointToRotationCh;
			core::vector<int> jointToScaleCh;
			for (u32 k = 0; k < numJoints; ++k) {
				jointToTranslationCh.Push(-1);
				jointToRotationCh.Push(-1);
				jointToScaleCh.Push(-1);
			}
			for (u32 k = 0; k < nodesMapTranslations.GetSize(); ++k) {
				u32 jIdx = getJointIndex(*joints, (i32)nodesMapTranslations[k]);
				if (jIdx != UINT32_MAX) {
					jointToTranslationCh[jIdx] = (int)k;
				}
			}
			for (u32 k = 0; k < nodesMapRotations.GetSize(); ++k) {
				u32 jIdx = getJointIndex(*joints, (i32)nodesMapRotations[k]);
				if (jIdx != UINT32_MAX) {
					jointToRotationCh[jIdx] = (int)k;
				}
			}
			for (u32 k = 0; k < nodesMapScales.GetSize(); ++k) {
				u32 jIdx = getJointIndex(*joints, (i32)nodesMapScales[k]);
				if (jIdx != UINT32_MAX) {
					jointToScaleCh[jIdx] = (int)k;
				}
			}

			/*
			  =======================================================
			  Build animatedNodesMatricesAccumulator indexed
			  by joint-index (0..numJoints-1), framesMax matrices each
			  =======================================================
			*/
			const JsonValue& nodes = requireMember( gltf, "nodes", path, "root" );
			core::vector<core::vector<mat4>> animatedNodesMatricesAccumulator;
			for (unsigned int j = 0; j < numJoints; ++j) {
				int tIdx = jointToTranslationCh[j];
				int rIdx = jointToRotationCh[j];
				int sIdx = jointToScaleCh[j];

				/*
				  ============================================================
				  Local defaults fresh on every joint, for not make possible
				  to collect data from previous iterations.
				  ============================================================
				*/
				core::vector<float> defaultTranslations;
				core::vector<float> defaultRotations;
				core::vector<float> defaultScales;

				/// Static TRS from node (integers and floats). Used if channel not exists.
				const JsonValue& node = *nodes.at( toIndex( *joints->at(j), path, "skins[0].joints" ) );
				float staticTranslation[3] = { 0.f, 0.f, 0.f };
				float staticRotation[4]    = { 0.f, 0.f, 0.f, 1.f };
				float staticScale[3]       = { 1.f, 1.f, 1.f };
				readNumberArray( node, "translation", staticTranslation, 3 );
				readNumberArray( node, "rotation", staticRotation, 4 );
				readNumberArray( node, "scale", staticScale, 3 );

				if (tIdx < 0) {
					for (u32 f = 0; f < framesMax; ++f) {
						defaultTranslations.Push(staticTranslation[0]);
						defaultTranslations.Push(staticTranslation[1]);
						defaultTranslations.Push(staticTranslation[2]);
					}
				}
				if (rIdx < 0) {
					for (u32 f = 0; f < framesMax; ++f) {
						defaultRotations.Push(staticRotation[0]);
						defaultRotations.Push(staticRotation[1]);
						defaultRotations.Push(staticRotation[2]);
						defaultRotations.Push(staticRotation[3]);
					}
				}
				if (sIdx < 0) {
					for (u32 f = 0; f < framesMax; ++f) {
						defaultScales.Push(staticScale[0]);
						defaultScales.Push(staticScale[1]);
						defaultScales.Push(staticScale[2]);
					}
				}

				core::vector<float>& boneT =
					(tIdx >= 0) ? translations[tIdx] : defaultTranslations;
				core::vector<float>& boneR =
					(rIdx >= 0) ? rotations[rIdx] : defaultRotations;
				core::vector<float>& boneS =
					(sIdx >= 0) ? scales[sIdx] : defaultScales;

				/*
				  ===============================================================
				  Chennels can has verious number of frames; framesMax - gloabal
				  maximum. Clamp index to last valid chennel frame, for not run
				  out after vectors bounds.
				  ===============================================================
				*/
				const u32 tFrames = boneT.GetSize() / 3;
				const u32 rFrames = boneR.GetSize() / 4;
				const u32 sFrames = boneS.GetSize() / 3;
				core::vector<mat4> perFrameMatrices;
				const bool malformedJoint = (tFrames == 0 || rFrames == 0 || sFrames == 0);
				for (unsigned int i = 0; i < framesMax; ++i) {
					if ( malformedJoint ) {
						perFrameMatrices.Push(mat4(1.0f));                                  ///< Keep one matrix per frame so indices stay aligned
						continue;
					}

					const u32 ti = (i < tFrames) ? i : tFrames - 1;
					const u32 ri = (i < rFrames) ? i : rFrames - 1;
					const u32 si = (i < sFrames) ? i : sFrames - 1;
					mat4 frameTranslation(1.0f);
					mat4 frameScale(1.0f);
					for (unsigned int q = 0; q < 3; ++q) {
						frameTranslation[3][q] = boneT[ti * 3 + q];
						frameScale[q][q] = boneS[si * 3 + q];
					}
					Quaternion frameRotationQuaternion;
					mat4 frameRotation(1.0f);
					frameRotationQuaternion.x = boneR[ri * 4];
					frameRotationQuaternion.y = boneR[ri * 4 + 1];
					frameRotationQuaternion.z = boneR[ri * 4 + 2];
					frameRotationQuaternion.w = boneR[ri * 4 + 3];
					frameRotation =
						rotateQuaternion<float, 4>(frameRotationQuaternion);
					frameRotation.SelfTensorTranspose();
					mat4 localTransform = frameScale * frameRotation * frameTranslation;
					perFrameMatrices.Push(localTransform);
				}
				animatedNodesMatricesAccumulator.Push(perFrameMatrices);
			}

			/*
			  =============================================================
			  Final comstruction of joint-matrices.
			  Both arrays indexed by joint-index.
			  =============================================================
			*/
			for (unsigned int j = 0; j < numJoints; ++j) {
				core::vector<mat4> globalAllFrameNodeMatrix;
				const core::vector<u32>& hierarchy = hierarchyByJoint[j];
				for (unsigned int i = 0; i < framesMax; ++i) {
					mat4 rootTransform(1.0f);
					for (unsigned int b = 0; b + 1 < hierarchy.GetSize(); ++b) {           ///< All ancestors, root first
						rootTransform =
							animatedNodesMatricesAccumulator[hierarchy[b]][i]
							* rootTransform;
					}
					globalAllFrameNodeMatrix.Push(
						inverseBindMatrixSet[j]
						* animatedNodesMatricesAccumulator[j][i] * rootTransform
						);
				}
				jointMatrices.Push(globalAllFrameNodeMatrix);
			}
		}

		jointMatricesPerMesh = jointMatrices;

		/*
		  ================================================================
		  De-indexed vertex stream: position(3) normal(3) uv(2) [joints(4) weights(4) for skinned meshes].
		  Missing optional attributes get defaults so the layout never shifts.
		  ================================================================
		*/
		const bool skinned = joints != nullptr;
		topY = -999.999f;
		for ( uint32_t i = 0; i < indices.GetSize(); ++i ) {
			aIndices_.push_back(i);

			const u32 vertexIndex = indices[i];
			unsigned int index = vertexIndex * 3;
			vec3 position = { verticesPosition[index],
				verticesPosition[index + 1],
				verticesPosition[index + 2] };

			if ( position[1] > topY )
				topY = position[1];

			aVertexes_.push_back(position[0]);
			aVertexes_.push_back(position[1]);
			aVertexes_.push_back(position[2]);

			if ( normals.GetSize() > 0 ) {
				aVertexes_.push_back(normals[index]);
				aVertexes_.push_back(normals[index + 1]);
				aVertexes_.push_back(normals[index + 2]);
			} else {
				aVertexes_.push_back(0.0f);
				aVertexes_.push_back(1.0f);
				aVertexes_.push_back(0.0f);
			}

			index = vertexIndex * 2;
			if ( textureCoordinates.GetSize() > 0 ) {
				aVertexes_.push_back(textureCoordinates[index]);
				aVertexes_.push_back(textureCoordinates[index + 1]);
			} else {
				aVertexes_.push_back(0.0f);
				aVertexes_.push_back(0.0f);
			}

			if ( !skinned )
				continue;

			index = vertexIndex * 4;
			if ( jointsIndices.GetSize() > 0 ) {
				aVertexes_.push_back(jointsIndices[index]);
				aVertexes_.push_back(jointsIndices[index + 1]);
				aVertexes_.push_back(jointsIndices[index + 2]);
				aVertexes_.push_back(jointsIndices[index + 3]);
			} else {
				for ( int k = 0; k < 4; ++k )
					aVertexes_.push_back(0.0f);
			}

			if ( weightsContainer.GetSize() > 0 ) {
				aVertexes_.push_back(weightsContainer[index]);
				aVertexes_.push_back(weightsContainer[index + 1]);
				aVertexes_.push_back(weightsContainer[index + 2]);
				aVertexes_.push_back(weightsContainer[index + 3]);
			} else {
				aVertexes_.push_back(1.0f);
				aVertexes_.push_back(0.0f);
				aVertexes_.push_back(0.0f);
				aVertexes_.push_back(0.0f);
			}
		}
	}

	u32 CJsonParser::getJointIndex(const Core::JsonValue& joints, i32 searchingIndex) const {
		for ( unsigned int i = 0; i < joints.size(); ++i ) {
			const JsonValue* joint = joints.at(i);
			if ( joint->isNumber() && joint->asNumber() == (double)searchingIndex )
				return i;
		}

		return UINT32_MAX;
	}

	CJsonParser::~CJsonParser() {
		delete root_;
	}
}
