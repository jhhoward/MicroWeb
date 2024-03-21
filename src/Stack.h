#ifndef _STACK_H_
#define _STACK_H_

#include "Memory/Memory.h"

template <typename T>
class Stack
{
public:
	Stack(Allocator& inAllocator) : allocator(inAllocator)
	{
		Reset();
	}

	void Reset()
	{
		top = &base;
		top->next = top->prev = nullptr;
	}

	T& Top()
	{
		return top->obj;
	}

	void Push()
	{
		if (top->next)
		{
			top->next->obj = top->obj;
			top = top->next;
		}
		else
		{
			Entry* newEntry = allocator.Alloc<Entry>();
			if (newEntry)
			{
				top->next = newEntry;
				newEntry->prev = top;
				newEntry->next = nullptr;
				newEntry->obj = top->obj;
				top = newEntry;
			}
			else
			{
				// TODO: handle allocation error more gracefully
				Platform::FatalError("Could not push to stack: out of memory");
			}
		}
	}

	void Pop()
	{
		if (top->prev)
		{
			top = top->prev;
		}
	}

	struct Entry
	{
		T obj;
		Entry* next;
		Entry* prev;
	};

	Entry* top;

private:
	Entry base;
	Allocator& allocator;
};

#endif
