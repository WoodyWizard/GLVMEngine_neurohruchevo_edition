// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef VERTEX_MATH
#define VERTEX_MATH

#include <cassert>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <ostream>

#define PI 3.14159265

template< class T>
struct point2D {
	T x;
	T y;
};

template< typename T> 
std::ostream& operator<<(std::ostream& ostream, const point2D<T>& point) {
	ostream << "x: " << point.x << " y: " << point.y;
	return ostream;	
}

struct point3D {
	float x;
	float y;
	float z;
};

template <typename T>
T clamp( T lowerThreshold, T targetValue, T upperThreshold ) {
	if( targetValue < lowerThreshold ) {
		targetValue = lowerThreshold;
	} else if( targetValue > upperThreshold ) {
		targetValue = upperThreshold;
	}

	return targetValue;
}

template <class T2, int var2>
class Vector;

template <class T, int var>
class Matrix
{
	T m_matrix[var][var] {};
public:
	Matrix(T arg = 0)
	{
		for(int i = 0; i < var; ++i)
		{
			m_matrix[i][i] = arg;
			// if(i == (var-1))
			// 	m_matrix[i][i] = 1;
		}
  	}

    Matrix(Vector<T, var> _vector1, Vector<T, var> _vector2,
           Vector<T, var> _vector3, Vector<T, var> _vector4)
    {
        m_matrix[0][0] = _vector1[0];
        m_matrix[0][1] = _vector1[1];
        m_matrix[0][2] = _vector1[2];
        m_matrix[0][3] = _vector1[3];

        m_matrix[1][0] = _vector2[0];
        m_matrix[1][1] = _vector2[1];
        m_matrix[1][2] = _vector2[2];
        m_matrix[1][3] = _vector2[3];

        m_matrix[2][0] = _vector3[0];
        m_matrix[2][1] = _vector3[1];
        m_matrix[2][2] = _vector3[2];
        m_matrix[2][3] = _vector3[3];

        m_matrix[3][0] = _vector4[0];
        m_matrix[3][1] = _vector4[1];
        m_matrix[3][2] = _vector4[2];
        m_matrix[3][3] = _vector4[3];
    }

    void SelfTensorTranspose()
    {
        T tempMatrix[var][var];
        for(int p = 0; p < var; ++p)
            for(int u = 0; u < var; ++u)
            {
                tempMatrix[p][u] = m_matrix[u][p];
            }
        for(int j = 0; j < var; ++j)
            for(int z = 0; z < var; ++z)
            {
                this->m_matrix[j][z] = tempMatrix[j][z];
            }
    }

    void SelfIdentity()
    {
        for(int i = 0; i < var; ++i)
            for(int j = 0; j < var; ++j)
            {
                if(i == j)
                    this->m_matrix[i][j] = 1.0f;
                else
                    this->m_matrix[i][j] = 0.0f;
            }
    }

	Matrix<T, var> operator+(const Matrix& matrix);
	Matrix<T, var> operator*(const T scalar);
	Matrix<T, var> operator*(const Matrix& matrix);
	T* operator[](const int index);
	const T* operator[](const int index) const;
	template<class T2, int var2>
	Vector<T2, var2> operator*(const Vector<T2, var2>& vector);
};

typedef Vector<float, 1> vec1;
typedef Vector<float, 2> vec2;
typedef Vector<float, 3> vec3;
typedef Vector<float, 4> vec4;

typedef Matrix<float, 1> mat1;
typedef Matrix<float, 2> mat2;
typedef Matrix<float, 3> mat3;
typedef Matrix<float, 4> mat4;

template <class T, int var>
Matrix<T, var> Matrix<T, var>::operator+(const Matrix& matrix) {
	Matrix<T, var> tempMatrix;
	for(int i = 0; i < var; ++i)
		for(int j = 0; j < var; ++j)
			tempMatrix[i][j] = this->m_matrix[i][j] + matrix.m_matrix[i][j];

	return tempMatrix;
}
	
template <class T, int var>
Matrix<T, var> Matrix<T, var>::operator*(const T scalar) {
	Matrix<T, var> tempMatrix;
	for(int i = 0; i < var; ++i)
		for(int j = 0; j < var; ++j)
			tempMatrix[i][j] = this->m_matrix[i][j] * scalar;

	return tempMatrix;
}

template<class T, int var>
Matrix<T, var> Matrix<T, var>::operator*(const Matrix& matrix)
{
	Matrix<T, var> tempMatrix;
	for(int i = 0; i < var; ++i)
		for(int j = 0; j < var; ++j)
			for(int n = 0; n < var; ++n)
			{
				tempMatrix.m_matrix[i][j] += m_matrix[i][n] * matrix.m_matrix[n][j];
			}
	return tempMatrix;
}

template <class T, int var>
T* Matrix<T, var>::operator[](const int index)
{
	return m_matrix[index];
}

template <class T, int var>
const T* Matrix<T, var>::operator[](const int index) const
{
	return m_matrix[index];
}

template <int var2>
std::ostream& operator<<(std::ostream& ostream, const Matrix<float, var2>& matrix) {
	for ( int i = 0; i < var2; ++i) {
		ostream << std::endl;
		for ( int j = 0; j < var2; ++j) {
			ostream << matrix[i][j] << " ";
		}
	}
	return ostream;
}

template <class T, int var>
template <class T2, int var2>
Vector<T2, var2> Matrix<T, var>::operator*(const Vector<T2, var2>& vector)
{
	static_assert(var == var2, "Size error");
	Vector<T2, var2> tempVector;
	for(int i = 0; i < var2; ++i)
		for(int j = 0; j < var; ++j)
		{
			tempVector[i] += m_matrix[i][j] * vector[j];
		}
	return tempVector;
}

template <class T2, int dim>
class Vector
{
	public: T2 m_vector[dim] {};
public:
	Vector(T2 x = 0, T2 y = 0, T2 z = 0, T2 w = 0)
	{
		T2 array[4] = {x, y, z, w};
		for(int i = 0; i < dim; ++i)
		{
			m_vector[i] = array[i];
		}
	}
	
    T2& operator[](const int index);
	const T2& operator[](const int index) const;
	template<class T, int dim2>
	Vector<T2, dim> operator*(const Matrix<T, dim2>& matrix);
    Vector<T2, dim> operator*(const Vector<T2, dim>& _vector);
    Vector<T2, dim> operator*=(const Vector<T2, dim>& _vector);
    Vector<T2, dim> operator-(const Vector<T2, dim>& _vector);
    Vector<T2, dim> operator+(const Vector<T2, dim>& _vector);
    void operator-=(const Vector<T2, dim>& _vector);
    void operator+=(const Vector<T2, dim>& _vector);
    Vector<T2, dim> operator*(const T2& _scalar);
	Vector<T2, dim> operator-();
	T2 Length() const;
	bool operator==(const Vector<T2, dim> vector) const;
};

template <class T2, int dim>
bool Vector<T2, dim>::operator==(const Vector<T2, dim> vector) const {
	if( dim == 3 ) {
		return this->m_vector[0] == vector.m_vector[0] && this->m_vector[1] == vector.m_vector[1] && this->m_vector[2] == vector.m_vector[2];
	}
}

template <class T2, int var2>
T2 Vector<T2, var2>::Length() const { 
	return std::sqrt(m_vector[0] * m_vector[0] + m_vector[1] * m_vector[1] + m_vector[2] * m_vector[2]);		
}

template <class T2, int var2>
std::ostream& operator<<(std::ostream& ostream, const Vector<T2, var2>& vector) {
	if( var2 == 3 ) {
		ostream << "x: " << vector[0] << " y: " << vector[1] << " z: " << vector[2] << " length: " << vector.Length();
	} else if( var2 == 4 ) {
		ostream << "x: " << vector[0] << " y: " << vector[1] << " z: " << vector[2] << " w: " << vector[3] << " length: " << vector.Length();
	} 
	return ostream;
}

template <class T2, int var2>
Vector<T2, var2> Vector<T2, var2>::operator-() {
	Vector<T2, var2> tempVector;
	for (int i = 0; i < var2; ++i)
		tempVector[i] = -m_vector[i];

	return tempVector;
}

template<class T2, int var2>
T2& Vector<T2, var2>::operator[](const int index)
{
	return m_vector[index];
}

template<class T2, int var2>
const T2& Vector<T2, var2>::operator[](const int index) const
{
	return m_vector[index];
}

template <class T2, int var2>
template <class T, int var>
Vector<T2, var2> Vector<T2, var2>::operator*(const Matrix<T, var>& matrix)
{
	static_assert(var == var2, "Size error");
	Vector<T2, var2> tempVector;
	for(int i = 0; i < var2; ++i)
		for(int j = 0; j < var; ++j)
		{
			tempVector[i] += m_vector[j] * matrix[j][i];
		}
	return tempVector;
}

template <typename T2, int var2>
Vector<T2, var2> Vector<T2, var2>::operator*(const Vector<T2, var2>& _vector)
{
    Vector<T2, var2> tempVector;
    for(int i = 0; i < 3; ++i)
        tempVector[i] = m_vector[i] * _vector[i];

    return tempVector;
}

template <typename T2, int var2>
Vector<T2, var2> Vector<T2, var2>::operator*=(const Vector<T2, var2>& _vector)
{
    Vector<T2, var2> tempVector;
    for(int i = 0; i < 3; ++i)
        tempVector[i] = m_vector[i] * _vector[i];

    return tempVector;
}

template <typename T2, int var2>
Vector<T2, var2> Vector<T2, var2>::operator-(const Vector<T2, var2>& _vector)
{
    Vector<T2, var2> temp_Vector(1);

    temp_Vector[0] = m_vector[0] - _vector[0];
    temp_Vector[1] = m_vector[1] - _vector[1];
    temp_Vector[2] = m_vector[2] - _vector[2];

    return temp_Vector;
}

template <typename T2, int var2>
Vector<T2, var2> Vector<T2, var2>::operator+(const Vector<T2, var2>& _vector)
{
	Vector<T2, var2> temp_Vector(1);

    temp_Vector[0] = m_vector[0] + _vector[0];
    temp_Vector[1] = m_vector[1] + _vector[1];
    temp_Vector[2] = m_vector[2] + _vector[2];

    return temp_Vector;
}

template <typename T2, int var2>
void Vector<T2, var2>::operator-=(const Vector<T2, var2>& _vector)
{
    m_vector[0] = m_vector[0] - _vector[0];
    m_vector[1] = m_vector[1] - _vector[1];
    m_vector[2] = m_vector[2] - _vector[2];
}

template <typename T2, int var2>
void Vector<T2, var2>::operator+=(const Vector<T2, var2>& _vector)
{
    m_vector[0] = m_vector[0] + _vector[0];
    m_vector[1] = m_vector[1] + _vector[1];
    m_vector[2] = m_vector[2] + _vector[2];
}

template <typename T2, int var2>
Vector<T2, var2> Vector<T2, var2>::operator*(const T2& _scalar)
{
    Vector<T2, var2> temp_vec(1.0f);
    
    for(int i = 0; i < var2; ++i)
        temp_vec[i] = m_vector[i] * _scalar;

    return temp_vec;
}

template <typename T2>
class Vector<T2, 1>
{
public:
	T2 x;

	Vector(T2 x = 0) 
	{
		this->x = x;
	}

	T2& operator[](const int index);
	const T2& operator[](const int index) const;
	template<class T, int dim2>
	Vector<T2, 1> operator*(const Matrix<T, dim2>& matrix);
    Vector<T2, 1> operator*(const Vector<T2, 1>& _vector);
    void operator*=(const Vector<T2, 1>& _vector);
    Vector<T2, 1> operator-(const Vector<T2, 1>& _vector);
    Vector<T2, 1> operator+(const Vector<T2, 1>& _vector);
    void operator-=(const Vector<T2, 1>& _vector);
    void operator+=(const Vector<T2, 1>& _vector);
    Vector<T2, 1> operator*(const T2& _scalar);
	Vector<T2, 1> operator-();
	T2 Length() const;
	bool operator==(const Vector<T2, 1> vector) const;
};

template <class T2>
bool Vector<T2, 1>::operator==(const Vector<T2, 1> vector) const {
	return this->x == vector.x;
}

template <class T2>
T2 Vector<T2, 1>::Length() const { 
	return std::sqrt(x * x);		
}

template <class T2>
std::ostream& operator<<(std::ostream& ostream, const Vector<T2, 1>& vector) {
	ostream << "x: " << vector.x << " length: " << vector.Length();
	
	return ostream;
}

template <class T2>
Vector<T2, 1> Vector<T2, 1>::operator-() {
	return { .x = -x };
}

template<class T2>
T2& Vector<T2, 1>::operator[](const int index) {
	return *(&x + index);
}

template<class T2>
const T2& Vector<T2, 1>::operator[](const int index) const {
	return *(&x + index);
}

template <class T2>
template <class T, int var>
Vector<T2, 1> Vector<T2, 1>::operator*(const Matrix<T, var>& matrix) {
	Vector<T2, 4> tempVector;
	for(int i = 0; i < 1; ++i) {
		for(int j = 0; j < var; ++j) {
			tempVector[i] += *((&this->x) + j) * matrix[j][i];
		}
	}

	return tempVector;
}

template <typename T2>
Vector<T2, 1> Vector<T2, 1>::operator*(const Vector<T2, 1>& _vector) {
    Vector<T2, 1> tempVector;
    for(int i = 0; i < 1; ++i) {
        tempVector[i] = (*this)[i] * _vector[i];
	}
	
    return tempVector;
}

template <typename T2>
void Vector<T2, 1>::operator*=(const Vector<T2, 1>& _vector)
{
    for(int i = 0; i < 1; ++i) {
        (*this)[i] = (*this)[i] * _vector[i];
	}
}

template <typename T2>
Vector<T2, 1> Vector<T2, 1>::operator-(const Vector<T2, 1>& _vector) {
    return { .x = this->x - _vector[0] };
}

template <typename T2>
Vector<T2, 1> Vector<T2, 1>::operator+(const Vector<T2, 1>& _vector) {
	return { .x = this->x + _vector[0] };
}

template <typename T2>
void Vector<T2, 1>::operator-=(const Vector<T2, 1>& _vector) {
	this->x = this->x - _vector[0];
}

template <typename T2>
void Vector<T2, 1>::operator+=(const Vector<T2, 1>& _vector)
{
	this->x = this->x + _vector[0];
}

template <typename T2>
Vector<T2, 1> Vector<T2, 1>::operator*(const T2& _scalar)
{
	return { .x = (*this)[0] * _scalar };
}

template <typename T2>
Vector<T2, 1> operator*(const Vector<T2, 1>& vector, const T2 _scalar)
{
	return { .x = vector[0] * _scalar };
}

template <typename T2>
class Vector<T2, 2>
{
public:
	T2 x;
	T2 y;

	Vector(T2 x = 0, T2 y = 0) {
		this->x = x;
		this->y = y;
	}

	T2& operator[](const int index);
	const T2& operator[](const int index) const;
	template<class T, int dim2>
	Vector<T2, 2> operator*(const Matrix<T, dim2>& matrix);
    Vector<T2, 2> operator*(const Vector<T2, 2>& _vector);
    void operator*=(const Vector<T2, 2>& _vector);
    Vector<T2, 2> operator-(const Vector<T2, 2>& _vector);
    Vector<T2, 2> operator+(const Vector<T2, 2>& _vector);
    void operator-=(const Vector<T2, 2>& _vector);
    void operator+=(const Vector<T2, 2>& _vector);
    Vector<T2, 2> operator*(const T2& _scalar);
	Vector<T2, 2> operator-();
	T2 Length() const;
	bool operator==(const Vector<T2, 2> vector) const;
};

template <class T2>
bool Vector<T2, 2>::operator==(const Vector<T2, 2> vector) const {
	return this->x == vector.x && this->y == vector.y;
}

template <class T2>
T2 Vector<T2, 2>::Length() const { 
	return std::sqrt(x * x + y * y);		
}

template <class T2>
std::ostream& operator<<(std::ostream& ostream, const Vector<T2, 2>& vector) {
	ostream << "x: " << vector.x << " y: " << vector.y << " z: " << vector.z << " w: " << vector.w << " length: " << vector.Length();
	
	return ostream;
}

template <class T2>
Vector<T2, 2> Vector<T2, 2>::operator-() {
	return { .x = -x, .y = -y };
}

template<class T2>
T2& Vector<T2, 2>::operator[](const int index) {
	return *(&x + index);
}

template<class T2>
const T2& Vector<T2, 2>::operator[](const int index) const {
	return *(&x + index);
}

template <class T2>
template <class T, int var>
Vector<T2, 2> Vector<T2, 2>::operator*(const Matrix<T, var>& matrix) {
	static_assert(var == 2, "Size error");
	Vector<T2, 2> tempVector;
	for(int i = 0; i < 2; ++i) {
		for(int j = 0; j < var; ++j) {
			tempVector[i] += *((&this->x) + j) * matrix[j][i];
		}
	}

	return tempVector;
}

template <typename T2>
Vector<T2, 2> Vector<T2, 2>::operator*(const Vector<T2, 2>& _vector) {
    Vector<T2, 2> tempVector;
    for(int i = 0; i < 2; ++i) {
        tempVector[i] = (*this)[i] * _vector[i];
	}
	
    return tempVector;
}

template <typename T2>
void Vector<T2, 2>::operator*=(const Vector<T2, 2>& _vector) {
    for(int i = 0; i < 2; ++i) {
        (*this)[i] = (*this)[i] * _vector[i];
	}
}

template <typename T2>
Vector<T2, 2> Vector<T2, 2>::operator-(const Vector<T2, 2>& _vector) {
    return { this->x - _vector[0], this->y - _vector[1] };
}

template <typename T2>
Vector<T2, 2> Vector<T2, 2>::operator+(const Vector<T2, 2>& _vector) {
	return { this->x + _vector[0], this->y + _vector[1] };
}

template <typename T2>
void Vector<T2, 2>::operator-=(const Vector<T2, 2>& _vector) {
	this->x = this->x - _vector[0];
	this->y = this->y - _vector[1];
}

template <typename T2>
void Vector<T2, 2>::operator+=(const Vector<T2, 2>& _vector) {
	this->x = this->x + _vector[0];
	this->y = this->y + _vector[1];
}

template <typename T2>
Vector<T2, 2> Vector<T2, 2>::operator*(const T2& _scalar) {
	return { .x = (*this)[0] * _scalar, .y = (*this)[1] * _scalar };
}

template <typename T2>
Vector<T2, 2> operator*(const Vector<T2, 2>& vector, const T2 _scalar) {
	return { .x = vector[0] * _scalar, .y = vector[1] * _scalar };
}

template <typename T2>
class Vector<T2, 3>
{
public:
	T2 x;
	T2 y;
	T2 z;

	Vector(T2 x = 0, T2 y = 0, T2 z = 0) {
		this->x = x;
		this->y = y;
		this->z = z;
	}

	T2& operator[](const int index);
	const T2& operator[](const int index) const;
	template<class T, int dim2>
	Vector<T2, 3> operator*(const Matrix<T, dim2>& matrix);
    Vector<T2, 3> operator*(const Vector<T2, 3>& _vector);
    void operator*=(const Vector<T2, 3>& _vector);
    Vector<T2, 3> operator-(const Vector<T2, 3>& _vector);
    Vector<T2, 3> operator+(const Vector<T2, 3>& _vector);
    void operator-=(const Vector<T2, 3>& _vector);
    void operator+=(const Vector<T2, 3>& _vector);
    Vector<T2, 3> operator*(const T2& _scalar);
	Vector<T2, 3> operator-();
	T2 Length() const;
	bool operator==(const Vector<T2, 3> vector) const;
};

template <class T2>
bool Vector<T2, 3>::operator==(const Vector<T2, 3> vector) const {
	return this->x == vector.x && this->y == vector.y && this->z == vector.z;
}

template <class T2>
T2 Vector<T2, 3>::Length() const { 
	return std::sqrt(x * x + y * y + z * z);		
}

template <class T2>
std::ostream& operator<<(std::ostream& ostream, const Vector<T2, 3>& vector) {
	ostream << "x: " << vector.x << " y: " << vector.y << " z: " << vector.z << " length: " << vector.Length();
	
	return ostream;
}

template <class T2>
Vector<T2, 3> Vector<T2, 3>::operator-() {
	return { -x, -y, -z };
}

template<class T2>
T2& Vector<T2, 3>::operator[](const int index)
{
	return *(&x + index);
}

template<class T2>
const T2& Vector<T2, 3>::operator[](const int index) const
{
	return *(&x + index);
}

template <class T2>
template <class T, int var>
Vector<T2, 3> Vector<T2, 3>::operator*(const Matrix<T, var>& matrix)
{
	static_assert(var == 3, "Size error");
	Vector<T2, 3> tempVector;
	for(int i = 0; i < 3; ++i) {
		for(int j = 0; j < var; ++j) {
			tempVector[i] += *((&this->x) + j) * matrix[j][i];
		}
	}

	return tempVector;
}

template <typename T2>
Vector<T2, 3> Vector<T2, 3>::operator*(const Vector<T2, 3>& _vector)
{
    Vector<T2, 3> tempVector;
    for(int i = 0; i < 3; ++i) {
        tempVector[i] = (*this)[i] * _vector[i];
	}
	
    return tempVector;
}

template <typename T2>
void Vector<T2, 3>::operator*=(const Vector<T2, 3>& _vector)
{
    for(int i = 0; i < 3; ++i) {
        (*this)[i] = (*this)[i] * _vector[i];
	}
}

template <typename T2>
Vector<T2, 3> Vector<T2, 3>::operator-(const Vector<T2, 3>& _vector) {
    return { this->x - _vector[0], this->y - _vector[1], this->z - _vector[2] };
}

template <typename T2>
Vector<T2, 3> Vector<T2, 3>::operator+(const Vector<T2, 3>& _vector) {
	return { this->x + _vector[0], this->y + _vector[1], this->z + _vector[2] };
}

template <typename T2>
void Vector<T2, 3>::operator-=(const Vector<T2, 3>& _vector) {
	this->x = this->x - _vector[0];
	this->y = this->y - _vector[1];
	this->z = this->z - _vector[2];
}

template <typename T2>
void Vector<T2, 3>::operator+=(const Vector<T2, 3>& _vector) {
	this->x = this->x + _vector[0];
	this->y = this->y + _vector[1];
	this->z = this->z + _vector[2];
}

template <typename T2>
Vector<T2, 3> Vector<T2, 3>::operator*(const T2& _scalar)
{
	return { (*this)[0] * _scalar, (*this)[1] * _scalar, (*this)[2] * _scalar };
}

template <typename T2>
Vector<T2, 3> operator*(const Vector<T2, 3>& vector, const T2 _scalar)
{
	return { vector[0] * _scalar, vector[1] * _scalar, vector[2] * _scalar };
}

template <typename T2, int var2>
Vector<T2, var2> operator*(const Vector<T2, var2>& vector, const T2 _scalar)
{
	Vector<T2, var2> temp;
    for(int i = 0; i < var2; ++i)
        temp[i] = vector[i] * _scalar;

    return temp;
}

template <typename T2>
class Vector<T2, 4>
{
public:
	T2 x;
	T2 y;
	T2 z;
	T2 w;

	Vector(T2 x = 0, T2 y = 0, T2 z = 0, T2 w = 0) 
	{
		this->x = x;
		this->y = y;
		this->z = z;
		this->w = w;
	}

	T2 get( const uint32_t index ) {
		switch( index ) {
		case 0:
			return x;
			break;
		case 1:
			return y;
			break;
		case 2:
			return z;
			break;
		case 3:
			return w;
			break;
		default:
			break;
		}

		assert("Index out of bound");
		return -1;
	}
	
	T2& operator[](const int index);
	const T2& operator[](const int index) const;
	template<class T, int dim2>
	Vector<T2, 4> operator*(const Matrix<T, dim2>& matrix);
    Vector<T2, 4> operator*(const Vector<T2, 4>& _vector);
    void operator*=(const Vector<T2, 4>& _vector);
    Vector<T2, 4> operator-(const Vector<T2, 4>& _vector);
    Vector<T2, 4> operator+(const Vector<T2, 4>& _vector);
    void operator-=(const Vector<T2, 4>& _vector);
    void operator+=(const Vector<T2, 4>& _vector);
    Vector<T2, 4> operator*(const T2& _scalar);
	Vector<T2, 4> operator-();
	T2 Length() const;
	bool operator==(const Vector<T2, 4> vector) const;
};

template <class T2>
bool Vector<T2, 4>::operator==(const Vector<T2, 4> vector) const {
	return this->x == vector.x && this->y == vector.y && this->z == vector.z && this->w == vector.w;
}

template <class T2>
T2 Vector<T2, 4>::Length() const { 
	return std::sqrt(x * x + y * y + z * z);		
}

template <class T2>
std::ostream& operator<<(std::ostream& ostream, const Vector<T2, 4>& vector) {
	ostream << "x: " << vector.x << " y: " << vector.y << " z: " << vector.z << " w: " << vector.w << " length: " << vector.Length();
	
	return ostream;
}

template <class T2>
Vector<T2, 4> Vector<T2, 4>::operator-() {
	return { .x = -x, .y = -y, .z = -z, .w = -w };
}

template<class T2>
T2& Vector<T2, 4>::operator[](const int index)
{
	return *(&x + index);
}

template<class T2>
const T2& Vector<T2, 4>::operator[](const int index) const
{
	return *(&x + index);
}

template <class T2>
template <class T, int var>
Vector<T2, 4> Vector<T2, 4>::operator*(const Matrix<T, var>& matrix)
{
	static_assert(var == 4, "Size error");
	Vector<T2, 4> tempVector;
	for(int i = 0; i < 4; ++i) {
		for(int j = 0; j < var; ++j) {
			tempVector[i] += *((&this->x) + j) * matrix[j][i];
		}
	}

	return tempVector;
}

template <typename T2>
Vector<T2, 4> Vector<T2, 4>::operator*(const Vector<T2, 4>& _vector)
{
    Vector<T2, 4> tempVector;
    for(int i = 0; i < 4; ++i) {
        tempVector[i] = (*this)[i] * _vector[i];
	}
	
    return tempVector;
}

template <typename T2>
void Vector<T2, 4>::operator*=(const Vector<T2, 4>& _vector)
{
    for(int i = 0; i < 4; ++i) {
        (*this)[i] = (*this)[i] * _vector[i];
	}
}

template <typename T2>
Vector<T2, 4> Vector<T2, 4>::operator-(const Vector<T2, 4>& _vector)
{
    return { .x = get(0) - _vector[0], .y = get(1) - _vector[1], .z = get(2) - _vector[2], .w = get(3) - _vector[3] };
}

template <typename T2>
Vector<T2, 4> Vector<T2, 4>::operator+(const Vector<T2, 4>& _vector)
{
	return { .x = get(0) + _vector[0], .y = get(1) + _vector[1], .z = get(2) + _vector[2], .w = get(3) + _vector[3] };
}

template <typename T2>
void Vector<T2, 4>::operator-=(const Vector<T2, 4>& _vector)
{
	this->x = this->x - _vector[0];
	this->y = this->y - _vector[1];
	this->z = this->z - _vector[2];
	this->w = this->w - _vector[3];
}

template <typename T2>
void Vector<T2, 4>::operator+=(const Vector<T2, 4>& _vector)
{
	this->x = this->x + _vector[0];
	this->y = this->y + _vector[1];
	this->z = this->z + _vector[2];
	this->w = this->w + _vector[3];
}

template <typename T2>
Vector<T2, 4> Vector<T2, 4>::operator*(const T2& _scalar)
{
	return { .x = (*this)[0] * _scalar, .y = (*this)[1] * _scalar, .z = (*this)[2] * _scalar, .w = (*this)[3] * _scalar };
}

template <typename T2>
Vector<T2, 4> operator*(const Vector<T2, 4>& vector, const T2 _scalar)
{
	return { .x = vector[0] * _scalar, .y = vector[1] * _scalar, .z = vector[2] * _scalar, .w = vector[3] * _scalar };
}

// template <typename T, int size, int row, int col>
// Matrix<T, size - 1> matrix_minor( Matrix<T, size> matrix ) {
// 	Matrix<T, size - 1> minor(0.0f);
// 	unsigned int minor_row = 0;
// 	unsigned int minor_col = 0;
	
// 	for ( unsigned int i = 0; i < size; ++i ) {
// 		if ( i == row )
// 			continue;
// 		for ( unsigned int j = 0; j < size; ++j ) {
// 			if ( j == col )
// 				continue;

// 			minor[minor_row][minor_col] = matrix[i][j];
// 			++minor_col;
// 		}
// 		++minor_row;
// 	}

// 	return minor;
// }

template <typename T>
T determinant_2x2( Matrix<T, 2> matrix ) {
	return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
}

template <typename T>
T determinant_3x3( Matrix<T, 3> matrix ) {
	Matrix<T, 2> remove_1row_1col(0.0f);
	remove_1row_1col[0][0] = matrix[1][1];
	remove_1row_1col[0][1] = matrix[1][2];
	remove_1row_1col[1][0] = matrix[2][1];
	remove_1row_1col[1][1] = matrix[2][2];

	Matrix<T, 2> remove_1row_2col(0.0f);
	remove_1row_2col[0][0] = matrix[1][0];
	remove_1row_2col[0][1] = matrix[1][2];
	remove_1row_2col[1][0] = matrix[2][0];
	remove_1row_2col[1][1] = matrix[2][2];

	Matrix<T, 2> remove_1row_3col(0.0f);
	remove_1row_3col[0][0] = matrix[1][0];
	remove_1row_3col[0][1] = matrix[1][1];
	remove_1row_3col[1][0] = matrix[2][0];
	remove_1row_3col[1][1] = matrix[2][1];

	T determinant = matrix[0][0] * determinant_2x2<T>(remove_1row_1col) -
		matrix[0][1] * determinant_2x2<T>(remove_1row_2col) +
		matrix[0][2] * determinant_2x2<T>(remove_1row_3col);
	
	return determinant;
}

template <typename T>
T determinant_4x4( Matrix<T, 4> matrix ) {
	Matrix<T, 3> remove_1row_1col(0.0f);
	remove_1row_1col[0][0] = matrix[1][1];
	remove_1row_1col[0][1] = matrix[1][2];
	remove_1row_1col[0][2] = matrix[1][3];
	remove_1row_1col[1][0] = matrix[2][1];
	remove_1row_1col[1][1] = matrix[2][2];
	remove_1row_1col[1][2] = matrix[2][3];
	remove_1row_1col[2][0] = matrix[3][1];
	remove_1row_1col[2][1] = matrix[3][2];
	remove_1row_1col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_1row_2col(0.0f);
	remove_1row_2col[0][0] = matrix[1][0];
	remove_1row_2col[0][1] = matrix[1][2];
	remove_1row_2col[0][2] = matrix[1][3];
	remove_1row_2col[1][0] = matrix[2][0];
	remove_1row_2col[1][1] = matrix[2][2];
	remove_1row_2col[1][2] = matrix[2][3];
	remove_1row_2col[2][0] = matrix[3][0];
	remove_1row_2col[2][1] = matrix[3][2];
	remove_1row_2col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_1row_3col(0.0f);
	remove_1row_3col[0][0] = matrix[1][0];
	remove_1row_3col[0][1] = matrix[1][1];
	remove_1row_3col[0][2] = matrix[1][3];
	remove_1row_3col[1][0] = matrix[2][0];
	remove_1row_3col[1][1] = matrix[2][1];
	remove_1row_3col[1][2] = matrix[2][3];
	remove_1row_3col[2][0] = matrix[3][0];
	remove_1row_3col[2][1] = matrix[3][1];
	remove_1row_3col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_1row_4col(0.0f);
	remove_1row_4col[0][0] = matrix[1][0];
	remove_1row_4col[0][1] = matrix[1][1];
	remove_1row_4col[0][2] = matrix[1][2];
	remove_1row_4col[1][0] = matrix[2][0];
	remove_1row_4col[1][1] = matrix[2][1];
	remove_1row_4col[1][2] = matrix[2][2];
	remove_1row_4col[2][0] = matrix[3][0];
	remove_1row_4col[2][1] = matrix[3][1];
	remove_1row_4col[2][2] = matrix[3][2];

	T determinant = matrix[0][0] * determinant_3x3<float>(remove_1row_1col) -
		matrix[0][1] * determinant_3x3<float>(remove_1row_2col) +
		matrix[0][2] * determinant_3x3<float>(remove_1row_3col) -
		matrix[0][3] * determinant_3x3<float>(remove_1row_4col);

	return determinant;
}

template <typename T>
Matrix<T, 4> inverse_matrix_4x4( Matrix<T, 4> matrix ) {
	Matrix<T, 3> remove_1row_1col(0.0f);
	remove_1row_1col[0][0] = matrix[1][1];
	remove_1row_1col[0][1] = matrix[1][2];
	remove_1row_1col[0][2] = matrix[1][3];
	remove_1row_1col[1][0] = matrix[2][1];
	remove_1row_1col[1][1] = matrix[2][2];
	remove_1row_1col[1][2] = matrix[2][3];
	remove_1row_1col[2][0] = matrix[3][1];
	remove_1row_1col[2][1] = matrix[3][2];
	remove_1row_1col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_1row_2col(0.0f);
	remove_1row_2col[0][0] = matrix[1][0];
	remove_1row_2col[0][1] = matrix[1][2];
	remove_1row_2col[0][2] = matrix[1][3];
	remove_1row_2col[1][0] = matrix[2][0];
	remove_1row_2col[1][1] = matrix[2][2];
	remove_1row_2col[1][2] = matrix[2][3];
	remove_1row_2col[2][0] = matrix[3][0];
	remove_1row_2col[2][1] = matrix[3][2];
	remove_1row_2col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_1row_3col(0.0f);
	remove_1row_3col[0][0] = matrix[1][0];
	remove_1row_3col[0][1] = matrix[1][1];
	remove_1row_3col[0][2] = matrix[1][3];
	remove_1row_3col[1][0] = matrix[2][0];
	remove_1row_3col[1][1] = matrix[2][1];
	remove_1row_3col[1][2] = matrix[2][3];
	remove_1row_3col[2][0] = matrix[3][0];
	remove_1row_3col[2][1] = matrix[3][1];
	remove_1row_3col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_1row_4col(0.0f);
	remove_1row_4col[0][0] = matrix[1][0];
	remove_1row_4col[0][1] = matrix[1][1];
	remove_1row_4col[0][2] = matrix[1][2];
	remove_1row_4col[1][0] = matrix[2][0];
	remove_1row_4col[1][1] = matrix[2][1];
	remove_1row_4col[1][2] = matrix[2][2];
	remove_1row_4col[2][0] = matrix[3][0];
	remove_1row_4col[2][1] = matrix[3][1];
	remove_1row_4col[2][2] = matrix[3][2];

	Matrix<T, 3> remove_2row_1col(0.0f);
	remove_2row_1col[0][0] = matrix[0][1];
	remove_2row_1col[0][1] = matrix[0][2];
	remove_2row_1col[0][2] = matrix[0][3];
	remove_2row_1col[1][0] = matrix[2][1];
	remove_2row_1col[1][1] = matrix[2][2];
	remove_2row_1col[1][2] = matrix[2][3];
	remove_2row_1col[2][0] = matrix[3][1];
	remove_2row_1col[2][1] = matrix[3][2];
	remove_2row_1col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_2row_2col(0.0f);
	remove_2row_2col[0][0] = matrix[0][0];
	remove_2row_2col[0][1] = matrix[0][2];
	remove_2row_2col[0][2] = matrix[0][3];
	remove_2row_2col[1][0] = matrix[2][0];
	remove_2row_2col[1][1] = matrix[2][2];
	remove_2row_2col[1][2] = matrix[2][3];
	remove_2row_2col[2][0] = matrix[3][0];
	remove_2row_2col[2][1] = matrix[3][2];
	remove_2row_2col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_2row_3col(0.0f);
	remove_2row_3col[0][0] = matrix[0][0];
	remove_2row_3col[0][1] = matrix[0][1];
	remove_2row_3col[0][2] = matrix[0][3];
	remove_2row_3col[1][0] = matrix[2][0];
	remove_2row_3col[1][1] = matrix[2][1];
	remove_2row_3col[1][2] = matrix[2][3];
	remove_2row_3col[2][0] = matrix[3][0];
	remove_2row_3col[2][1] = matrix[3][1];
	remove_2row_3col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_2row_4col(0.0f);
	remove_2row_4col[0][0] = matrix[0][0];
	remove_2row_4col[0][1] = matrix[0][1];
	remove_2row_4col[0][2] = matrix[0][2];
	remove_2row_4col[1][0] = matrix[2][0];
	remove_2row_4col[1][1] = matrix[2][1];
	remove_2row_4col[1][2] = matrix[2][2];
	remove_2row_4col[2][0] = matrix[3][0];
	remove_2row_4col[2][1] = matrix[3][1];
	remove_2row_4col[2][2] = matrix[3][2];

	Matrix<T, 3> remove_3row_1col(0.0f);
	remove_3row_1col[0][0] = matrix[0][1];
	remove_3row_1col[0][1] = matrix[0][2];
	remove_3row_1col[0][2] = matrix[0][3];
	remove_3row_1col[1][0] = matrix[1][1];
	remove_3row_1col[1][1] = matrix[1][2];
	remove_3row_1col[1][2] = matrix[1][3];
	remove_3row_1col[2][0] = matrix[3][1];
	remove_3row_1col[2][1] = matrix[3][2];
	remove_3row_1col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_3row_2col(0.0f);
	remove_3row_2col[0][0] = matrix[0][0];
	remove_3row_2col[0][1] = matrix[0][2];
	remove_3row_2col[0][2] = matrix[0][3];
	remove_3row_2col[1][0] = matrix[1][0];
	remove_3row_2col[1][1] = matrix[1][2];
	remove_3row_2col[1][2] = matrix[1][3];
	remove_3row_2col[2][0] = matrix[3][0];
	remove_3row_2col[2][1] = matrix[3][2];
	remove_3row_2col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_3row_3col(0.0f);
	remove_3row_3col[0][0] = matrix[0][0];
	remove_3row_3col[0][1] = matrix[0][1];
	remove_3row_3col[0][2] = matrix[0][3];
	remove_3row_3col[1][0] = matrix[1][0];
	remove_3row_3col[1][1] = matrix[1][1];
	remove_3row_3col[1][2] = matrix[1][3];
	remove_3row_3col[2][0] = matrix[3][0];
	remove_3row_3col[2][1] = matrix[3][1];
	remove_3row_3col[2][2] = matrix[3][3];

	Matrix<T, 3> remove_3row_4col(0.0f);
	remove_3row_4col[0][0] = matrix[0][0];
	remove_3row_4col[0][1] = matrix[0][1];
	remove_3row_4col[0][2] = matrix[0][2];
	remove_3row_4col[1][0] = matrix[1][0];
	remove_3row_4col[1][1] = matrix[1][1];
	remove_3row_4col[1][2] = matrix[1][2];
	remove_3row_4col[2][0] = matrix[3][0];
	remove_3row_4col[2][1] = matrix[3][1];
	remove_3row_4col[2][2] = matrix[3][2];

	Matrix<T, 3> remove_4row_1col(0.0f);
	remove_4row_1col[0][0] = matrix[0][1];
	remove_4row_1col[0][1] = matrix[0][2];
	remove_4row_1col[0][2] = matrix[0][3];
	remove_4row_1col[1][0] = matrix[1][1];
	remove_4row_1col[1][1] = matrix[1][2];
	remove_4row_1col[1][2] = matrix[1][3];
	remove_4row_1col[2][0] = matrix[2][1];
	remove_4row_1col[2][1] = matrix[2][2];
	remove_4row_1col[2][2] = matrix[2][3];

	Matrix<T, 3> remove_4row_2col(0.0f);
	remove_4row_2col[0][0] = matrix[0][0];
	remove_4row_2col[0][1] = matrix[0][2];
	remove_4row_2col[0][2] = matrix[0][3];
	remove_4row_2col[1][0] = matrix[1][0];
	remove_4row_2col[1][1] = matrix[1][2];
	remove_4row_2col[1][2] = matrix[1][3];
	remove_4row_2col[2][0] = matrix[2][0];
	remove_4row_2col[2][1] = matrix[2][2];
	remove_4row_2col[2][2] = matrix[2][3];

	Matrix<T, 3> remove_4row_3col(0.0f);
	remove_4row_3col[0][0] = matrix[0][0];
	remove_4row_3col[0][1] = matrix[0][1];
	remove_4row_3col[0][2] = matrix[0][3];
	remove_4row_3col[1][0] = matrix[1][0];
	remove_4row_3col[1][1] = matrix[1][1];
	remove_4row_3col[1][2] = matrix[1][3];
	remove_4row_3col[2][0] = matrix[2][0];
	remove_4row_3col[2][1] = matrix[2][1];
	remove_4row_3col[2][2] = matrix[2][3];

	Matrix<T, 3> remove_4row_4col(0.0f);
	remove_4row_4col[0][0] = matrix[0][0];
	remove_4row_4col[0][1] = matrix[0][1];
	remove_4row_4col[0][2] = matrix[0][2];
	remove_4row_4col[1][0] = matrix[1][0];
	remove_4row_4col[1][1] = matrix[1][1];
	remove_4row_4col[1][2] = matrix[1][2];
	remove_4row_4col[2][0] = matrix[2][0];
	remove_4row_4col[2][1] = matrix[2][1];
	remove_4row_4col[2][2] = matrix[2][2];

	Matrix<T, 4> matrix_of_minors(0.0f);
	matrix_of_minors[0][0] = determinant_3x3<T>(remove_1row_1col);
	matrix_of_minors[0][1] = determinant_3x3<T>(remove_1row_2col);
	matrix_of_minors[0][2] = determinant_3x3<T>(remove_1row_3col);
	matrix_of_minors[0][3] = determinant_3x3<T>(remove_1row_4col);
	matrix_of_minors[1][0] = determinant_3x3<T>(remove_2row_1col);
	matrix_of_minors[1][1] = determinant_3x3<T>(remove_2row_2col);
	matrix_of_minors[1][2] = determinant_3x3<T>(remove_2row_3col);
	matrix_of_minors[1][3] = determinant_3x3<T>(remove_2row_4col);
	matrix_of_minors[2][0] = determinant_3x3<T>(remove_3row_1col);
	matrix_of_minors[2][1] = determinant_3x3<T>(remove_3row_2col);
	matrix_of_minors[2][2] = determinant_3x3<T>(remove_3row_3col);
	matrix_of_minors[2][3] = determinant_3x3<T>(remove_3row_4col);
	matrix_of_minors[3][0] = determinant_3x3<T>(remove_4row_1col);
	matrix_of_minors[3][1] = determinant_3x3<T>(remove_4row_2col);
	matrix_of_minors[3][2] = determinant_3x3<T>(remove_4row_3col);
//	std::cout << "3 2: " << determinant_3x3<T>(remove_4row_3col) << std::endl;
	matrix_of_minors[3][3] = determinant_3x3<T>(remove_4row_4col);

	// Compute matrix of cofactors
	Matrix<T, 4> matrix_of_cofactors(0.0f);
	for ( unsigned int i = 0; i < 4; ++i )
		for ( unsigned int j = 0; j < 4; ++j ) {
			if ( ( i + j ) % 2 == 0 ) {
				matrix_of_cofactors[i][j] = matrix_of_minors[i][j];
			} else {
				matrix_of_cofactors[i][j] = -matrix_of_minors[i][j];
			}
		}

	// Compute adjoint matrix
	Matrix<T, 4> adjoint_matrix(0.0f);
	adjoint_matrix = matrix_of_cofactors;
	adjoint_matrix.SelfTensorTranspose();

	T determinant_of_basic_matrix = determinant_4x4<T>(matrix);
	Matrix<T, 4> inverse_matrix(0.0f);
	inverse_matrix = adjoint_matrix * (1 / determinant_of_basic_matrix);
	
	return inverse_matrix;
}

template <class T, class T2,int var, int var2>
Matrix<T, var> LookAt(Matrix<T, var> matrix, Vector<T2, var2> vector)
{
	Matrix<T, var> tempMatrix(1.0f);
	tempMatrix = matrix;

	unsigned const int variable = 3;
	for(int i = 0; i < var2; ++i)
		tempMatrix[i][variable] = -vector[i];

	return tempMatrix;
}

template <class T, class T2, int var, int var2>
Matrix<T, var> Translate(Matrix<T, var> matrix, Vector<T2, var2> vector)
{
	Matrix<T, var> tempMatrix(1.0f);
	tempMatrix = matrix;

	// for(int i = 0; i < var; ++i)
	// 	tempMatrix[i][var-1] += vector[i];

	for(int i = 0; i < var; ++i)
		tempMatrix[var-1][i] += vector[i];

	return tempMatrix;
}

template <class T, class T2, int var, int var2>
Matrix<T, var> Scale(Matrix<T, var> matrix, Vector<T2, var2> vector)
{
	Matrix<T, var> tempMatrix;
	tempMatrix = matrix;

	for(int i = 0; i < var; ++i)
		for(int j = 0; j < var; ++j)
		{
			tempMatrix[j][i] *= vector[i];
		}
	return tempMatrix;
}

template <class T, int var>
Matrix<T, var> RotateZ(Matrix<T, var> matrix, float angle)
{
	Matrix<T, var> tempMatrix(1.0f);
	tempMatrix[0][0] = std::cos(angle*PI/180);
	tempMatrix[0][1] = -std::sin(angle*PI/180);
	tempMatrix[1][0] = std::sin(angle*PI/180);
	tempMatrix[1][1] = std::cos(angle*PI/180);

	Matrix<T, var> tempMatrix2;
	tempMatrix2 = matrix * tempMatrix;

	return tempMatrix2;
}

// template <typename T, int size>
// T Deteminant(Matrix<T, size> _matrix)
// {
//     if(size <= 1)
//         return;
//     int iDeterminant = 0;
//     int iSign = 1;

//     Matrix<T, (size - 1)> _temp_Matrix;
    
//     for(int i = 0; i < size; ++i)
//     {
//         if(i % 2 == 1)
//             iSign = -1;
//         else
//             iSign = 1;

//         if(size == 2)
//             return (_matrix[0][i] *iSign) * (_matrix[0][0] * _matrix[1][1] - _matrix[0][1] * _matrix[1][0]);

//         for(int j = 0; j < (size - 1); ++j)
//         {
//             if(j == i)
//                 continue;
//             for(int x = 0; x < (size - 1); ++x)
//             {
//                 if(x == i)
//                     continue;

//                 _temp_Matrix = _matrix[j][x];
//             }
//         }

//         iDeterminant = (_matrix[0][i] * iSign) * iDeterminant(_temp_Matrix);
//     }

//     return iDeterminant;
// }

// template <typename T>
// Vector<T, 3> Cross(Vector<T, 3> _vector1, Vector<T, 3> _vector2)
// {
//     return Vector<T, 3>(_vector1[1] * _vector2[2] - _vector1[2] * _vector2[1],
//                            _vector1[2] * _vector2[0] - _vector1[0] * _vector2[2],
//                            _vector1[0] * _vector2[1] - _vector1[1] * _vector2[0]);
// }

template <typename T>
Vector<T, 3> Cross(const Vector<T, 3>& _vector1, const Vector<T, 3>& _vector2)
{
    return Vector<T, 3>(_vector1[1] * _vector2[2] - _vector1[2] * _vector2[1],
                           _vector1[2] * _vector2[0] - _vector1[0] * _vector2[2],
                           _vector1[0] * _vector2[1] - _vector1[1] * _vector2[0]);
}

template<typename T>
T cross( const Vector<T, 2>& vector0, const Vector<T, 2> vector1 ) {
	return vector0[0] * vector1[1] - vector0[1] * vector1[0];
}

template <typename T>
T Dot(const Vector<T, 3>& _vector1, const Vector<T, 3>& _vector2)
{
    return (_vector1[0] * _vector2[0] + _vector1[1] * _vector2[1] + _vector1[2] * _vector2[2]);
}

template <typename T>
T VectorLength(const Vector<T, 3>& _vector1, const Vector<T, 3>& _vector2)
{
    T _x_axis = _vector2[0] - _vector1[0];
    T _y_axis = _vector2[1] - _vector1[1];
    T _z_axis = _vector2[2] - _vector1[2];
    return std::sqrt(_x_axis * _x_axis + _y_axis * _y_axis + _z_axis * _z_axis);
}

template <typename T>
T VecLength(const Vector<T, 3>& vector)
{
    return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
}

template <typename T>
Vector<T, 3> Normalize(Vector<T, 3> _vector)
{
	if (_vector[0] == 0 && _vector[1] == 0 && _vector[2] == 0)
		return vec3 { 0.0f, 0.0f, 0.0f }; 
	
    float range = std::sqrt(_vector[0]*_vector[0] + _vector[1]*_vector[1] + _vector[2]*_vector[2]);
	for(int l = 0; l < 3; ++l)
		_vector[l] = _vector[l]/range;

    return _vector;
}

struct AABB {
	vec3 origin;
	vec3 extents;
};

struct plane {
	vec3 normal;
	float distance;
};

struct Frustum {
	plane planes[6];
			
	struct PlaneIndex {
		enum EPlaneIndex {
			LEFT_PLANE,
			RIGHT_PLANE,
			BOTTOM_PLANE,
			TOP_PLANE,
			NEAR_PLANE,
			FAR_PLANE
		};
	} planeIndex;
};

// template <typename T>
// Matrix<T, 4> LookAtRH(Vector<T, 3> _eye, Vector<T, 3> _target, Vector<T, 3> _up)
// {
//     Vector<T, 3> z_axis = Normalize(_eye - _target);
//     Vector<T, 3> x_axis = Normalize(Cross(_up, z_axis));
//     Vector<T, 3> y_axis = Cross(z_axis, x_axis);
    
//     Matrix<T, 4> tOrientation(Vector<T, 4>(x_axis[0], y_axis[0], z_axis[0], 0),
//                               Vector<T, 4>(x_axis[1], y_axis[1], z_axis[1], 0),
//                               Vector<T, 4>(x_axis[2], y_axis[2], z_axis[2], 0),
//                               Vector<T, 4>(0, 0, 0, 1));

//     Matrix<T, 4> tTranslation(Vector<T, 4>(1, 0, 0 ,0),
//                               Vector<T, 4>(0, 1, 0, 0),
//                               Vector<T, 4>(0, 0, 1, 0),
//                               Vector<T, 4>(-_eye[0], -_eye[1], -_eye[2], 1));
    
//     return (tOrientation * tTranslation);

//     // Matrix<T, 4> tView(Vector<T, 4>(x_axis[0], y_axis[0], z_axis[0], 0),
//     //                    Vector<T, 4>(x_axis[1], y_axis[1], z_axis[1], 0),
//     //                    Vector<T, 4>(x_axis[2], y_axis[2], z_axis[2], 0),
//     //                    Vector<T, 4>(-Dot(x_axis, _eye), -Dot(y_axis, _eye), -Dot(z_axis, _eye), 1));

//     // return tView;
// }

template <typename T>
Matrix<T, 4> GLVM_perspectiveRH_ZO(T fov, T aspect, T near_plane, T far_plane) {
	T const tanHalfFov = std::tan(fov / static_cast<T>(2));

	mat4 Result(static_cast<T>(0));

	Result[0][0] = static_cast<T>(1) / (aspect * tanHalfFov);
	Result[1][1] = static_cast<T>(1) / (tanHalfFov);
	Result[2][2] = far_plane / (near_plane - far_plane);
	Result[2][3] = - static_cast<T>(1);
	Result[3][2] = -(far_plane * near_plane) / (far_plane - near_plane);
	return Result;
}

template <typename T>
Matrix<T, 4> GLVM_perspectiveRH_NO(T fov, T aspect, T near_plane, T far_plane) {
	T const tanHalfFov = std::tan(fov / static_cast<T>(2));

	mat4 Result(static_cast<T>(0));

	Result[0][0] = static_cast<T>(1) / (aspect * tanHalfFov);
	Result[1][1] = static_cast<T>(1) / (tanHalfFov);
	Result[2][2] = - (far_plane + near_plane) / (far_plane - near_plane);
	Result[2][3] = - static_cast<T>(1);
	Result[3][2] = -(static_cast<T>(2) * far_plane * near_plane) / (far_plane - near_plane);
	return Result;
}

template <typename T>
Matrix<T, 4> Perspective(T fov, T aspect, T near_plane, T far_plane) {
#ifdef GLVM_OPENGL_RENDER_BIT
	return GLVM_perspectiveRH_NO<T>(fov, aspect, near_plane, far_plane);
#else
	return GLVM_perspectiveRH_ZO<T>(fov, aspect, near_plane, far_plane);
#endif
}

template <typename T>
Matrix<T, 4> lookAtRH(Vector<T, 3> _eye, Vector<T, 3> _center, Vector<T, 3> _up)
{
    Vector<T, 3> f = (Normalize(_center - _eye));
    Vector<T, 3> s = (Normalize(Cross(f, _up)));
    Vector<T, 3> u = (Cross(s, f));

    Matrix<T, 4> Result(1.0f);
    Result[0][0] = s[0];
    Result[1][0] = s[1];
    Result[2][0] = s[2];
    Result[0][1] = u[0];
    Result[1][1] = u[1];
    Result[2][1] = u[2];
    Result[0][2] = -f[0];
    Result[1][2] = -f[1];
    Result[2][2] = -f[2];
    Result[3][0] = -Dot(s, _eye);
    Result[3][1] = -Dot(u, _eye);
    Result[3][2] = Dot(f, _eye);
//    Result.SelfTensorTranspose();

    return Result;
}

template<typename T>
Matrix<T, 4> lookAtLH(Vector<T, 3> _eye, Vector<T, 3> _center, Vector<T, 3> _up)
{
	Vector<T, 3> f(Normalize(_center - _eye));
	Vector<T, 3> s(Normalize(Cross(_up, f)));
	Vector<T, 3> u(Cross(f, s));

	Matrix<T, 4> Result(1);
	Result[0][0] = s[0];
	Result[1][0] = s[1];
	Result[2][0] = s[2];
	Result[0][1] = u[0];
	Result[1][1] = u[1];
	Result[2][1] = u[2];
	Result[0][2] = f[0];
	Result[1][2] = f[1];
	Result[2][2] = f[2];
	Result[3][0] = -Dot(s, _eye);
	Result[3][1] = -Dot(u, _eye);
	Result[3][2] = -Dot(f, _eye);
	return Result;
}

template <typename T>
Matrix<T, 4> LookAtMain(Vector<T, 3> _eye, Vector<T, 3> _center, Vector<T, 3> _up)
{
#ifdef GLVM_OPENGL_RENDER_BIT
	return lookAtLH<T>(_eye, _center, _up);
#else
	return lookAtRH<T>(_eye, _center, _up);
#endif
}

template <typename T>
Matrix<T, 4> FPSview(Vector<T, 3> _eye, Vector<T, 3> _center, Vector<T, 3> _up)
{
//    _center.m_vector[1] = _eye.m_vector[1];
    
    Vector<T, 3> f(Normalize(_center - _eye));
    Vector<T, 3> s(Normalize(Cross(f, _up)));
    Vector<T, 3> u(Cross(s, f));

    Matrix<T, 4> Result(1.0f);
    Result[0][0] = s[0];
    Result[1][0] = s[1];
    Result[2][0] = s[2];
    Result[0][1] = u[0];
    Result[1][1] = u[1];
    Result[2][1] = u[2];
    Result[0][2] = -f[0];
    Result[1][2] = -f[1];
    Result[2][2] = -f[2];
    Result[3][0] = -Dot(s, _eye);
    Result[3][1] = -Dot(u, _eye);
    Result[3][2] = Dot(f, _eye);
//    Result.SelfTensorTranspose();

    return Result;
}

template <typename T3>
T3 Radians(T3 _angle)
{
    _angle *= PI / static_cast<T3>(180);

    return _angle;
}

template <typename T>
Matrix<T, 4> FPS_View_RH(Vector<T, 3> _eye, float _pitch, float _yaw)
{
    _pitch *= PI / 180;
    _yaw *= PI / 180;

    float fCos_Pitch = std::cos(_pitch);
    float fSin_Pitch = std::sin(_pitch);
    float fCos_Yaw = std::cos(_yaw);
    float fSin_Yaw = std::sin(_yaw);

    Vector<T, 3> x_axis(fCos_Yaw, 0, -fSin_Yaw);
    Vector<T, 3> y_axis(fSin_Yaw * fSin_Pitch, fCos_Pitch, fCos_Yaw * fSin_Pitch);
    Vector<T, 3> z_axis(fSin_Yaw * fCos_Pitch, -fSin_Pitch, fCos_Pitch * fCos_Yaw);

    Matrix<T, 4> tView(Vector<T, 4>(x_axis[0], y_axis[0], z_axis[0], 0),
                       Vector<T, 4>(x_axis[1], y_axis[1], z_axis[1], 0),
                       Vector<T, 4>(x_axis[2], y_axis[2], z_axis[2], 0),
                       Vector<T, 4>(-Dot(x_axis, _eye), -Dot(y_axis, _eye), -Dot(z_axis, _eye), 1));

    return tView;
}

template <class T, int var, int vec_size>
Matrix<T, var> Rotate(Vector<T, vec_size> vector, float angle)
{
    vector = (Normalize(vector));

//	angle *= PI / 180;
	Matrix<T, var> tempMatrix(1.0f);

    /// Transposed rotate matrix.
    
	tempMatrix[0][0] = std::cos(angle) + (vector[0] * vector[0]) * (static_cast<T>(1) - std::cos(angle));
	tempMatrix[1][0] = vector[0] * vector[1] * (static_cast<T>(1) - std::cos(angle)) - vector[2] * std::sin(angle);
	tempMatrix[2][0] = vector[0] * vector[2] * (static_cast<T>(1) - std::cos(angle)) + vector[1] * std::sin(angle);
	tempMatrix[3][0] = static_cast<T>(0);
	tempMatrix[0][1] = vector[1] * vector[0] * (static_cast<T>(1) - std::cos(angle)) + vector[2] * std::sin(angle);
	tempMatrix[1][1] = std::cos(angle) + (vector[1] * vector[1]) * (static_cast<T>(1) - std::cos(angle));
	tempMatrix[2][1] = vector[1] * vector[2] * (static_cast<T>(1) - std::cos(angle)) - vector[0] * std::sin(angle);
	tempMatrix[3][1] = static_cast<T>(0);
	tempMatrix[0][2] = vector[2] * vector[0] * (static_cast<T>(1) - std::cos(angle)) - vector[1] * std::sin(angle);
	tempMatrix[1][2] = vector[2] * vector[1] * (static_cast<T>(1) - std::cos(angle)) + vector[0] * std::sin(angle);
	tempMatrix[2][2] = std::cos(angle) + (vector[2] * vector[2]) * (static_cast<T>(1) - std::cos(angle));
	tempMatrix[3][2] = static_cast<T>(0);
	tempMatrix[0][3] = static_cast<T>(0);
	tempMatrix[1][3] = static_cast<T>(0);
	tempMatrix[2][3] = static_cast<T>(0);
	tempMatrix[3][3] = static_cast<T>(1);

    /// Normal rotate matrix.
    
	// tempMatrix[0][0] = std::cos(angle) + (vector[0] * vector[0]) * (static_cast<T>(1) - std::cos(angle));
	// tempMatrix[0][1] = vector[0] * vector[1] * (static_cast<T>(1) - std::cos(angle)) - vector[2] * std::sin(angle);
	// tempMatrix[0][2] = vector[0] * vector[2] * (static_cast<T>(1) - std::cos(angle)) + vector[1] * std::sin(angle);
	// tempMatrix[0][3] = static_cast<T>(0);
	// tempMatrix[1][0] = vector[1] * vector[0] * (static_cast<T>(1) - std::cos(angle)) + vector[2] * std::sin(angle);
	// tempMatrix[1][1] = std::cos(angle) + (vector[1] * vector[1]) * (static_cast<T>(1) - std::cos(angle));
	// tempMatrix[1][2] = vector[1] * vector[2] * (static_cast<T>(1) - std::cos(angle)) - vector[0] * std::sin(angle);
	// tempMatrix[1][3] = static_cast<T>(0);
	// tempMatrix[2][0] = vector[2] * vector[0] * (static_cast<T>(1) - std::cos(angle)) - vector[1] * std::sin(angle);
	// tempMatrix[2][1] = vector[2] * vector[1] * (static_cast<T>(1) - std::cos(angle)) + vector[0] * std::sin(angle);
	// tempMatrix[2][2] = std::cos(angle) + (vector[2] * vector[2]) * (static_cast<T>(1) - std::cos(angle));
	// tempMatrix[2][3] = static_cast<T>(0);
	// tempMatrix[3][0] = static_cast<T>(0);
	// tempMatrix[3][1] = static_cast<T>(0);
	// tempMatrix[3][2] = static_cast<T>(0);
	// tempMatrix[3][3] = static_cast<T>(1);
    
    // tempMatrix[0][0] = std::cos(angle) + (vector[0] * vector[0]) * (1 - std::cos(angle));
	// tempMatrix[0][1] = vector[0] * vector[1] * (1 - std::cos(angle)) - vector[2] * std::sin(angle);
	// tempMatrix[0][2] = vector[0] * vector[2] * (1 - std::cos(angle)) + vector[1] * std::sin(angle);
	// tempMatrix[0][3] = 0;
	// tempMatrix[1][0] = vector[1] * vector[0] * (1 - std::cos(angle)) + vector[2] * std::sin(angle);
	// tempMatrix[1][1] = std::cos(angle) + (vector[1] * vector[1]) * (1 - std::cos(angle));
	// tempMatrix[1][2] = vector[1] * vector[2] * (1 - std::cos(angle)) - vector[0] * std::sin(angle);
	// tempMatrix[1][3] = 0;
	// tempMatrix[2][0] = vector[2] * vector[0] * (1 - std::cos(angle)) - vector[1] * std::sin(angle);
	// tempMatrix[2][1] = vector[2] * vector[1] * (1 - std::cos(angle)) + vector[0] * std::sin(angle);
	// tempMatrix[2][2] = std::cos(angle) + (vector[2] * vector[2]) * (1 - std::cos(angle));
	// tempMatrix[2][3] = 0;
	// tempMatrix[3][0] = 0;
	// tempMatrix[3][1] = 0;
	// tempMatrix[3][2] = 0;
	// tempMatrix[3][3] = 1;

    // Matrix<float, 4> model_mat(1.0f);
    // model_mat = matrix * tempMatrix;
    
	return tempMatrix;
}

template <class T, int var>
Matrix<T, var> Ortho(float w, float h, float zn, float zf)
{
	Matrix<T, var> tempMatrix(1.0f);
	tempMatrix[0][0] = 2/w;
	tempMatrix[1][1] = 2/h;
	tempMatrix[2][2] = 1/(zf-zn);

	return tempMatrix;
}

template <class T>
Matrix<T, 4> orthoRH_ZO(T left, T right, T bottom, T top, T near_plane, T far_plane) {
	Matrix<float, 4> tempMatrix(1);
	tempMatrix[0][0] = static_cast<T>(2) / (right - left);
	tempMatrix[1][1] = static_cast<T>(2) / (top - bottom);
	tempMatrix[2][2] = - static_cast<T>(1) / (far_plane - near_plane);
	tempMatrix[3][0] = - (right + left) / (right - left);
	tempMatrix[3][1] = - (top + bottom) / (top - bottom);
	tempMatrix[3][2] = - near_plane / (far_plane - near_plane);

	return tempMatrix;
}

template <class T>
Matrix<T, 4> orthoLH_NO(T left, T right, T bottom, T top, T near_plane, T far_plane) {
	Matrix<float, 4> tempMatrix(1);
	tempMatrix[0][0] = static_cast<T>(2) / (right - left);
	tempMatrix[1][1] = static_cast<T>(2) / (top - bottom);
	tempMatrix[2][2] = - static_cast<T>(2) / (far_plane - near_plane);
	tempMatrix[3][0] = - (right + left) / (right - left);
	tempMatrix[3][1] = - (top + bottom) / (top - bottom);
	tempMatrix[3][2] = - (far_plane + near_plane) / (far_plane - near_plane);

	return tempMatrix;
}

template <class T>
Matrix<T, 4> ortho(T left, T right, T bottom, T top, T near_plane, T far_plane) {
#ifdef GLVM_OPENGL_RENDER_BIT
	return orthoLH_NO<T>(left, right, bottom, top, near, far);
#else
	return orthoRH_ZO<T>(left, right, bottom, top, near_plane, far_plane);
#endif
}

template <class T, int var>
Matrix<T, var> perspectiveRH_ZO(T fov, T aspect, T near_plane, T far_plane) {
	const float tanHalfFov = std::tan((fov * 0.5) * (PI / 360));
	
	Matrix<float, var> tempMatrix(static_cast<T>(0));
	tempMatrix[0][0] = static_cast<T>(1) / (aspect * tanHalfFov);
	tempMatrix[1][1] = static_cast<T>(1) / tanHalfFov;
	tempMatrix[2][2] = far_plane / (near_plane - far_plane);
	tempMatrix[2][3] = static_cast<T>(1);
	tempMatrix[3][2] = -(far_plane * near_plane) / (far_plane - near_plane);

	return tempMatrix;
}

template <class T, int var>
Matrix<T, var> perspectiveLH_NO(T fov, T aspect, T near_plane, T far_plane) {
	const float tanHalfFov = std::tan((fov * 0.5) * (PI / 360));
	
	Matrix<float, var> tempMatrix(static_cast<T>(0));
	tempMatrix[0][0] = static_cast<T>(1) / (aspect * tanHalfFov);
	tempMatrix[1][1] = static_cast<T>(1) / tanHalfFov;
	tempMatrix[2][2] = - (far_plane + near_plane) / (far_plane - near_plane);
	tempMatrix[2][3] = - static_cast<T>(1);
	tempMatrix[3][2] = - (static_cast<T>(2) * far_plane * near_plane) / (far_plane - near_plane);

	return tempMatrix;
}

template <class T, int var>
Matrix<T, var> Perspective(const T fov, const T aspect, const T near_plane, const T far_plane) {
#ifdef GLVM_OPENGL_RENDER_BIT
	return perspectiveLH_NO(fov, aspect, near, far);
#else
	return perspectiveRH_ZO(fov, aspect, near_plane, far_plane);
#endif
	// float S = std::tan((fov/2)*(PI/360));
	// Matrix<float, var> tempMatrix;
	// tempMatrix[0][0] = 1/((1280.0f/1280.0f)*S);
	// tempMatrix[1][1] = 1/S;
	// tempMatrix[2][2] = -((f+n)/(f-n));
	// tempMatrix[2][3] = -((2*f*n)/(f-n));
	// tempMatrix[3][2] = -1;
}

constexpr float Max(float var1, float var2) {
	return var1 > var2 ? var1 : var2;
}

constexpr float Min(float var1, float var2) {
	return var1 < var2 ? var1 : var2;
}

struct Quaternion
{
	float w, x, y, z;
	Quaternion() = default;
	Quaternion(float _w, float _x, float _y, float _z) : w(_w), x(_x), y(_y), z(_z) {}
	Quaternion(float real, vec3 imaginary) : w(real), x(imaginary[0]), y(imaginary[1]), z(imaginary[2]) {}
};

inline std::ostream& operator<<(std::ostream& ostream, const Quaternion& quaternion) {
	ostream << "w: " << quaternion.w << " x: " << quaternion.x << " y: " << quaternion.y << " z: " << quaternion.z;
	
	return ostream;
}

inline Quaternion conjugate(Quaternion quaternion) {
	quaternion.w = quaternion.w;
	quaternion.x = -quaternion.x;
	quaternion.y = -quaternion.y;
	quaternion.z = -quaternion.z;
	
	return quaternion;
}

inline Quaternion multiplyQuaternion(const Quaternion& a, const Quaternion& b) {
	Quaternion result;
	result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
	result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
	result.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
	result.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;

	return result;
}

inline Quaternion operator*(const Quaternion& a, const Quaternion& b) {
	Quaternion result;
	result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
	result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
	result.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
	result.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;

	return result;
}

inline float normQuaternion(const Quaternion& quaternion) {
	return sqrt(quaternion.w * quaternion.w + quaternion.x * quaternion.x +
				quaternion.y * quaternion.y + quaternion.z * quaternion.z);
}

inline Quaternion normalizeQuaternion(Quaternion quaternion) {
	float norm = normQuaternion(quaternion);
	float inverseNorm = 1.0f / norm;
	
	quaternion.w *= inverseNorm;
	quaternion.x *= inverseNorm;
	quaternion.y *= inverseNorm;
	quaternion.z *= inverseNorm;

	return quaternion;
}

inline Quaternion inverseQuaternion(Quaternion quaternion) {
	Quaternion linkedValue = conjugate(quaternion);
	float norm = normQuaternion(quaternion);
	float inverseNorm = 1.0f / norm;

	quaternion.w = linkedValue.w * inverseNorm;
	quaternion.x = linkedValue.x * inverseNorm;
	quaternion.y = linkedValue.y * inverseNorm;
	quaternion.z = linkedValue.z * inverseNorm;
	
	return quaternion;
}

inline Quaternion eulerToQuaternion(const float roll, const float pitch, const float yaw) {
	const float cr = cos(roll * 0.5);
    const float sr = sin(roll * 0.5);
    const float cp = cos(pitch * 0.5);
    const float sp = sin(pitch * 0.5);
    const float cy = cos(yaw * 0.5);
    const float sy = sin(yaw * 0.5);

    Quaternion q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;

	return q;
}

template <class T, int var>
Matrix<T, var> rotateQuaternion(Quaternion quaternion) {
	mat4 result(0.0f);

	quaternion = normalizeQuaternion(quaternion);

	result[0][0] = 1 - 2 * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
	result[0][1] = 2 * (quaternion.x * quaternion.y - quaternion.z * quaternion.w);
	result[0][2] = 2 * (quaternion.x * quaternion.z + quaternion.y * quaternion.w);

	result[1][0] = 2 * (quaternion.x * quaternion.y + quaternion.z * quaternion.w);
	result[1][1] = 1 - 2 * (quaternion.x * quaternion.x + quaternion.z * quaternion.z);
	result[1][2] = 2 * (quaternion.y * quaternion.z - quaternion.x * quaternion.w);

	result[2][0] = 2 * (quaternion.x * quaternion.z - quaternion.y * quaternion.w);
	result[2][1] = 2 * (quaternion.y * quaternion.z + quaternion.x * quaternion.w);
	result[2][2] = 1 - 2 * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
	
	result[0][0] = 2 * (quaternion.w * quaternion.w + quaternion.x * quaternion.x) - 1;
	result[0][1] = 2 * (quaternion.x * quaternion.y - quaternion.w * quaternion.z);
	result[0][2] = 2 * (quaternion.x * quaternion.z + quaternion.w * quaternion.y);

	result[3][3] = 1.0f;

	return result;
}

inline Frustum extractFrustum( const mat4& vp ) {
	Frustum frustum;
		
	frustum.planes[Frustum::PlaneIndex::LEFT_PLANE] = {
		{
			vp[0][3] + vp[0][0],
			vp[1][3] + vp[1][0],
			vp[2][3] + vp[2][0]
		},
		vp[3][3] + vp[3][0]
	};

	frustum.planes[Frustum::PlaneIndex::RIGHT_PLANE] = {
		{
			vp[0][3] - vp[0][0],
			vp[1][3] - vp[1][0],
			vp[2][3] - vp[2][0]
		},
		vp[3][3] - vp[3][0]
	};

	frustum.planes[Frustum::PlaneIndex::BOTTOM_PLANE] = {
		{
			vp[0][3] + vp[0][1],
			vp[1][3] + vp[1][1],
			vp[2][3] + vp[2][1]
		},
		vp[3][3] + vp[3][1]
	};

	frustum.planes[Frustum::PlaneIndex::TOP_PLANE] = {
		{
			vp[0][3] - vp[0][1],
			vp[1][3] - vp[1][1],
			vp[2][3] - vp[2][1]
		},
		vp[3][3] - vp[3][1]
	};

	frustum.planes[Frustum::PlaneIndex::NEAR_PLANE] = {
		{
			vp[0][3] + vp[0][2],
			vp[1][3] + vp[1][2],
			vp[2][3] + vp[2][2]
		},
		vp[3][3] + vp[3][2]
	};

	frustum.planes[Frustum::PlaneIndex::FAR_PLANE] = {
		{
			vp[0][3] - vp[0][2],
			vp[1][3] - vp[1][2],
			vp[2][3] - vp[2][2]
		},
		vp[3][3] - vp[3][2]
	};

	for ( plane& plane : frustum.planes ) {
		const float length = plane.normal.Length();
		if ( length <= 1e-6f )              ///< Degenerate view-projection matrix: leave the plane unnormalized
			continue;
		plane.normal   = Normalize( plane.normal );
		plane.distance = plane.distance / length;
	}

	return frustum;
}

inline bool isFrustumIntersect(const Frustum& frustum, const AABB& aabb) {
	for (int i = 0; i < 6; ++i) {
		const plane& plane = frustum.planes[i];

		float boxExtentsProjectionOnPlaneNormal = aabb.extents[0] * std::abs(plane.normal[0]) +
			aabb.extents[1] * std::abs(plane.normal[1]) +
			aabb.extents[2] * std::abs(plane.normal[2]);

		float boxCentreToPlaneDistance = (plane.normal[0] * aabb.origin[0]) + 
			(plane.normal[1] * aabb.origin[1]) + 
			(plane.normal[2] * aabb.origin[2]) + 
			plane.distance;

		if ( boxCentreToPlaneDistance < -boxExtentsProjectionOnPlaneNormal ) {
			return false;
		}
	}

	return true; 
}

inline AABB computeLocalAABB( const float scale, vec3 originOffsets, vec3 extents ) {
	const vec3 scaledOffsets = {
		originOffsets[0] * scale,
		originOffsets[1] * scale,
		originOffsets[2] * scale };

	const vec3 scaledExtents = {
		extents[0] * scale,
		extents[1] * scale,
		extents[2] * scale };

	return { .origin = scaledOffsets, .extents = scaledExtents };
}

inline AABB computeWorldAABB( const AABB aabb, vec3 position ) {
	return { .origin = position + aabb.origin, .extents = aabb.extents };
}

#endif
