#include "ShmemObj.h"

// ShmemList methods
inline size_t ShmemAccessor::ShmemList::listCapacity() const
{
    return this->capacity() / sizeof(ptrdiff_t);
}

size_t ShmemAccessor::ShmemList::construct(size_t listCapacity, ShmemHeap *heapPtr)
{
    size_t offset = heapPtr->shmalloc(sizeof(ShmemList) + listCapacity * sizeof(ptrdiff_t));
    ShmemList *ptr = static_cast<ShmemList *>(resolveOffset(offset, heapPtr));
    ptr->type = List;
    ptr->size = 0;
    // ptr->listCapacity() might be different (bigger) than listCapacity, as it's the capacity
    // of memory block assigned to this list
    memset(ptr->relativeOffsetPtr(), 0, ptr->listCapacity() * sizeof(ptrdiff_t));
    return offset;
}

void ShmemAccessor::ShmemList::deconstruct(size_t offset, ShmemHeap *heapPtr)
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

ptrdiff_t *ShmemAccessor::ShmemList::relativeOffsetPtr()
{
    return reinterpret_cast<ptrdiff_t *>(reinterpret_cast<Byte *>(this) + sizeof(ShmemObj));
}

inline int ShmemAccessor::ShmemList::resolveIndex(int index)
{
    if (index < 0)
    { // negative index is relative to the end
        index += this->size;
    }
    if (index >= this->size || index < 0)
    {
        throw IndexError("List index out of bounds");
    }
    return index;
}

ShmemAccessor::ShmemObj *ShmemAccessor::ShmemList::at(int index)
{
    if (this->type != List)
    {
        throw std::runtime_error("Cannot at on non-list ShmemObj");
    }
    return reinterpret_cast<ShmemObj *>(reinterpret_cast<Byte *>(this) + relativeOffsetPtr()[resolveIndex(index)]);
}

void ShmemAccessor::ShmemList::add(ShmemObj *newObj)
{
    relativeOffsetPtr()[this->size] = reinterpret_cast<Byte *>(newObj) - reinterpret_cast<Byte *>(this);
    this->size++;
}

ShmemAccessor::ShmemObj *ShmemAccessor::ShmemList::replace(int index, ShmemObj *newObj)
{
    ptrdiff_t *basePtr = relativeOffsetPtr();
    int index = resolveIndex(index);
    ShmemObj *originalObj = this->at(index);
    basePtr[index] = reinterpret_cast<Byte *>(newObj) - reinterpret_cast<Byte *>(this);
    return originalObj;
}

ShmemAccessor::ShmemObj *ShmemAccessor::ShmemList::pop(int index)
{
    return replace(index, nullptr);
}

std::string ShmemAccessor::ShmemList::toString(ShmemAccessor::ShmemList *list, int indent, int maxElements)
{
    std::string result;
    std::string indentStr = std::string(indent, ' ');

    result += "[\n";
    for (int i = 0; i < list->size; i++)
    {
        result += ShmemObj::toString(list->at(i), indent + 1) + "\n";
    }
    result += indentStr + "]";

    return result;
}
