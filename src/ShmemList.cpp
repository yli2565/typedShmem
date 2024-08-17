#include "ShmemList.h"

// Protected methods

size_t ShmemList::makeSpace(size_t listCapacity, ShmemHeap *heapPtr)
{
    size_t offset = heapPtr->shmalloc(sizeof(ShmemList));
    size_t listSpaceOffset = makeListSpace(listCapacity, heapPtr);
    ShmemList *ptr = static_cast<ShmemList *>(resolveOffset(offset, heapPtr));

    ptr->type = List;
    ptr->size = listCapacity; // The size in ShmemObj is the capacity, the dynamic size is recorded in listSize

    ptr->listSize = 0;
    ptr->listSpaceOffset = listSpaceOffset - offset; // The offset provided by shmalloc is relative to the heap head, we need to convert it to the offset relative to the list object

    return offset;
}

size_t ShmemList::makeListSpace(size_t listCapacity, ShmemHeap *heapPtr)
{
    size_t listSpaceOffset = heapPtr->shmalloc(listCapacity * sizeof(ptrdiff_t));
    Byte *payloadPtr = heapPtr->heapHead() + listSpaceOffset;
    size_t payLoadSize = reinterpret_cast<ShmemHeap::BlockHeader *>(payloadPtr - sizeof(ShmemHeap::BlockHeader))->size() - sizeof(ShmemHeap::BlockHeader);

    // init the list space with NPtr (would be interpret as nullptr)
    size_t maxListCapacity = payLoadSize / sizeof(ptrdiff_t);
    std::fill(reinterpret_cast<ptrdiff_t *>(payloadPtr), reinterpret_cast<ptrdiff_t *>(payloadPtr) + maxListCapacity, NPtr);

    return listSpaceOffset;
}

// Public methods

size_t ShmemList::construct(size_t capacity, ShmemHeap *heapPtr)
{
    return ShmemList::makeSpace(capacity, heapPtr);
}

void ShmemList::deconstruct(size_t offset, ShmemHeap *heapPtr)
{
    ShmemList *ptr = reinterpret_cast<ShmemList *>(resolveOffset(offset, heapPtr));
    Byte *heapHead = heapPtr->heapHead();
    for (int i = 0; i < ptr->size; i++)
    {
        ShmemObj *obj = ptr->at(i);
        ShmemObj::deconstruct(reinterpret_cast<Byte *>(obj) - heapHead, heapPtr);
    }
    heapPtr->shfree(ptr);
}

ShmemObj *ShmemList::at(int index)
{
    return getObj(index);
}

void ShmemList::add(ShmemObj *newObj, ShmemHeap *heapPtr)
{
    if (this->listSize >= this->listCapacity())
    {
        // TODO: extend the list
        throw std::runtime_error("List is full");
    }
    relativeOffsetPtr()[this->listSize] = reinterpret_cast<Byte *>(newObj) - reinterpret_cast<Byte *>(this);
    this->listSize++;
}

void ShmemList::assign(int index, ShmemObj *newObj, ShmemHeap *heapPtr)
{
    ptrdiff_t *basePtr = relativeOffsetPtr();
    int resolvedIndex = resolveIndex(index);
    ShmemObj *originalObj = this->getObj(resolvedIndex);
    if (originalObj != nullptr)
    {
        ShmemObj::deconstruct(reinterpret_cast<Byte *>(originalObj) - reinterpret_cast<Byte *>(this), heapPtr);
    }
    setObj(resolvedIndex, newObj);
}

ShmemObj *ShmemList::pop()
{
    ShmemObj *originalObj = this->getObj(this->listSize);
    setObj(this->listSize, nullptr);
    return originalObj;
}

std::string ShmemList::toString(ShmemList *list, int indent, int maxElements)
{
    std::string result;
    std::string indentStr = std::string(indent, ' ');

    result += "[\n";
    for (int i = 0; i < std::min(static_cast<int>(list->listSize), maxElements); i++)
    {
        result += ShmemObj::toString(list->getObj(i), indent + 1) + "\n";
    }
    if (list->listSize > maxElements)
    {
        result += indentStr + "...\n";
    }
    result += indentStr + "]";

    return result;
}
