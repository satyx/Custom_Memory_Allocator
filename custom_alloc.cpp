#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <iostream>
#include <memory>
#include <stddef.h>
#include <unistd.h>
#include <new>
#include <assert.h>
#include "custom_alloc.h"

static MemorySegment *segmentListHead = nullptr, *heapStart = nullptr;
static MemorySegment *segmentListTop = segmentListHead;
static auto searchMode = SearchMode::firstFit;

inline size_t align(size_t size)
{
	return (size + sizeof(generic_t)-1) & ~(sizeof(generic_t)-1);
}

inline MemorySegment* getHeader(generic_t* segment)
{
	return (MemorySegment *)((char*)segment + (sizeof(std::declval<MemorySegment>().data)-sizeof(MemorySegment)));
}

inline size_t allocSize(size_t size) {
  return size + sizeof(MemorySegment) - sizeof(std::declval<MemorySegment>().data);
}


MemorySegment* demandKernel(size_t size)
{
	auto segment = (MemorySegment*) sbrk(0);
	if(sbrk(allocSize(size)) == (void*)-1 )
		return nullptr;
	if(heapStart == nullptr)
		heapStart = segment;
	return segment;
}

MemorySegment* firstFit(size_t size)
{
	MemorySegment *currSegment = segmentListHead, *prevSegment = nullptr;
	while(currSegment!=nullptr)
	{
		if(!currSegment->dirty && currSegment->size >= size)
			break;
		prevSegment = currSegment;
		currSegment = currSegment->next;
	}

	if(currSegment!=nullptr)
	{
		if(prevSegment==nullptr)
			segmentListHead = currSegment->next;
		else
			prevSegment->next = currSegment->next;
	}
	return currSegment;
}

MemorySegment* bestFit(size_t size)
{
	MemorySegment *block = segmentListHead, *ret = nullptr;
	int currDiff = INT_MAX;

	while(block!=nullptr)
	{
		if(!block->dirty && block->size>= size && currDiff>block->size-size)
		{
			currDiff = block->size-size;
			ret = block;
			if(currDiff==0)
				break;
		}
		block = block->next;
	}
	return ret;
}

MemorySegment* findMemorySegment(size_t size)
{
	switch(searchMode)
	{
		case SearchMode::firstFit:
			return firstFit(size);
		case SearchMode::bestFit:
			return bestFit(size);	
	}
}

generic_t* alloc(size_t reqSize)
{
	size_t size = align(reqSize);
	if(auto segment = findMemorySegment(size))
	{
		segment->dirty = true;
		return segment->data;
	}
	auto segment = demandKernel(size);
	
	if(segment == nullptr)
		throw std::bad_alloc();
	segment->dirty = true;
	segment->size = size;
	return segment->data;
}

void free(void* space1)
{
	generic_t* space = reinterpret_cast<generic_t*>(space1);
	auto segment = getHeader(space);
	segment->dirty = false;
	if(segmentListHead==nullptr)
		segmentListHead = segment;
	else
		segmentListTop->next = segment;
	segmentListTop = segment;
}

// vulnerable to memory leak.
// Memory isn't returned back to OS for simplicity purpose
// otherwise systemcall int madvise(void *addr, size_t length, int advice) can be used with advice = MADV_FREE
void init(SearchMode mode)
{
	if(heapStart == nullptr)
		return;
	brk(heapStart);
	heapStart = nullptr;
	segmentListHead = nullptr;
	segmentListTop = nullptr;
	searchMode = mode;
}

// driver code
int main()
{
	auto p1 = alloc(8);
	auto p1Segment = getHeader(p1);
	free(p1);
	auto p2 = alloc(8);
	auto p2Segment = getHeader(p2);
	assert(p1Segment == p2Segment);
	auto pp = alloc(sizeof(int)*10);
	int *p = (int*)(pp);
	
	for(int i=0;i<10;i++)
		p[i] = i*10;
	for(int i=0;i<10;i++)
		std::cout<<p[i]<<std::endl;
	free(p);
	return 0;
}

// // driver code for using custom allocator with STL
// #include <vector>
// template <class T>
// struct custom_allocator {
//   typedef T value_type;
//   custom_allocator() noexcept {}
//   template <class U> custom_allocator (const custom_allocator<U>&) noexcept {}
//   T* allocate (std::size_t n) { return reinterpret_cast<T*>(alloc(n*sizeof(T))); }
//   void deallocate (T* p, std::size_t n) { free(p); }
// };

// template <class T, class U>
// constexpr bool operator== (const custom_allocator<T>&, const custom_allocator<U>&) noexcept
// {return true;}

// template <class T, class U>
// constexpr bool operator!= (const custom_allocator<T>&, const custom_allocator<U>&) noexcept
// {return false;}


// int main () {
//   std::vector<int,custom_allocator<int>> foo = {10,20,30};
//   for (auto x: foo) std::cout << x << " ";
//   std::cout <<"\n";
//   return 0;
// }
