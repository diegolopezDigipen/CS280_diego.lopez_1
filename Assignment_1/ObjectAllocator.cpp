#include "ObjectAllocator.h"
#include "string.h"


// Creates the ObjectManager per the specified values
    // Throws an exception if the construction fails. (Memory allocation problem)
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
{
    
    configuration_ = config;
    unsigned pdBytes = configuration_.PadBytes_;
    unsigned hdBytes = configuration_.HBlockInfo_.size_;
    stats_.ObjectSize_ = ObjectSize;
    stats_.PageSize_ = configuration_.ObjectsPerPage_ * ObjectSize + sizeof(char*) + 2 * configuration_.ObjectsPerPage_ * pdBytes + hdBytes * configuration_.ObjectsPerPage_;
    stats_.FreeObjects_ = config.ObjectsPerPage_;
    stats_.ObjectsInUse_ = 0;
    stats_.PagesInUse_ = 1;
    stats_.MostObjects_ = 0;
    stats_.Allocations_ = 0;
    stats_.Deallocations_ = 0;

    
    bool header = (configuration_.hbBasic || configuration_.hbExtended || configuration_.hbExternal);
    char* Block;

    try {
        Block = new char[configuration_.ObjectsPerPage_ * ObjectSize + sizeof(char*) + 2 * configuration_.ObjectsPerPage_ * pdBytes];
    }
    catch (const std::exception&) { throw OAException(OAException::E_NO_MEMORY, "There is now memory, error when using new"); }

    if (pdBytes)
    {
        memset(Block + sizeof(GenericObject*), PAD_PATTERN, pdBytes);
        memset(Block + sizeof(GenericObject*) + pdBytes + ObjectSize, PAD_PATTERN, pdBytes);
    }

    memset(Block + sizeof(GenericObject*) + pdBytes + sizeof(void*), UNALLOCATED_PATTERN, ObjectSize - sizeof(void*));

    PageList_ = reinterpret_cast<GenericObject*>(Block);
    PageList_->Next = nullptr;


    FreeList_ = reinterpret_cast<GenericObject*>(reinterpret_cast<char*>((PageList_ + 1)+pdBytes));
    FreeList_->Next = nullptr;
    char* other = Block + sizeof(char*);


    for (int i = 1; i < configuration_.ObjectsPerPage_; i++)
    {    
        memset(other + pdBytes + i * (ObjectSize+2*pdBytes)+sizeof(void*), UNALLOCATED_PATTERN, ObjectSize-sizeof(void*));
        if (pdBytes)
        {
            memset(other + i * (ObjectSize + 2 * pdBytes), PAD_PATTERN, pdBytes);
            memset(other + pdBytes + i * (ObjectSize + 2 * pdBytes) + stats_.ObjectSize_, PAD_PATTERN, pdBytes);
        }
        
        
        GenericObject* prev = FreeList_;
        FreeList_ = reinterpret_cast<GenericObject*>(other + pdBytes + i * (ObjectSize + 2 * pdBytes));
        FreeList_->Next = prev;
    }
}


// Destroys the ObjectManager (never throws)
ObjectAllocator::~ObjectAllocator()
{

}
// Take an object from the free list and give it to the client (simulates new)
// Throws an exception if the object can't be allocated. (Memory allocation problem)
void* ObjectAllocator::Allocate(const char* label)
{  
    unsigned pdBytes = configuration_.PadBytes_;
    if (stats_.FreeObjects_ == 0 && stats_.PagesInUse_!= configuration_.MaxPages_)
    {
        try
        {
            if (stats_.PagesInUse_ != configuration_.MaxPages_)
            {
                char* Block;
                try {
                    Block = new char[configuration_.ObjectsPerPage_ * stats_.ObjectSize_ + sizeof(char*)];
                }
                catch (const std::exception&) { throw OAException(OAException::E_NO_MEMORY, "There is now memory, error when using new"); }

                stats_.PagesInUse_++;
                memset(Block + sizeof(char*), PAD_PATTERN, pdBytes);
                memset(Block + sizeof(char*) + pdBytes, UNALLOCATED_PATTERN, stats_.ObjectSize_);
                GenericObject* previous = PageList_;
                PageList_ = reinterpret_cast<GenericObject*>(Block);
                PageList_->Next = previous;

                FreeList_ = PageList_ + 1;
                FreeList_->Next = nullptr;
                char* other = Block + sizeof(char*);


                for (int i = 1; i < configuration_.ObjectsPerPage_; i++)
                {
                    memset(other + i * stats_.ObjectSize_, UNALLOCATED_PATTERN, stats_.ObjectSize_);
                    memset(other + i * stats_.ObjectSize_ + pdBytes, PAD_PATTERN, pdBytes);
                    GenericObject* prev = FreeList_;
                    FreeList_ = reinterpret_cast<GenericObject*>(other + i * stats_.ObjectSize_+pdBytes);
                    FreeList_->Next = prev;
                }

                stats_.FreeObjects_ += configuration_.ObjectsPerPage_;
            }
            else
            {
                throw(std::exception());
            }
        }
        catch (const std::exception&){ throw OAException(OAException::E_NO_PAGES, "Couldnt allocate, max number of ages reached coulndt allocate more space"); }
    }
   if(stats_.FreeObjects_ != 0)
   {    
       GenericObject* temp = FreeList_;

       if(FreeList_->Next)
        FreeList_ = FreeList_->Next;

       memset(temp, 0xBB, stats_.ObjectSize_);
       stats_.FreeObjects_--;
       stats_.ObjectsInUse_++;
       if (stats_.ObjectsInUse_ > stats_.MostObjects_)
       {
           stats_.MostObjects_ = stats_.ObjectsInUse_;
       }
       stats_.Allocations_++;
       return reinterpret_cast<void*>(temp);
   }
    
    return nullptr;
}

// Returns an object to the free list for the client (simulates delete)
// Throws an exception if the the object can't be freed. (Invalid object)
void ObjectAllocator::Free(void* Object)
{
    try
    {
        //if()
        //{
        //
        //}
        memset(Object, FREED_PATTERN, stats_.ObjectSize_);
        stats_.FreeObjects_++;
        stats_.ObjectsInUse_--;
        GenericObject* ptr = reinterpret_cast<GenericObject*>(Object);
        ptr->Next = FreeList_;
        FreeList_ = ptr;
        stats_.Deallocations_++;
    }
    catch (const std::exception&)
    {

    }
    
}

// Calls the callback fn for each block still in use
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const
{
return 0;
}

// Calls the callback fn for each block that is potentially corrupted
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const
{
    return 0;
}

// Frees all empty pages (extra credit)
unsigned ObjectAllocator::FreeEmptyPages(void)
{
    return 0;
}

// Returns true if FreeEmptyPages and alignments are implemented
bool ObjectAllocator::ImplementedExtraCredit(void)
{
    return false;
}

// Testing/Debugging/Statistic methods
void         ObjectAllocator::SetDebugState(bool State)
{
    
}
    // true=enable, false=disable
const void*  ObjectAllocator::GetFreeList(void) const
{
    return FreeList_;
}   // returns a pointer to the internal free list
const void*  ObjectAllocator::GetPageList(void) const
{
    return PageList_;
}  // returns a pointer to the internal page list
OAConfig     ObjectAllocator::GetConfig(void) const
{
    return configuration_;
}    // returns the configuration parameters
OAStats      ObjectAllocator::GetStats(void) const
{
    return stats_;
}    // returns the statistics for the allocator
