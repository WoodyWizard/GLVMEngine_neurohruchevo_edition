// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef WAVEFRONT_OBJ_PARSER
#define WAVEFRONT_OBJ_PARSER

#include <string>
#include <string_view>
#include "Vector.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <cmath>
#include <cassert>
#include <mutex>

#include <chrono>
#include <thread>

namespace GLVM::core
{
    class SVertex
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

	public:
        float& operator[](const unsigned int _iIndex) {
            assert(_iIndex < 3 && "Wrong index");
            switch(_iIndex) {
            default:
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            }
        }

		float operator[](const unsigned int _iIndex) const {
            assert(_iIndex < 3 && "Wrong index");
            switch(_iIndex) {
            default:
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            }
        }
    };

	/// One triangle. [0] - position indices, [1] - texture coordinate indices, [2] - normal indices.
	/// All indices are 0-based and point to existing elements of the parser containers.
    class SFace
    {
        GLVM::core::vector<int> vertexIndex;
        GLVM::core::vector<int> textureIndex;
        GLVM::core::vector<int> normalIndex;

	public:
        GLVM::core::vector<int>& operator[](const unsigned int _iIndex) {
            assert(_iIndex < 3 && "Wrong index");
            switch(_iIndex) {
            default:
            case 0:
                return vertexIndex;
            case 1:
                return textureIndex;
            case 2:
                return normalIndex;
            }
        }

		const GLVM::core::vector<int>& operator[](const unsigned int _iIndex) const {
            assert(_iIndex < 3 && "Wrong index");
            switch(_iIndex) {
            default:
            case 0:
                return vertexIndex;
            case 1:
                return textureIndex;
            case 2:
                return normalIndex;
            }
        }
    };

	/*
	  Wavefront .obj reader: v, vt, vn and f records. Supports v, v/vt, v//vn and v/vt/vn face corners,
	  negative (relative) indices, CRLF line endings, comments and polygons (triangulated as a fan).
	  Missing texture coordinates get (0, 0), missing normals get the flat face normal.
	  Malformed data is reported with std::runtime_error.
	*/
    class CWaveFrontObjParser
    {
        GLVM::core::vector<SVertex> coordinateVertices_;
        GLVM::core::vector<SVertex> textureVertices_;
		GLVM::core::vector<SVertex> normals_;
        GLVM::core::vector<SFace> faces_;

        std::string sWavefrontObjFileData;
		std::string filePath_;
		bool isFileRead_ = false;

		[[noreturn]] void ParseError(unsigned int lineNumber, const std::string& message) const;
		float ParseFloat(std::string_view token, unsigned int lineNumber) const;
		SVertex ParseVector(const std::vector<std::string_view>& tokens, unsigned int minComponents, unsigned int lineNumber) const;
		int ResolveIndex(std::string_view token, int elementsCount, unsigned int lineNumber) const;

    public:
        CWaveFrontObjParser();

        [[nodiscard]] const GLVM::core::vector<SVertex>& getCoordinateVertices() const;
        [[nodiscard]] const GLVM::core::vector<SVertex>& getTextureVertices() const;
		[[nodiscard]] const GLVM::core::vector<SVertex>& getNormals() const;
        [[nodiscard]] const GLVM::core::vector<SFace>&   getFaces() const;

        bool ReadFile(const char* _filePath);                 ///< Returns false if the file can't be read
        void ParseFile();                                      ///< Throws std::runtime_error on malformed data
    };
}

#endif
