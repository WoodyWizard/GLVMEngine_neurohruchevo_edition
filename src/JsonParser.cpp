// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "JsonParser.hpp"
#include "Gltf/GltfEngineAdapter.hpp"
#include "ShaderStructs.hpp"
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

	void CJsonParser::ReadText(const std::string& text, const std::string& sourceName) {
		filePath_      = sourceName;
		sJsonFileData_ = text;
		pJsonFileData_ = sJsonFileData_.c_str();
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
					auto readHex4 = [this]() {
						uint32_t codeUnit = 0;
						for ( int i = 0; i < 4; ++i ) {
							const char hex = pJsonFileData_[globalFileCounter_];
							codeUnit <<= 4;
							if ( hex >= '0' && hex <= '9' )      codeUnit |= (uint32_t)(hex - '0');
							else if ( hex >= 'a' && hex <= 'f' ) codeUnit |= (uint32_t)(hex - 'a' + 10);
							else if ( hex >= 'A' && hex <= 'F' ) codeUnit |= (uint32_t)(hex - 'A' + 10);
							else ParseError("invalid \\u escape");
							++globalFileCounter_;
						}
						return codeUnit;
					};
					uint32_t codePoint = readHex4();
					/// Characters outside the BMP are written as a UTF-16 surrogate pair: 😀
					if ( codePoint >= 0xD800 && codePoint <= 0xDBFF && pJsonFileData_[globalFileCounter_] == '\\' &&
						 pJsonFileData_[globalFileCounter_ + 1] == 'u' ) {
						globalFileCounter_ += 2;
						const uint32_t lowSurrogate = readHex4();
						if ( lowSurrogate < 0xDC00 || lowSurrogate > 0xDFFF )
							ParseError("invalid UTF-16 surrogate pair in \\u escape");
						codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (lowSurrogate - 0xDC00);
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
	  glTF models for the engine: the file is loaded by the glTF 2.0 loader (src/Gltf) and baked into
	  the engine vertex layout: position(3) normal(3) uv(2) [joints(4) weights(4) for animated models].
	  ===================================================
	*/
	void CJsonParser::LoadGLTF(const char* pathsGLTF_,
							   std::vector<float>& aVertexes_,
							   std::vector<uint32_t>& aIndices_,
							   core::vector<core::vector<mat4>>& jointMatricesPerMesh,
							   core::vector<float>& frames,
							   bool& noAnimations,
							   float& topY) {
		const std::string path = pathsGLTF_ ? pathsGLTF_ : "";
		std::cout << "path: " << path << std::endl;

		gltf::LoadOptions loadOptions;
		loadOptions.decodeImages = false;                                               ///< Engine textures come from the texture manager
		const gltf::Model model = gltf::loadModel( path, loadOptions );

		gltf::EngineBakeOptions bakeOptions;
		bakeOptions.maxJoints = MAX_JOINTS_NUMBER;
		const gltf::EngineMesh mesh = gltf::bakeForEngine( model, bakeOptions );

		for ( const std::string& warning : model.warnings )
			std::cerr << "glTF loader: " << path << ": " << warning << std::endl;
		for ( const std::string& warning : mesh.warnings )
			std::cerr << "glTF loader: " << path << ": " << warning << std::endl;

		aVertexes_.insert( aVertexes_.end(), mesh.vertices.begin(), mesh.vertices.end() );
		const uint32_t firstIndex = (uint32_t)aIndices_.size();
		for ( uint32_t index : mesh.indices )
			aIndices_.push_back( firstIndex + index );
		noAnimations = !mesh.isAnimated;
		topY = mesh.topY;

		/// glTF matrices are column-major: column c is engine matrix row c (the engine multiplies row vectors).
		jointMatricesPerMesh.clear();
		for ( const std::vector<gltf::Mat4>& jointFrames : mesh.jointMatrices ) {
			core::vector<mat4> engineFrames;
			for ( const gltf::Mat4& matrix : jointFrames ) {
				mat4 engineMatrix(1.0f);
				for ( unsigned int column = 0; column < 4; ++column )
					for ( unsigned int row = 0; row < 4; ++row )
						engineMatrix[column][row] = matrix.m[column * 4 + row];
				engineFrames.Push( engineMatrix );
			}
			jointMatricesPerMesh.Push( engineFrames );
		}

		frames.clear();
		for ( float time : mesh.frameTimes )
			frames.Push( time );
	}

	CJsonParser::~CJsonParser() {
		delete root_;
	}
}
