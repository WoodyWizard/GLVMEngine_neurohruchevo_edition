// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#include "WavefrontObjParser.hpp"
#include "Vector.hpp"
#include <charconv>
#include <stdexcept>
#include <system_error>

namespace GLVM::core
{
    CWaveFrontObjParser::CWaveFrontObjParser() {}

    const GLVM::core::vector<SVertex>& CWaveFrontObjParser::getCoordinateVertices() const { return coordinateVertices_; }
    const GLVM::core::vector<SVertex>& CWaveFrontObjParser::getTextureVertices()    const { return textureVertices_; }
	const GLVM::core::vector<SVertex>& CWaveFrontObjParser::getNormals()            const { return normals_; }
    const GLVM::core::vector<SFace>& CWaveFrontObjParser::getFaces()                const { return faces_; }

    bool CWaveFrontObjParser::ReadFile(const char* _filePath) {
        std::ifstream WavefrontObjFileInputStream;
        std::stringstream WavefrontObjFileOutputStream;

		filePath_   = _filePath ? _filePath : "";
		isFileRead_ = false;

        WavefrontObjFileInputStream.open(filePath_, std::ios::binary);
        if(WavefrontObjFileInputStream.good()) {
            WavefrontObjFileOutputStream << WavefrontObjFileInputStream.rdbuf();
            WavefrontObjFileInputStream.close();
            sWavefrontObjFileData = WavefrontObjFileOutputStream.str();
        } else {
            std::cerr << "Error of reading wavefront.obj file: " << filePath_ << std::endl;
            return false;
        }

		isFileRead_ = true;
		return true;
    }

	void CWaveFrontObjParser::ParseError(unsigned int lineNumber, const std::string& message) const {
		throw std::runtime_error("Wavefront .obj parser: " + filePath_ + ":" + std::to_string(lineNumber) + ": " + message);
	}

	float CWaveFrontObjParser::ParseFloat(std::string_view token, unsigned int lineNumber) const {
		if ( !token.empty() && token.front() == '+' )                                   ///< from_chars doesn't accept an explicit plus
			token.remove_prefix(1);

		float value = 0.0f;
		const std::from_chars_result result = std::from_chars(token.data(), token.data() + token.size(), value);
		if ( result.ec != std::errc() || result.ptr != token.data() + token.size() )
			ParseError(lineNumber, "invalid number \"" + std::string(token) + "\"");
		return value;
	}

	SVertex CWaveFrontObjParser::ParseVector(const std::vector<std::string_view>& tokens, unsigned int minComponents,
											 unsigned int lineNumber) const {
		if ( tokens.size() - 1 < minComponents )
			ParseError(lineNumber, "\"" + std::string(tokens[0]) + "\" needs at least " + std::to_string(minComponents) + " numbers");

		SVertex vertex;
		for ( unsigned int i = 1; i < tokens.size() && i <= 3; ++i )                   ///< Extra components (w, vertex colors) are ignored
			vertex[i - 1] = ParseFloat(tokens[i], lineNumber);
		return vertex;
	}

	int CWaveFrontObjParser::ResolveIndex(std::string_view token, int elementsCount, unsigned int lineNumber) const {
		int index = 0;
		const std::from_chars_result result = std::from_chars(token.data(), token.data() + token.size(), index);
		if ( result.ec != std::errc() || result.ptr != token.data() + token.size() || index == 0 )
			ParseError(lineNumber, "invalid face index \"" + std::string(token) + "\"");

		/// Positive indices are 1-based, negative ones are relative to the elements defined so far
		const int resolved = index > 0 ? index - 1 : elementsCount + index;
		if ( resolved < 0 )
			ParseError(lineNumber, "face index \"" + std::string(token) + "\" is out of range");
		return resolved;
	}

    void CWaveFrontObjParser::ParseFile() {
		if ( !isFileRead_ )
			throw std::runtime_error("Wavefront .obj parser: file was not read: " + filePath_);

		struct Corner { int vertex; int texture; int normal; unsigned int line; };                ///< -1 marks a missing index
		std::vector<Corner> triangles;                                                              ///< 3 corners per triangle
		std::vector<std::string_view> tokens;
		std::vector<Corner> polygon;

		const std::string_view data(sWavefrontObjFileData);
		size_t lineStart = 0;
		unsigned int lineNumber = 0;
		while ( lineStart < data.size() ) {
			size_t lineEnd = data.find('\n', lineStart);
			if ( lineEnd == std::string_view::npos )
				lineEnd = data.size();
			std::string_view line = data.substr(lineStart, lineEnd - lineStart);
			lineStart = lineEnd + 1;
			++lineNumber;

			const size_t commentStart = line.find('#');
			if ( commentStart != std::string_view::npos )
				line = line.substr(0, commentStart);

			tokens.clear();                                                                         ///< Split by spaces, tabs and '\r' (CRLF files)
			size_t position = 0;
			while ( position < line.size() ) {
				while ( position < line.size() && (line[position] == ' ' || line[position] == '\t' || line[position] == '\r') )
					++position;
				const size_t tokenStart = position;
				while ( position < line.size() && line[position] != ' ' && line[position] != '\t' && line[position] != '\r' )
					++position;
				if ( position > tokenStart )
					tokens.push_back(line.substr(tokenStart, position - tokenStart));
			}

			if ( tokens.empty() )
				continue;

			if ( tokens[0] == "v" ) {
				coordinateVertices_.Push(ParseVector(tokens, 3, lineNumber));
			} else if ( tokens[0] == "vt" ) {
				textureVertices_.Push(ParseVector(tokens, 1, lineNumber));
			} else if ( tokens[0] == "vn" ) {
				normals_.Push(ParseVector(tokens, 3, lineNumber));
			} else if ( tokens[0] == "f" ) {
				if ( tokens.size() < 4 )
					ParseError(lineNumber, "a face needs at least 3 vertices");

				polygon.clear();
				for ( size_t i = 1; i < tokens.size(); ++i ) {
					const std::string_view corner = tokens[i];
					const size_t firstSlash  = corner.find('/');
					const size_t secondSlash = firstSlash == std::string_view::npos ? std::string_view::npos : corner.find('/', firstSlash + 1);

					const std::string_view vertexPart  = corner.substr(0, firstSlash);
					std::string_view texturePart;
					std::string_view normalPart;
					if ( firstSlash != std::string_view::npos ) {
						texturePart = corner.substr(firstSlash + 1, secondSlash == std::string_view::npos ? std::string_view::npos : secondSlash - firstSlash - 1);
						if ( secondSlash != std::string_view::npos )
							normalPart = corner.substr(secondSlash + 1);
					}

					Corner faceCorner;
					faceCorner.line    = lineNumber;
					faceCorner.vertex  = ResolveIndex(vertexPart, (int)coordinateVertices_.GetSize(), lineNumber);
					faceCorner.texture = texturePart.empty() ? -1 : ResolveIndex(texturePart, (int)textureVertices_.GetSize(), lineNumber);
					faceCorner.normal  = normalPart.empty()  ? -1 : ResolveIndex(normalPart, (int)normals_.GetSize(), lineNumber);
					polygon.push_back(faceCorner);
				}

				for ( size_t k = 1; k + 1 < polygon.size(); ++k ) {                                  ///< Triangle fan: (0, k, k + 1)
					triangles.push_back(polygon[0]);
					triangles.push_back(polygon[k]);
					triangles.push_back(polygon[k + 1]);
				}
			}
			/// Other records (o, g, s, usemtl, mtllib, l, p ...) are not used by the engine
		}

		/// Validate indices against the final element counts (absolute indices may reference later elements)
		for ( const Corner& corner : triangles ) {
			if ( corner.vertex >= (int)coordinateVertices_.GetSize() )
				ParseError(corner.line, "vertex index " + std::to_string(corner.vertex + 1) + " is out of range");
			if ( corner.texture >= (int)textureVertices_.GetSize() )
				ParseError(corner.line, "texture coordinate index " + std::to_string(corner.texture + 1) + " is out of range");
			if ( corner.normal >= (int)normals_.GetSize() )
				ParseError(corner.line, "normal index " + std::to_string(corner.normal + 1) + " is out of range");
		}

		int defaultTextureIndex = -1;
		for ( size_t t = 0; t < triangles.size(); t += 3 ) {
			Corner* corners = &triangles[t];
			SFace face;

			for ( int j = 0; j < 3; ++j ) {
				if ( corners[j].texture < 0 ) {
					if ( defaultTextureIndex < 0 ) {
						defaultTextureIndex = (int)textureVertices_.GetSize();
						textureVertices_.Push(SVertex());
					}
					corners[j].texture = defaultTextureIndex;
				}
			}

			if ( corners[0].normal < 0 || corners[1].normal < 0 || corners[2].normal < 0 ) {          ///< Flat normal of the triangle
				const SVertex& p0 = coordinateVertices_[corners[0].vertex];
				const SVertex& p1 = coordinateVertices_[corners[1].vertex];
				const SVertex& p2 = coordinateVertices_[corners[2].vertex];
				const float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
				const float e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
				float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
				const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
				SVertex normal;
				if ( length > 0.0f ) {
					normal[0] = n[0] / length;
					normal[1] = n[1] / length;
					normal[2] = n[2] / length;
				} else {
					normal[1] = 1.0f;                                                                   ///< Degenerate triangle
				}
				const int normalIndex = (int)normals_.GetSize();
				normals_.Push(normal);
				for ( int j = 0; j < 3; ++j ) {
					if ( corners[j].normal < 0 )
						corners[j].normal = normalIndex;
				}
			}

			for ( int j = 0; j < 3; ++j ) {
				face[0].Push(corners[j].vertex);
				face[1].Push(corners[j].texture);
				face[2].Push(corners[j].normal);
			}
			faces_.Push(face);
		}
    }
}
