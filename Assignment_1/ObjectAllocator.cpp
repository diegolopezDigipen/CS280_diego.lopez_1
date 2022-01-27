#include "ObjectAllocator.h"
#include "string.h"


// Creates the ObjectManager per the specified values
    // Throws an exception if the construction fails. (Memory allocation problem)
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config)
{
    
    configuration_ = config;
    unsigned pdBytes = configuration_.PadBytes_;
    size_t hdBytes = configuration_.HBlockInfo_.size_;
    stats_.ObjectSize_ = ObjectSize;
    stats_.PageSize_ = configuration_.ObjectsPerPage_ * ObjectSize + sizeof(char*) + 2 * configuration_.ObjectsPerPage_ * pdBytes + hdBytes * configuration_.ObjectsPerPage_;
    stats_.FreeObjects_ = 0;
    stats_.ObjectsInUse_ = 0;
    stats_.PagesInUse_ = 0;
    stats_.MostObjects_ = 0;
    stats_.Allocations_ = 0;
    stats_.Deallocations_ = 0;
    PageList_ = nullptr;
    FreeList_ = nullptr;

    CreatePage(FreeList_, PageList_);
}

void ObjectAllocator::CreatePage( GenericObject*& FreeList_, GenericObject*& PageList_)
{
    unsigned pdBytes = configuration_.PadBytes_;
    size_t hdBytes = configuration_.HBlockInfo_.size_;
    size_t ObjectSize = stats_.ObjectSize_;
    char* Block;
    //Allocate
    try {
        Block = new char[stats_.PageSize_];
    }
    catch (const std::exception&) { throw OAException(OAException::E_NO_MEMORY, "There is no memory, error when using new"); }


     if(hdBytes)
     {
         memset(Block + sizeof(GenericObject*), 0, hdBytes);
     }

    if (pdBytes)
    {
        memset(Block + sizeof(GenericObject*) + hdBytes, PAD_PATTERN, pdBytes);
        memset(Block + sizeof(GenericObject*) + hdBytes + pdBytes + ObjectSize, PAD_PATTERN, pdBytes);
    }

    memset(Block + sizeof(GenericObject*) + pdBytes + hdBytes + sizeof(void*) , UNALLOCATED_PATTERN, ObjectSize - sizeof(void*));

    GenericObject* previous = PageList_;
    PageList_ = reinterpret_cast<GenericObject*>(Block);
    PageList_->Next = previous;

    previous = FreeList_;
    FreeList_ = reinterpret_cast<GenericObject*>(reinterpret_cast<char*>((PageList_ + 1)) + pdBytes + hdBytes);
    FreeList_->Next = previous;
    char* other = Block + sizeof(char*);


    for (unsigned int i = 1; i < configuration_.ObjectsPerPage_; i++)
    {
        GenericObject* prev = FreeList_;
        FreeList_ = reinterpret_cast<GenericObject*>(other + pdBytes + hdBytes + i * (ObjectSize + 2 * pdBytes + hdBytes));
        FreeList_->Next = prev;
        GenericObject* temp = FreeList_;
        memset(reinterpret_cast<char*>(temp)+sizeof(void*), UNALLOCATED_PATTERN, ObjectSize-sizeof(void*));   
        

        //Headers
        if(hdBytes && configuration_.DebugOn_)
            memset(other + i * (ObjectSize + 2 * pdBytes + hdBytes), 0, hdBytes);

        //Pad Pattern
        if (pdBytes && configuration_.DebugOn_)
        {
            memset(other + i * (ObjectSize + 2 * pdBytes + hdBytes) + hdBytes, PAD_PATTERN, pdBytes);
            memset(other + i * (ObjectSize + 2 * pdBytes + hdBytes) + hdBytes + ObjectSize + pdBytes, PAD_PATTERN, pdBytes);
        }    
    }

    stats_.FreeObjects_ += configuration_.ObjectsPerPage_;
    stats_.PagesInUse_++;
}

// Destroys the ObjectManager (never throws)
ObjectAllocator::~ObjectAllocator()
{
    unsigned pdBytes = configuration_.PadBytes_;
    size_t hdBytes = configuration_.HBlockInfo_.size_;

    //Check if headers need to be deallocated
    if (configuration_.HBlockInfo_.type_ == OAConfig::hbExternal)
    {      
        //Check through all the lists
        while (PageList_)
        {
            char* pList = reinterpret_cast<char*>(PageList_) + sizeof(void*);
            unsigned OPP = configuration_.ObjectsPerPage_;
            for (unsigned i = 0; i < OPP; i++)
            {
                char* header = pList + i * (stats_.ObjectSize_ + 2 * pdBytes + hdBytes);
                MemBlockInfo** headerPtr = reinterpret_cast<MemBlockInfo**>(header);
                if (*headerPtr)
                {
                    delete[](*headerPtr)->label;
                    (*headerPtr)->label = nullptr;
                    delete *headerPtr;
                    (*headerPtr) = nullptr;
                }
            }
            GenericObject* temp = PageList_;
            PageList_ = PageList_->Next;
            delete[] temp;
        }
    }
    else
    {
        while (PageList_)
        {
            GenericObject* temp = PageList_;
            PageList_ = PageList_->Next;
            delete[] temp;
        }
    }
}
// Take an object from the free list and give it to the client (simulates new)
// Throws an exception if the object can't be allocated. (Memory allocation problem)
void* ObjectAllocator::Allocate(const char* label)
{  
    bool State = configuration_.DebugOn_;
    if (configuration_.UseCPPMemManager_)
    {
        char* ptr = new char[stats_.ObjectSize_];
        //Stats
        stats_.ObjectsInUse_++;
        if (stats_.ObjectsInUse_ > stats_.MostObjects_)
        {
            stats_.MostObjects_ = stats_.ObjectsInUse_;
        }
        stats_.Allocations_++;
        return static_cast<void*>(ptr);
    }
    else
    {
        unsigned pdBytes = configuration_.PadBytes_;
        size_t hdBytes = configuration_.HBlockInfo_.size_;
        if (stats_.FreeObjects_ == 0)
        {
            if (stats_.PagesInUse_ != configuration_.MaxPages_)
            {
                CreatePage(FreeList_, PageList_);
            }
            else
            {
                throw OAException(OAException::E_NO_PAGES, "Couldnt allocate, max number of ages reached coulndt allocate more space");
            }
        }
        if (stats_.FreeObjects_ != 0)
        {
            GenericObject* temp = FreeList_;

            FreeList_ = FreeList_->Next;
            if(State)
                memset(temp, 0xBB, stats_.ObjectSize_);
            stats_.FreeObjects_--;
            stats_.ObjectsInUse_++;
            if (stats_.ObjectsInUse_ > stats_.MostObjects_)
            {
                stats_.MostObjects_ = stats_.ObjectsInUse_;
            }
            stats_.Allocations_++;

            //Header
            unsigned int* ptr;
            short* counter;
            char* header = reinterpret_cast<char*>(temp) - pdBytes - hdBytes;
            if (configuration_.HBlockInfo_.type_ != OAConfig::hbNone)
            {
                if (configuration_.HBlockInfo_.type_ == OAConfig::hbBasic && State)
                {
                    ptr = reinterpret_cast<unsigned int*>(header);
                    *ptr = stats_.Allocations_;
                    *(header + 4) = 1;
                }
                else if (configuration_.HBlockInfo_.type_ == OAConfig::hbExtended && State)
                {
                    counter = reinterpret_cast<short*>(header + 1);
                    (*counter)++;
                    ptr = reinterpret_cast<unsigned int*>(header + 3);
                    *ptr = stats_.Allocations_;
                    *(header + 7) = 1;
                }
                else if (configuration_.HBlockInfo_.type_ == OAConfig::hbExternal)
                {
                    MemBlockInfo** headerPtr = reinterpret_cast<MemBlockInfo**>(header);
                    *(headerPtr) = new MemBlockInfo();
                    (*headerPtr)->in_use = 1;
                    (*headerPtr)->alloc_num = stats_.Allocations_;
                    if (label)
                    {
                        (*headerPtr)->label = new char[strlen(label) + 1];
                        strcpy((*headerPtr)->label, label);
                    }
                    else
                    {
                        (*headerPtr)->label = nullptr;
                    }
                }
            }
            return reinterpret_cast<void*>(temp);
        }
    }
    return nullptr;
}

// Returns an object to the free list for the client (simulates delete)
// Throws an exception if the the object can't be freed. (Invalid object)
void ObjectAllocator::Free(void* Object)
{
    bool State = configuration_.DebugOn_;
    if (configuration_.UseCPPMemManager_)
    {
        delete [] reinterpret_cast<char*>(Object);
        //Stats
        stats_.FreeObjects_++;
        stats_.ObjectsInUse_--;
        stats_.Deallocations_++;
    }
    else
    {
        unsigned pdBytes = configuration_.PadBytes_;
        size_t hdBytes = configuration_.HBlockInfo_.size_;

        GenericObject* other = PageList_;
        bool found = false;

        if (State)
        {
            //look thorough the pages for boundary check
            while (other)
            {
                GenericObject* temp = other;
                if (Object > temp && Object < reinterpret_cast<char*>(temp) + stats_.PageSize_)
                {
                    char* PageTemp = reinterpret_cast<char*>(other);
                    size_t firstOBJ = sizeof(void*) + hdBytes + pdBytes;
                    size_t offset = (stats_.ObjectSize_ + 2 * pdBytes + hdBytes);
                    auto position = reinterpret_cast<char*>(Object) - PageTemp - firstOBJ;
                    if (position % offset == 0)
                    {
                        found = true;
                        break;
                    }
                    else
                    {
                        found = false;
                        break;
                    }
                }
                other = other->Next;
                if (found)
                    break;
            }
            if (!found)
                throw OAException(OAException::E_BAD_BOUNDARY, "Bad boundary Free");

            if (hdBytes)
            {
                if (configuration_.HBlockInfo_.type_ == OAConfig::hbBasic || configuration_.HBlockInfo_.type_ == OAConfig::hbExtended)
                {
                    if (*(reinterpret_cast<char*>(Object) - pdBytes - 1) == 0)
                        throw OAException(OAException::E_MULTIPLE_FREE, "Multiple Free");
                }
                else if (configuration_.HBlockInfo_.type_ == OAConfig::hbExternal)
                {
                    MemBlockInfo** save = reinterpret_cast<MemBlockInfo**>(reinterpret_cast<char*>(Object) - pdBytes - hdBytes);
                    if (*save)
                    {
                        if (!(*save)->in_use)
                            throw OAException(OAException::E_MULTIPLE_FREE, "Multiple Free");
                    }
                }
            }
            else
            {
                //Check double Free
                GenericObject* temp = FreeList_;
                for (unsigned int i = 0; i < stats_.FreeObjects_; i++)
                {
                    if (Object == temp)
                        throw OAException(OAException::E_MULTIPLE_FREE, "Multiple Free");
                    temp = temp->Next;
                }
            }

            unsigned char* ptr1 = reinterpret_cast<unsigned char*>(Object) - pdBytes;
            unsigned char* ptr2 = ptr1 + stats_.ObjectSize_ + pdBytes;
            for (size_t i = 0; i < pdBytes; i++)
            {
                if (ptr1[i] != PAD_PATTERN)
                {
                    throw OAException(OAException::E_CORRUPTED_BLOCK, "Corrupted");
                }
                if (ptr2[i] != PAD_PATTERN)
                {
                    throw OAException(OAException::E_CORRUPTED_BLOCK, "Corrupted");
                }
            }

            memset(reinterpret_cast<char*>(Object), FREED_PATTERN, stats_.ObjectSize_);
        }
        GenericObject* ptr = reinterpret_cast<GenericObject*>(Object);
        ptr->Next = FreeList_;
        FreeList_ = ptr;

        //Stats
        stats_.FreeObjects_++;
        stats_.ObjectsInUse_--;
        stats_.Deallocations_++;

        //Header

        char* header = reinterpret_cast<char*>(Object) - pdBytes - hdBytes;
        if (configuration_.HBlockInfo_.type_ != OAConfig::hbNone)
        {
            if (configuration_.HBlockInfo_.type_ == OAConfig::hbBasic && State)
            {

                *reinterpret_cast<unsigned int*>(header) = 0;
                *(header + 4) = 0;
            }
            else if (configuration_.HBlockInfo_.type_ == OAConfig::hbExtended && State)
            {
                *reinterpret_cast<unsigned int*>(header + 3) = 0;
                *(header + 7) = 0;
            }
            else if (configuration_.HBlockInfo_.type_ == OAConfig::hbExternal)
            {
                MemBlockInfo** headerPtr = reinterpret_cast<MemBlockInfo**>(header);
                if ((*headerPtr))
                {
                    if ((*headerPtr)->label)
                    {
                        delete[](*headerPtr)->label;
                        (*headerPtr)->label = nullptr;
                    }
                    delete* headerPtr;
                    *headerPtr = nullptr;
                    headerPtr = nullptr;
                }
            }

        }
    }
}

// Calls the callback fn for each block still in use
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const
{
    unsigned count = 0;
    GenericObject* temp = PageList_;
    size_t pdBytes = configuration_.PadBytes_;
    size_t hdBytes = configuration_.HBlockInfo_.size_;
    while(temp)
    {
        char* itr = reinterpret_cast<char*>(temp);
        for (unsigned int i = 0; i < configuration_.ObjectsPerPage_; i++)
        {
            //Take object
            GenericObject* object = reinterpret_cast<GenericObject*>(itr + sizeof(void*) + hdBytes + pdBytes + i * (stats_.ObjectSize_ + 2 * pdBytes + hdBytes));

            //Is it in use
            GenericObject* temp1 = FreeList_;
            bool in_use = true;
            for (unsigned int i = 0; i < stats_.FreeObjects_; i++)
            {
                if (object == temp1)
                {
                    in_use = false;
                    break;
                }
                temp1 = temp1->Next;
            }
            if (in_use)
            {
                count++;
                fn(object, stats_.ObjectSize_);
            }
        }
        temp = temp->Next;
    }
return count;
}

// Calls the callback fn for each block that is potentially corrupted
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const
{
    if (!configuration_.DebugOn_ || !configuration_.PadBytes_)
        return 0;
    unsigned count= 0;
    unsigned pdBytes = configuration_.PadBytes_;
    size_t hdBytes = configuration_.HBlockInfo_.size_;
    //Have to check through the pad bites to see if any of them where corrupted
    GenericObject* other = PageList_;
    //look thorough the pages for boundary check
    while (other)
    {
        unsigned char* temp = reinterpret_cast<unsigned char*>(other);
        for (unsigned int i = 0; i < configuration_.ObjectsPerPage_; i++)
        {
            unsigned char* ptr1 = temp + sizeof(void*) + hdBytes + i * (stats_.ObjectSize_ + 2 * pdBytes + hdBytes);
            unsigned char* ptr2 = ptr1 + stats_.ObjectSize_ + pdBytes;
            for (size_t i = 0; i < pdBytes; i++)
            {
                if (ptr1[i] != PAD_PATTERN)
                {
                    fn(ptr1 + pdBytes, stats_.ObjectSize_);
                    count++;
                    break;
                }
                if (ptr2[i] != PAD_PATTERN)
                {
                    fn(ptr1 + pdBytes, stats_.ObjectSize_);
                    count++;
                    break;
                }
            }
            
        }
        other = other->Next;
    }
    return count;
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
    configuration_.DebugOn_ = State;
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
