using generic_t = intptr_t;
class MemorySegment
{
public:
	size_t size;
	bool dirty;
	MemorySegment *next;
	generic_t data[1];
	MemorySegment():next(nullptr),dirty(false){}
};

enum class SearchMode
{
	firstFit,
	bestFit
};
