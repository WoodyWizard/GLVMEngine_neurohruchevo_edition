// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef JSON_PARSER
#define JSON_PARSER

#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include "Vector.hpp"
#include "HashMap.hpp"
#include <cmath>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <string.h>
#include "stack.hpp"
#include "typenames.hpp"

namespace GLVM::Core
{
    enum JsonType
    {
		JSON_INVALID_VALUE,
        JSON_OBJECT,
        JSON_FLOAT_NUMBER,
		JSON_INTEGER_NUMBER,
        JSON_STRING,
        JSON_BOOLEAN,
        JSON_NULL,
        JSON_ARRAY
    };

	struct JsonValue;

    union JsonVariant
    {
        std::string* string;
        double fNumber;
		int iNumber;
        bool boolean;
        void* null;
        GLVM::core::vector<JsonValue>* array;
        HashMap<JsonValue>* object;
        JsonVariant() { memset((void*)this, 0, sizeof(JsonVariant)); }          ///< All bytes are zeroed: reading an inactive member never gives garbage
		JsonVariant(const JsonVariant& object) {
			memcpy((void*)this, &object, sizeof(JsonVariant));
		}
		JsonVariant& operator=(const JsonVariant& object) {
			memcpy((void*)this, &object, sizeof(JsonVariant));
			return *this;
		}

        ~JsonVariant() {}
    };

    struct JsonValue
    {
        JsonVariant value;
        JsonType type;

		JsonValue() { type = JSON_INVALID_VALUE; }
		JsonValue(std::string _string) {
			type = JSON_STRING;
			value.string = new std::string(_string);
		}
		JsonValue(double _float) {
			type = JSON_FLOAT_NUMBER;
			value.fNumber = _float;
		}
		JsonValue(int _int) {
			type = JSON_INTEGER_NUMBER;
			value.iNumber = _int;
		}
		JsonValue(bool _bool) {
			type = JSON_BOOLEAN;
			value.boolean = _bool;
		}

		JsonValue(const JsonValue& _value) {
			type = JSON_INVALID_VALUE;
			CopyFrom(_value);
		}

		~JsonValue() {
			Release();
		}

		JsonValue& operator=(const JsonValue& _value) {
			if (this == &_value)
				return *this;

			JsonValue copy(_value);                     ///< Copy first: _value may be owned by this value
			Release();
			value = copy.value;
			type  = copy.type;
			copy.type = JSON_INVALID_VALUE;             ///< Ownership moved, the copy must not free anything
			return *this;
		}

		JsonValue& operator[](std::string key_) {
			const char* key = key_.c_str();
			switch (type) {
			case JSON_OBJECT:
				return (*value.object)[key];
				break;
			default:
				throw std::out_of_range("Type is not a json object");
				break;
			}
		}

		JsonValue& operator[](const unsigned int index_) {
			switch (type) {
			case JSON_ARRAY:
				if (index_ >= value.array->GetSize())
					throw std::out_of_range("Json array index is out of range");
				return (*value.array)[index_];
				break;
			default:
				throw std::out_of_range("Type is not a json array");
				break;
			}
		}

		/// Read-only member lookup: nullptr if this is not an object or the key is missing (never inserts).
		const JsonValue* find(const char* key) const {
			if (type != JSON_OBJECT)
				return nullptr;
			return value.object->Find(key);
		}

		/// Read-only array access: nullptr if this is not an array or the index is out of range.
		const JsonValue* at(const unsigned int index) const {
			if (type != JSON_ARRAY || index >= value.array->GetSize())
				return nullptr;
			return &(*value.array)[index];
		}

		/// Calls function(key, value) for every member of an object (in hash order), does nothing for other types.
		template<typename Function>
		void forEachMember(Function&& function) const {
			if (type != JSON_OBJECT)
				return;
			for (unsigned int i = 0; i < value.object->GetCapacity(); ++i) {
				for (const Node<JsonValue>* node = value.object->hashMap_[i]; node != nullptr; node = node->next_)
					function(node->key_, node->value_);
			}
		}

		/// Number of array elements, 0 for any other type.
		unsigned int size() const {
			return type == JSON_ARRAY ? value.array->GetSize() : 0;
		}

		/// Numeric value regardless of integer/float storage.
		double asNumber(double fallback = 0.0) const {
			if (type == JSON_INTEGER_NUMBER)
				return value.iNumber;
			if (type == JSON_FLOAT_NUMBER)
				return value.fNumber;
			return fallback;
		}

		bool isInvalid()  const { return type == JSON_INVALID_VALUE; }
		bool isObject()   const { return type == JSON_OBJECT; }
		bool isFloat()    const { return type == JSON_FLOAT_NUMBER; }
		bool isInterger() const { return type == JSON_INTEGER_NUMBER; }
		bool isNumber()   const { return type == JSON_INTEGER_NUMBER || type == JSON_FLOAT_NUMBER; }
		bool isString()   const { return type == JSON_STRING; }
		bool isBoolean()  const { return type == JSON_BOOLEAN; }
		bool isNull()     const { return type == JSON_NULL; }
		bool isArray()    const { return type == JSON_ARRAY; }

	private:
		void Release() {
			switch (type) {
			case JSON_OBJECT:
				delete value.object;
				break;
			case JSON_STRING:
				delete value.string;
				break;
			case JSON_ARRAY:
				delete value.array;
				break;
			default:
				break;
			}
			type = JSON_INVALID_VALUE;
		}

		void CopyFrom(const JsonValue& _value) {
			switch (_value.type) {
			case JSON_OBJECT:
				value.object = new HashMap<JsonValue>(*_value.value.object);
				break;
			case JSON_INTEGER_NUMBER:
				value.iNumber = _value.value.iNumber;
				break;
			case JSON_FLOAT_NUMBER:
				value.fNumber = _value.value.fNumber;
				break;
			case JSON_STRING:
				value.string = new std::string(*_value.value.string);
				break;
			case JSON_BOOLEAN:
				value.boolean = _value.value.boolean;
				break;
			case JSON_NULL:
				value.null = _value.value.null;
				break;
			case JSON_ARRAY:
				value.array = new core::vector<JsonValue>(*_value.value.array);
				break;
			default:
				break;
			}
			type = _value.type;
		}
    };

	/*
	  JSON parser and glTF 2.0 loader. All errors (missing file, malformed JSON, invalid or unsupported
	  glTF data) are reported with std::runtime_error that names the file.
	*/
    class CJsonParser
    {
        std::string sJsonFileData_;
        const char* pJsonFileData_ = nullptr;
        unsigned int globalFileCounter_ = 0;
		std::string filePath_;

		core::vector<JsonValue*> stackOfJsonValues_;
		JsonValue* root_ = nullptr;
	    std::string lastKey_ = "";

		void SearchInJsonArray(core::vector<JsonValue>* arrayValue, const char* key_,
							   core::vector<JsonValue>& resultVector) const;
		[[noreturn]] void ParseError(const std::string& message) const;
		void SkipWhitespace();
		void AddValue(const JsonValue& jsonValue);
		void OpenContainer(const JsonValue& container);
		void CloseContainer(JsonType expectedType);

    public:
		void SearchInJsonObject(HashMap<JsonValue>* mapValue, const char* key_,
								core::vector<JsonValue>& resultVector) const;

		CJsonParser() = default;
		CJsonParser(const CJsonParser&) = delete;
		CJsonParser& operator=(const CJsonParser&) = delete;
		~CJsonParser();
		JsonValue* GetRoot() { return root_; }
        bool ReadFile(const char* _filePath);                 ///< Returns false if the file can't be read
		void ReadText(const std::string& text, const std::string& sourceName);   ///< JSON from memory, sourceName is used in errors
        void Parse();                                          ///< Throws std::runtime_error on malformed JSON
		JsonValue CreateJsonHashMap();
		JsonValue CreateJsonArray();
		std::string BoolOrNullParse();
		JsonValue NumberParse();
		std::string StringParse();
		core::vector<JsonValue> Search(const char* key_) const;
		/// Loads a .gltf / .glb model in the engine vertex layout (see Gltf/GltfEngineAdapter.hpp). Throws std::runtime_error.
		void LoadGLTF(const char* pathsGLTF_,
					  std::vector<float>& aVertexes_,
					  std::vector<uint32_t>& aIndices_,
					  core::vector<core::vector<mat4>>& jointMatricesPerMesh,
					  core::vector<float>& frames,
					  bool& noAnimations,
					  float& topY);
    };
}

#endif
