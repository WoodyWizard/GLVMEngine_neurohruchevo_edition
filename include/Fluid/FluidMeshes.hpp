// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/// Simple meshes for the props around the fluid (tanks, bodies): triangle lists of (position, normal) vec4 pairs,
/// counter-clockwise seen from the outside.

#ifndef GLVM_FLUID_MESHES_HPP
#define GLVM_FLUID_MESHES_HPP

#include "Fluid/FluidMath.hpp"

#include <vector>

namespace GLVM::fluid
{
	/// Unit cube [-1, 1]^3; isOpen leaves out the top and the bottom (glass walls of a tank).
	inline void appendBoxMesh( std::vector<float>& vertices, bool isOpen ) {
		const float faces[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for ( const auto& n : faces ) {
			const Vec3 normal = { n[0], n[1], n[2] };
			if ( isOpen && normal.y != 0.0f )
				continue;
			const Vec3 up = std::fabs( normal.y ) > 0.5f ? Vec3{ 0, 0, 1 } : Vec3{ 0, 1, 0 };
			const Vec3 side = cross( up, normal );
			const Vec3 corners[4] = { normal + side * -1.0f + up * -1.0f, normal + side + up * -1.0f, normal + side + up, normal + side * -1.0f + up };
			const int order[6] = { 0, 1, 2, 0, 2, 3 };
			for ( int k : order )
				vertices.insert( vertices.end(), { corners[k].x, corners[k].y, corners[k].z, 1.0f, normal.x, normal.y, normal.z, 0.0f } );
		}
	}

	/// Unit sphere (latitude / longitude grid).
	inline void appendSphereMesh( std::vector<float>& vertices, int slices, int stacks ) {
		auto point = [&]( int slice, int stack ) {
			const float theta = 3.14159265f * stack / stacks, phi = 6.2831853f * slice / slices;
			return Vec3{ std::sin( theta ) * std::cos( phi ), std::cos( theta ), std::sin( theta ) * std::sin( phi ) };
		};
		for ( int stack = 0; stack < stacks; ++stack )
			for ( int slice = 0; slice < slices; ++slice ) {
				const Vec3 a = point( slice, stack ), b = point( slice + 1, stack ), c = point( slice + 1, stack + 1 ), d = point( slice, stack + 1 );
				for ( const Vec3& p : { a, b, c, a, c, d } )
					vertices.insert( vertices.end(), { p.x, p.y, p.z, 1.0f, p.x, p.y, p.z, 0.0f } );
			}
	}

	/// Square in the XZ plane, [-1, 1]^2, normal +Y.
	inline void appendFloorMesh( std::vector<float>& vertices ) {
		for ( const Vec3& p : { Vec3{ -1, 0, -1 }, Vec3{ 1, 0, 1 }, Vec3{ 1, 0, -1 }, Vec3{ -1, 0, -1 }, Vec3{ -1, 0, 1 }, Vec3{ 1, 0, 1 } } )
			vertices.insert( vertices.end(), { p.x, p.y, p.z, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f } );
	}

	/// Model matrix of an axis aligned box with the given center and half sizes (for the unit cube mesh).
	inline Mat4 boxModel( const Vec3& center, const Vec3& half ) {
		Mat4 model;
		model.at( 0, 0 ) = half.x;
		model.at( 1, 1 ) = half.y;
		model.at( 2, 2 ) = half.z;
		model.at( 0, 3 ) = center.x;
		model.at( 1, 3 ) = center.y;
		model.at( 2, 3 ) = center.z;
		return model;
	}
}

#endif
