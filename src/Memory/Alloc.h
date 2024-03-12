#ifndef _ALLOC_H_
#define _ALLOC_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <malloc.h>
#pragma warning(disable:4996)

class Allocator
{
public:
	virtual void* Allocate(size_t numBytes) = 0;

	char* AllocString(const char* inString)
	{
		char* result = (char*)Allocate(strlen(inString) + 1);
		if (result)
		{
			strcpy(result, inString);
		}
		return result;
	}

	char* AllocString(const char* inString, size_t length)
	{
		char* result = (char*)Allocate(length + 1);
		if (result)
		{
			memcpy(result, inString, length);
			result[length] = '\0';
		}
		return result;
	}

	template <typename T>
	T* Alloc()
	{
		void* mem = Allocate(sizeof(T));
		if (mem)
		{
			return new (mem) T();
		}
		return nullptr;
	}

	template <typename T, typename A>
	T* Alloc(A a)
	{
		void* mem = Allocate(sizeof(T));
		if (mem)
		{
			return new (mem) T(a);
		}
		return nullptr;
	}

	template <typename T, typename A, typename B>
	T* Alloc(A a, B b)
	{
		void* mem = Allocate(sizeof(T));
		if (mem)
		{
			return new (mem) T(a, b);
		}
		return nullptr;
	}

	template <typename T, typename A, typename B, typename C>
	T* Alloc(A a, B b, C c)
	{
		void* mem = Allocate(sizeof(T));
		if (mem)
		{
			return new (mem) T(a, b, c);
		}
		return nullptr;
	}
};

class MallocWrapper : public Allocator
{
public:
	virtual void* Allocate(size_t numBytes)
	{
		return malloc(numBytes);
	}

};

#endif
