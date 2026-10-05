// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/// Helpers shared by the glTF loader sources, not a public interface.

#ifndef GLVM_GLTF_INTERNAL_HPP
#define GLVM_GLTF_INTERNAL_HPP

#include "Gltf/Gltf.hpp"
#include <string>
#include <vector>

namespace GLVM::gltf::detail
{
	[[noreturn]] void fail( const std::string& source, const std::string& message );

	/// Decodes a PNG or JPEG image into RGBA8. Returns false and fills error on failure.
	bool decodeImage( const uint8_t* data, uint64_t size, uint32_t& width, uint32_t& height,
					  std::vector<uint8_t>& rgbaPixels, std::string& error );

	/// Byte size of one accessor element, matrix columns are padded to 4 bytes.
	uint32_t elementByteSize( ComponentType componentType, AccessorType type );
	/// Byte offset of component index (column-major for matrices) inside an element.
	uint32_t componentByteOffset( ComponentType componentType, AccessorType type, uint32_t component );
}

#endif
