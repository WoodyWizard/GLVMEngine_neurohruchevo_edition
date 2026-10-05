// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/// Image decoding for the glTF loader: PNG and JPEG (the formats of the glTF core specification) with stb_image.

#include "GltfInternal.hpp"

#include <cstring>
#include <limits>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO                                                     ///< Data always comes from memory
#define STBI_FAILURE_USERMSG

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include "ThirdParty/stb_image.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace GLVM::gltf::detail
{
	bool decodeImage( const uint8_t* data, uint64_t size, uint32_t& width, uint32_t& height,
					  std::vector<uint8_t>& rgbaPixels, std::string& error ) {
		if ( data == nullptr || size == 0 ) {
			error = "no image data";
			return false;
		}
		if ( size > (uint64_t)std::numeric_limits<int>::max() ) {
			error = "image data is too large";
			return false;
		}

		int imageWidth = 0;
		int imageHeight = 0;
		int channels = 0;
		stbi_uc* pixels = stbi_load_from_memory( data, (int)size, &imageWidth, &imageHeight, &channels, STBI_rgb_alpha );
		if ( pixels == nullptr ) {
			const char* reason = stbi_failure_reason();
			error = reason ? reason : "unknown image format";
			return false;
		}

		width  = (uint32_t)imageWidth;
		height = (uint32_t)imageHeight;
		rgbaPixels.assign( pixels, pixels + (size_t)width * height * 4 );
		stbi_image_free( pixels );
		return true;
	}
}
