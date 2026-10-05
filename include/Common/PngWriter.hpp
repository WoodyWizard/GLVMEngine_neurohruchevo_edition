// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#ifndef GLVM_COMMON_PNG_WRITER_HPP
#define GLVM_COMMON_PNG_WRITER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace GLVM::core
{
	/// Writes 8 bit RGB pixels (rows top to bottom) as a PNG with stored (uncompressed) deflate blocks.
	bool writePng( const std::string& path, uint32_t width, uint32_t height, const std::vector<uint8_t>& rgb );
}

#endif
