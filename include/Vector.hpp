// This file is part of Game Loop Versatile Modules (GLVM)
// Copyright © 2024 Maksim Manokhin a.k.a. Yuriorkis_Scream. Contacts: <fellfrostqtw@gmail.com>
// Author: Maksim Manokhin a.k.a. Yuriorkis_Scream
// License: http://opensource.org/licenses/MIT

#ifndef VECTOR_CONTAINER_HPP
#define VECTOR_CONTAINER_HPP

#include "Constants.hpp"
#include "IContainer.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <new>
#include <utility>
#include "VertexMath.hpp"
#include <assert.h>
#include "Iterator.hpp"

namespace GLVM::core
{
	template <class T>
	class vector;

	/// Half-open iterator: [current, end). It is valid while current < end.
	template <class T>
	class VectorIterator : public Iterator<T>
	{
		T* current;
		T* end;

	public:
		VectorIterator(vector<T>& vector) {
			current = vector.GetVectorContainer();
			end     = current + vector.GetSize();
		}

		bool Next() override {
			if ( !ValidStatus() )
				return false;

			++current;
			return ValidStatus();
		}

 		bool ValidStatus() override {
			return current != nullptr && current < end;
		}

		T& Current() override {
			return *current;
		}

		T& Last() override {
			return *(end - 1);
		}
	};

	template<class T>
	class vector : public IContainer
	{
		unsigned int size = 0;
		unsigned int capacity = 0;
		unsigned char* rowInnerData = nullptr;

		static constexpr unsigned int minimalCapacity = 8;

		T* Data() { return reinterpret_cast<T*>(rowInnerData); }
		const T* Data() const { return reinterpret_cast<const T*>(rowInnerData); }

		static unsigned char* Allocate(const unsigned int count);
		static void Deallocate(unsigned char* data);
		unsigned int CalculateGrownCapacity(const unsigned int requiredCapacity) const;
		void Reallocate(const unsigned int newCapacity);
		template<class U>
		void PushImpl(U&& item);
		void DestroyElements();
	public:
        vector() = default;
        vector(const vector<T>& _vector);
        vector(vector<T>&& _vector) noexcept;
        ~vector() override;
		void Push(const T& item);
		void Push(T&& item);
		void Pop();
		void Swap(T& firstElement, T& secondElement);
		VectorIterator<T> Find(T& element);
		void Resize(const unsigned int index);
		void Remove(unsigned int index);
		void RemoveFirstItem();
		T& GetFirstItem();
		T& GetHead();
		T* GetVectorContainer();
		[[nodiscard]] unsigned int GetSize() const;
		int GetCapacity();
		const T& operator[](const unsigned int _iIndex) const;
		T& operator[](const unsigned int _iIndex);
		void clear();
        void Print();
        vector& operator=(const vector<T>& _vector);
        vector& operator=(vector<T>&& _vector) noexcept;
        bool operator==(const char* string_);
		bool empty();
	};

	template <class T>
	unsigned char* vector<T>::Allocate(const unsigned int count) {
		if ( count == 0 )
			return nullptr;

		return static_cast<unsigned char*>(::operator new(static_cast<std::size_t>(count) * sizeof(T)));
	}

	template <class T>
	void vector<T>::Deallocate(unsigned char* data) {
		::operator delete(static_cast<void*>(data));
	}

	/// Geometric (x2) growth. Computed in 64 bit to avoid overflow of the 32 bit counters.
	template <class T>
	unsigned int vector<T>::CalculateGrownCapacity(const unsigned int requiredCapacity) const {
		uint64_t newCapacity = capacity < minimalCapacity ? minimalCapacity : static_cast<uint64_t>(capacity) * 2;
		if ( newCapacity < requiredCapacity )
			newCapacity = requiredCapacity;
		if ( newCapacity > UINT32_MAX )
			newCapacity = UINT32_MAX;

		return static_cast<unsigned int>(newCapacity);
	}

	/// Move all alive elements into a new buffer of newCapacity elements (newCapacity >= size).
	template <class T>
	void vector<T>::Reallocate(const unsigned int newCapacity) {
		unsigned char* newData = Allocate(newCapacity);
		T* newElements = reinterpret_cast<T*>(newData);
		T* oldElements = Data();
		for ( unsigned int i = 0; i < size; ++i ) {
			new (&newElements[i]) T(std::move(oldElements[i]));
			oldElements[i].~T();
		}

		Deallocate(rowInnerData);
		rowInnerData = newData;
		capacity = newCapacity;
	}

	template <class T>
	void vector<T>::DestroyElements() {
		T* elements = Data();
		for ( unsigned int i = 0; i < size; ++i )
			elements[i].~T();

		size = 0;
	}

    template <class T>
    bool vector<T>::operator==(const char* string_) {
		const std::size_t strSize = std::strlen(string_);
		if ( strSize != size )
			return false;

        for (unsigned int i = 0; i < size; ++i) {
            if (Data()[i] != string_[i])
                return false;
        }

        return true;
    }

    template <class T>
    vector<T>& vector<T>::operator=(const vector<T>& _vector)
    {
        if(this == &_vector)
            return *this;

		DestroyElements();
		if ( _vector.size > capacity ) {
			Deallocate(rowInnerData);
			rowInnerData = Allocate(_vector.size);
			capacity     = _vector.size;
		}

		T* elements = Data();
		const T* sourceElements = _vector.Data();
        for(unsigned int i = 0; i < _vector.size; ++i)
			new (&elements[i]) T(sourceElements[i]);

		size = _vector.size;
        return *this;
    }

    template <class T>
    vector<T>& vector<T>::operator=(vector<T>&& _vector) noexcept
    {
        if(this == &_vector)
            return *this;

		DestroyElements();
		Deallocate(rowInnerData);

		rowInnerData = _vector.rowInnerData;
		size         = _vector.size;
		capacity     = _vector.capacity;

		_vector.rowInnerData = nullptr;
		_vector.size         = 0;
		_vector.capacity     = 0;

        return *this;
    }

    template <class T>
    vector<T>::vector(const vector<T>& _vector) : IContainer() {
		rowInnerData = Allocate(_vector.size);
		capacity     = _vector.size;

		T* elements = Data();
		const T* sourceElements = _vector.Data();
        for(unsigned int i = 0; i < _vector.size; ++i)
			new (&elements[i]) T(sourceElements[i]);

		size = _vector.size;
    }

    template <class T>
    vector<T>::vector(vector<T>&& _vector) noexcept : IContainer() {
		rowInnerData = _vector.rowInnerData;
		size         = _vector.size;
		capacity     = _vector.capacity;

		_vector.rowInnerData = nullptr;
		_vector.size         = 0;
		_vector.capacity     = 0;
    }

	template<class T>
	vector<T>::~vector() {
		DestroyElements();
		Deallocate(rowInnerData);
		rowInnerData = nullptr;
		capacity     = 0;
	}

	/// The new element is constructed before the old buffer is released, so pushing
	/// an element of this same vector (v.Push(v[0])) stays valid during reallocation.
	template<class T>
	template<class U>
	void vector<T>::PushImpl(U&& item) {
		if ( size == capacity ) {
			const unsigned int newCapacity = CalculateGrownCapacity(size + 1);
			unsigned char* newData = Allocate(newCapacity);
			T* newElements = reinterpret_cast<T*>(newData);
			new (&newElements[size]) T(std::forward<U>(item));

			T* oldElements = Data();
			for ( unsigned int i = 0; i < size; ++i ) {
				new (&newElements[i]) T(std::move(oldElements[i]));
				oldElements[i].~T();
			}

			Deallocate(rowInnerData);
			rowInnerData = newData;
			capacity     = newCapacity;
		} else {
			new (&Data()[size]) T(std::forward<U>(item));
		}

		++size;
	}

    /// Push element on top of the container.
	template<class T>
	void vector<T>::Push(const T& item) {
		PushImpl(item);
	}

	template<class T>
	void vector<T>::Push(T&& item) {
		PushImpl(std::move(item));
	}

	template <class T>
	void vector<T>::Pop() {
		if ( size < 1 )
			return;

		Data()[size - 1].~T();
		--size;
	}

	template <class T>
	void vector<T>::Swap(T& firstElement, T& secondElement) {
		if ( size < 1)
			return;

		if ( &firstElement == &secondElement ) {
		    return;
		}

		T tempElement  = std::move(firstElement);
		firstElement   = std::move(secondElement);
		secondElement  = std::move(tempElement);
	}

	/// Returns an iterator that points on the found element. If the element is not found the
	/// returned iterator is not valid (ValidStatus() == false).
	template <class T>
	VectorIterator<T> vector<T>::Find(T& element) {
		VectorIterator<T> iterator(*this);
		while ( iterator.ValidStatus() ) {
			if ( iterator.Current() == element )
				return iterator;

			iterator.Next();
		}

		return iterator;
	}

	/// Change number of elements. New elements are value-initialized.
	template<typename T>
	void vector<T>::Resize(const unsigned int index)
	{
		if ( index < size ) {
			T* elements = Data();
			for(unsigned int j = index; j < size; ++j)
				elements[j].~T();

			size = index;
		} else if( index > size ) {
			if ( index > capacity )
				Reallocate(CalculateGrownCapacity(index));

			T* elements = Data();
			for ( unsigned int i = size; i < index; ++i)
				new (&elements[i]) T{};

			size = index;
		}
	}

	/// Remove element with shifting of all next elements. Out of range index is ignored.
 	template<class T>
	void vector<T>::Remove(unsigned int index)
	{
		if ( index >= size )
			return;

		T* elements = Data();
		for(unsigned int j = index; j < size - 1; ++j) {
			elements[j].~T();
			new (&elements[j]) T(std::move(elements[j + 1]));
		}

		elements[size - 1].~T();
	    --size;
	}

	template<class T>
	void vector<T>::RemoveFirstItem()
	{
		Remove(0);
	}

	template<class T>
	T& vector<T>::GetFirstItem() { return Data()[0]; }

	template<class T>
	T& vector<T>::GetHead() { return Data()[size - 1]; }

	template<class T>
	T* vector<T>::GetVectorContainer() { return Data(); }

	template<typename T>
	unsigned int vector<T>::GetSize() const { return size; }

	template<typename T>
	int vector<T>::GetCapacity() { return capacity; }
	template<typename T>
	const T& vector<T>::operator[](const unsigned int _iIndex) const {
//		assert( _iIndex < size );
		return Data()[_iIndex];
	}
	template<typename T>
	T& vector<T>::operator[](const unsigned int _iIndex) {
//		assert( _iIndex < size );
		return Data()[_iIndex];
	}

	/// Destroy all elements. Allocated memory is kept for reuse.
	template<typename T>
	void vector<T>::clear() {
		DestroyElements();
	}

 	template<class T>
	bool vector<T>::empty() { return size == 0; }

    template<class T>
    void vector<T>::Print()
    {
        for(unsigned int i = 0; i < size; ++i)
            std::cout << Data()[i] << std::endl;

        std::cout << "End of container" << std::endl;
    }
}

#endif
