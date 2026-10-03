#include "TextureLoadingUtils.hpp"
#include "TextureFormatStructs.hpp"
#include <algorithm>
#include <string>

namespace GLVM::core {

	namespace {
		[[noreturn]] void ddsError( const char* filename, const std::string& message ) {
			throw std::runtime_error( std::string("DDS loader: ") + filename + ": " + message );
		}
	}

	/// Loads a 2D BC7 texture (DX10 header) with its mip chain. Everything read from the file is validated.
	DDSData loadDDS(const char* filename) {
		constexpr uint32_t DDS_HEADER_SIZE            = 124;
		constexpr uint32_t DDS_PIXELFORMAT_SIZE       = 32;
		constexpr uint32_t DDPF_FOURCC                = 0x4;
		constexpr uint32_t DDS_DIMENSION_TEXTURE2D    = 3;
		constexpr uint32_t DDS_RESOURCE_MISC_TEXTURECUBE = 0x4;
		constexpr uint32_t DXGI_FORMAT_BC7_UNORM      = 98;
		constexpr uint32_t DXGI_FORMAT_BC7_UNORM_SRGB = 99;
		constexpr uint32_t MAX_TEXTURE_DIMENSION      = 16384;
		constexpr uint32_t BC7_BLOCK_SIZE             = 16;              ///< Bytes per 4x4 block

		std::ifstream file(
			filename,
			std::ios::binary | std::ios::ate
			);

		if (!file)
			ddsError(filename, "failed to open");

		const std::streamsize fileSize = file.tellg();

		file.seekg(0);

		char magic[4];

		file.read(magic, 4);

		if (file.gcount() != 4 || std::memcmp(magic, "DDS ", 4) != 0)
			ddsError(filename, "not a DDS file");

		DDS_HEADER header{};

		file.read(reinterpret_cast<char*>(&header), sizeof(header));

		if (file.gcount() != (std::streamsize)sizeof(header) || header.size != DDS_HEADER_SIZE ||
			header.pixelFormat.size != DDS_PIXELFORMAT_SIZE)
			ddsError(filename, "invalid DDS header");

		constexpr uint32_t FOURCC_DX10 = makeFourCC('D', 'X', '1', '0');

		if ( !(header.pixelFormat.flags & DDPF_FOURCC) || header.pixelFormat.fourCC != FOURCC_DX10 )
			ddsError(filename, "only DX10 header DDS files are supported (BC7)");

		DDS_HEADER_DXT10 dx10{};
		file.read(reinterpret_cast<char*>(&dx10), sizeof(dx10));
		if (file.gcount() != (std::streamsize)sizeof(dx10))
			ddsError(filename, "truncated DX10 header");

		if (dx10.dxgiFormat != DXGI_FORMAT_BC7_UNORM &&
			dx10.dxgiFormat != DXGI_FORMAT_BC7_UNORM_SRGB) {
			ddsError(filename, "DDS is not BC7");
		}
		if ( dx10.resourceDimension != DDS_DIMENSION_TEXTURE2D || dx10.arraySize != 1 ||
			 (dx10.miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE) )
			ddsError(filename, "only a single 2D texture is supported (no arrays, cube maps or volumes)");

		if ( header.width == 0 || header.height == 0 ||
			 header.width > MAX_TEXTURE_DIMENSION || header.height > MAX_TEXTURE_DIMENSION )
			ddsError(filename, "invalid texture size " + std::to_string(header.width) + "x" + std::to_string(header.height));

		DDSData result;

		result.width = header.width;
		result.height = header.height;

		result.mipLevels = header.mipMapCount ? header.mipMapCount : 1;

		uint32_t maxMipLevels = 1;
		for ( uint32_t size = std::max(header.width, header.height); size > 1; size /= 2 )
			++maxMipLevels;
		if ( result.mipLevels > maxMipLevels )
			ddsError(filename, "mipMapCount " + std::to_string(result.mipLevels) + " is too large for the texture size");

		/// Size of the whole mip chain in BC7 blocks
		uint64_t expectedDataSize = 0;
		for ( uint32_t level = 0; level < result.mipLevels; ++level ) {
			const uint64_t levelWidth  = std::max<uint64_t>(1, header.width >> level);
			const uint64_t levelHeight = std::max<uint64_t>(1, header.height >> level);
			expectedDataSize += ((levelWidth + 3) / 4) * ((levelHeight + 3) / 4) * BC7_BLOCK_SIZE;
		}

		const std::streamsize dataSize = fileSize - file.tellg();
		if ( dataSize < 0 || (uint64_t)dataSize < expectedDataSize )
			ddsError(filename, "texture data is truncated");

		result.data.resize(expectedDataSize);
		file.read(reinterpret_cast<char*>(result.data.data()), (std::streamsize)expectedDataSize);
		if ( file.gcount() != (std::streamsize)expectedDataSize )
			ddsError(filename, "failed to read texture data");

		return result;
	}

}; ///< namespace GLVM::core
