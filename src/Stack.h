#ifndef _STACK_H_
#define _STACK_H_

#include "Memory/Memory.h"

template <typename T>
class Stack
{
public:
	Stack(LinearAllocator& inAllocator) : allocator(inAllocator)
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
				// TODO: handle allocation error
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

private:
	struct Entry
	{
		T obj;
		Entry* next;
		Entry* prev;
	};

	LinearAllocator& allocator;
	Entry base;
	Entry* top;
};

#endif
