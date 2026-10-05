// This file is part of Game Loop Versatile Modules (GLVM)
// License: http://opensource.org/licenses/MIT

/// Minimal vector math of the fluid module: column-major matrices with the GLSL layout, Vulkan clip space
/// (y down, depth 0 at the near plane and 1 at the far plane), right handed view space looking down -z.

#ifndef GLVM_FLUID_MATH_HPP
#define GLVM_FLUID_MATH_HPP

#include <cmath>

namespace GLVM::fluid
{
	struct Vec3 {
		float x = 0.0f, y = 0.0f, z = 0.0f;

		Vec3 operator+( const Vec3& o ) const { return { x + o.x, y + o.y, z + o.z }; }
		Vec3 operator-( const Vec3& o ) const { return { x - o.x, y - o.y, z - o.z }; }
		Vec3 operator*( float s ) const { return { x * s, y * s, z * s }; }
		Vec3 operator-() const { return { -x, -y, -z }; }
	};

	inline float dot( const Vec3& a, const Vec3& b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline Vec3 cross( const Vec3& a, const Vec3& b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	inline float length( const Vec3& a ) { return std::sqrt( dot( a, a ) ); }
	inline Vec3 normalize( const Vec3& a ) { const float l = length( a ); return l > 0.0f ? a * (1.0f / l) : Vec3{ 0.0f, 1.0f, 0.0f }; }

	struct Mat4 {
		float m[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };              ///< Column-major: element (row r, column c) is m[c * 4 + r]

		float& at( int row, int column ) { return m[column * 4 + row]; }
		float at( int row, int column ) const { return m[column * 4 + row]; }

		Mat4 operator*( const Mat4& o ) const {
			Mat4 result;
			for ( int c = 0; c < 4; ++c )
				for ( int r = 0; r < 4; ++r ) {
					float sum = 0.0f;
					for ( int k = 0; k < 4; ++k )
						sum += at( r, k ) * o.at( k, c );
					result.at( r, c ) = sum;
				}
			return result;
		}

		Vec3 transformPoint( const Vec3& p ) const {
			const float w = at( 3, 0 ) * p.x + at( 3, 1 ) * p.y + at( 3, 2 ) * p.z + at( 3, 3 );
			return Vec3{ at( 0, 0 ) * p.x + at( 0, 1 ) * p.y + at( 0, 2 ) * p.z + at( 0, 3 ),
						 at( 1, 0 ) * p.x + at( 1, 1 ) * p.y + at( 1, 2 ) * p.z + at( 1, 3 ),
						 at( 2, 0 ) * p.x + at( 2, 1 ) * p.y + at( 2, 2 ) * p.z + at( 2, 3 ) } * (1.0f / w);
		}

		/// Right handed view matrix looking from eye to target.
		static Mat4 lookAt( const Vec3& eye, const Vec3& target, const Vec3& up ) {
			const Vec3 f = normalize( target - eye );
			const Vec3 s = normalize( cross( f, up ) );
			const Vec3 u = cross( s, f );
			Mat4 view;
			view.at( 0, 0 ) = s.x;  view.at( 0, 1 ) = s.y;  view.at( 0, 2 ) = s.z;  view.at( 0, 3 ) = -dot( s, eye );
			view.at( 1, 0 ) = u.x;  view.at( 1, 1 ) = u.y;  view.at( 1, 2 ) = u.z;  view.at( 1, 3 ) = -dot( u, eye );
			view.at( 2, 0 ) = -f.x; view.at( 2, 1 ) = -f.y; view.at( 2, 2 ) = -f.z; view.at( 2, 3 ) = dot( f, eye );
			return view;
		}

		/// Perspective projection to Vulkan clip space: y down, depth 0 at near and 1 at far.
		static Mat4 perspective( float verticalFov, float aspect, float nearPlane, float farPlane ) {
			const float f = 1.0f / std::tan( 0.5f * verticalFov );
			Mat4 projection;
			projection.at( 0, 0 ) = f / aspect;
			projection.at( 1, 1 ) = -f;
			projection.at( 2, 2 ) = farPlane / (nearPlane - farPlane);
			projection.at( 2, 3 ) = farPlane * nearPlane / (nearPlane - farPlane);
			projection.at( 3, 2 ) = -1.0f;
			projection.at( 3, 3 ) = 0.0f;
			return projection;
		}

		/// Orthographic projection of the box [l, r] x [b, t] x [-n, -f] in view space.
		static Mat4 orthographic( float left, float right, float bottom, float top, float nearPlane, float farPlane ) {
			Mat4 projection;
			projection.at( 0, 0 ) = 2.0f / (right - left);
			projection.at( 1, 1 ) = -2.0f / (top - bottom);
			projection.at( 2, 2 ) = 1.0f / (nearPlane - farPlane);
			projection.at( 0, 3 ) = -(right + left) / (right - left);
			projection.at( 1, 3 ) = (top + bottom) / (top - bottom);
			projection.at( 2, 3 ) = nearPlane / (nearPlane - farPlane);
			return projection;
		}

		Mat4 inverse() const {
			const float* a = m;
			float inv[16];
			inv[0]  =  a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
			inv[4]  = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
			inv[8]  =  a[4] * a[9]  * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
			inv[12] = -a[4] * a[9]  * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
			inv[1]  = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
			inv[5]  =  a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
			inv[9]  = -a[0] * a[9]  * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
			inv[13] =  a[0] * a[9]  * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
			inv[2]  =  a[1] * a[6]  * a[15] - a[1] * a[7]  * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7]  - a[13] * a[3] * a[6];
			inv[6]  = -a[0] * a[6]  * a[15] + a[0] * a[7]  * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7]  + a[12] * a[3] * a[6];
			inv[10] =  a[0] * a[5]  * a[15] - a[0] * a[7]  * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7]  - a[12] * a[3] * a[5];
			inv[14] = -a[0] * a[5]  * a[14] + a[0] * a[6]  * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6]  + a[12] * a[2] * a[5];
			inv[3]  = -a[1] * a[6]  * a[11] + a[1] * a[7]  * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9]  * a[2] * a[7]  + a[9]  * a[3] * a[6];
			inv[7]  =  a[0] * a[6]  * a[11] - a[0] * a[7]  * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8]  * a[2] * a[7]  - a[8]  * a[3] * a[6];
			inv[11] = -a[0] * a[5]  * a[11] + a[0] * a[7]  * a[9]  + a[4] * a[1] * a[11] - a[4] * a[3] * a[9]  - a[8]  * a[1] * a[7]  + a[8]  * a[3] * a[5];
			inv[15] =  a[0] * a[5]  * a[10] - a[0] * a[6]  * a[9]  - a[4] * a[1] * a[10] + a[4] * a[2] * a[9]  + a[8]  * a[1] * a[6]  - a[8]  * a[2] * a[5];
			const float determinant = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
			Mat4 result;
			if ( determinant == 0.0f )
				return result;
			for ( int i = 0; i < 16; ++i )
				result.m[i] = inv[i] / determinant;
			return result;
		}
	};
}

#endif
