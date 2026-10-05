// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

#include "Common/PngWriter.hpp"

#include <algorithm>
#include <fstream>

namespace GLVM::core
{
	namespace
	{
		uint32_t crc32( const uint8_t* data, size_t size, uint32_t crc = 0xFFFFFFFFu ) {
			static uint32_t table[256];
			static bool isTableReady = false;
			if ( !isTableReady ) {
				for ( uint32_t n = 0; n < 256; ++n ) {
					uint32_t c = n;
					for ( int k = 0; k < 8; ++k )
						c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
					table[n] = c;
				}
				isTableReady = true;
			}
			for ( size_t i = 0; i < size; ++i )
				crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
			return crc;
		}

		void appendU32( std::vector<uint8_t>& out, uint32_t value ) {
			for ( int shift = 24; shift >= 0; shift -= 8 )
				out.push_back( (uint8_t)(value >> shift) );
		}

		void appendChunk( std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data ) {
			appendU32( out, (uint32_t)data.size() );
			const size_t typeStart = out.size();
			out.insert( out.end(), type, type + 4 );
			out.insert( out.end(), data.begin(), data.end() );
			appendU32( out, crc32( out.data() + typeStart, out.size() - typeStart ) ^ 0xFFFFFFFFu );
		}
	}

	bool writePng( const std::string& path, uint32_t width, uint32_t height, const std::vector<uint8_t>& rgb ) {
		/// Raw scanlines with filter type 0.
		std::vector<uint8_t> raw;
		raw.reserve( (size_t)(width * 3 + 1) * height );
		for ( uint32_t y = 0; y < height; ++y ) {
			raw.push_back( 0 );
			raw.insert( raw.end(), rgb.begin() + (size_t)y * width * 3, rgb.begin() + (size_t)(y + 1) * width * 3 );
		}

		/// zlib stream of stored deflate blocks (at most 65535 bytes each) and the Adler-32 checksum.
		std::vector<uint8_t> zlib = { 0x78, 0x01 };
		for ( size_t offset = 0; offset < raw.size() || offset == 0; ) {
			const size_t size = std::min<size_t>( 65535, raw.size() - offset );
			const bool isLast = offset + size >= raw.size();
			zlib.push_back( isLast ? 1 : 0 );
			zlib.push_back( (uint8_t)(size & 0xFF) );
			zlib.push_back( (uint8_t)(size >> 8) );
			zlib.push_back( (uint8_t)(~size & 0xFF) );
			zlib.push_back( (uint8_t)((~size >> 8) & 0xFF) );
			zlib.insert( zlib.end(), raw.begin() + offset, raw.begin() + offset + size );
			offset += size;
			if ( isLast )
				break;
		}
		uint32_t a = 1, b = 0;
		for ( uint8_t value : raw ) {
			a = (a + value) % 65521u;
			b = (b + a) % 65521u;
		}
		appendU32( zlib, (b << 16) | a );

		std::vector<uint8_t> png = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
		std::vector<uint8_t> header;
		appendU32( header, width );
		appendU32( header, height );
		header.insert( header.end(), { 8, 2, 0, 0, 0 } );                       ///< 8 bit, RGB, deflate, no filter, no interlace
		appendChunk( png, "IHDR", header );
		appendChunk( png, "IDAT", zlib );
		appendChunk( png, "IEND", {} );

		std::ofstream file( path, std::ios::binary );
		if ( !file.is_open() )
			return false;
		file.write( reinterpret_cast<const char*>(png.data()), (std::streamsize)png.size() );
		return file.good();
	}
}
