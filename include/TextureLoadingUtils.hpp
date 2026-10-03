#ifndef TEXTURE_LOADING_UTILS_HPP
#define TEXTURE_LOADING_UTILS_HPP

#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include "typenames.hpp"
#include "TextureFormatStructs.hpp"

namespace GLVM::core {
	constexpr uint32_t makeFourCC( char a, char b, char c, char d) {
		return
			static_cast<uint32_t>(static_cast<unsigned char>(a)) |
			(static_cast<uint32_t>(static_cast<unsigned char>(b)) << 8) |
			(static_cast<uint32_t>(static_cast<unsigned char>(c)) << 16) |
			(static_cast<uint32_t>(static_cast<unsigned char>(d)) << 24);
	}
	DDSData loadDDS(const char* filename);
}; ///< namespace GLVM::core

#endif
