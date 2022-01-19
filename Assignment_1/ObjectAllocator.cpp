#include "ObjectAllocator.h"


// Creates the ObjectManager per the specified values
    // Throws an exception if the construction fails. (Memory allocation problem)
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
{

}

// Destroys the ObjectManager (never throws)
ObjectAllocator::~ObjectAllocator()
{

}

// Take an object from the free list and give it to the client (simulates new)
// Throws an exception if the object can't be allocated. (Memory allocation problem)
void* ObjectAllocator::Allocate(const char* label = 0)
{

}

// Returns an object to the free list for the client (simulates delete)
// Throws an exception if the the object can't be freed. (Invalid object)
void ObjectAllocator::Free(void* Object)
{

}

// Calls the callback fn for each block still in use
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const
{

}

// Calls the callback fn for each block that is potentially corrupted
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const
{

}

// Frees all empty pages (extra credit)
unsigned ObjectAllocator::FreeEmptyPages(void)
{

}

// Returns true if FreeEmptyPages and alignments are implemented
static bool ObjectAllocator::ImplementedExtraCredit(void)
{

}

// Testing/Debugging/Statistic methods
void         ObjectAllocator::SetDebugState(bool State)
{

}
    // true=enable, false=disable
const void*  ObjectAllocator::GetFreeList(void) const;   // returns a pointer to the internal free list
const void*  ObjectAllocator::GetPageList(void) const;   // returns a pointer to the internal page list
OAConfig     ObjectAllocator::GetConfig(void) const;     // returns the configuration parameters
OAStats      ObjectAllocator::GetStats(void) const;      // returns the statistics for the allocator
