#include "ShmemObj.h"

// Protected methods
ShmemAccessor::ShmemObj *ShmemAccessor::ShmemList::getObj(int index) const
{
    ptrdiff_t offset = relativeOffsetPtr()[resolveIndex(index)];
    if (offset == NPtr)
        return nullptr;
    return const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + offset));
}

void ShmemAccessor::ShmemList::setObj(int index, ShmemObj *obj)
{
    if (obj == nullptr)
        relativeOffsetPtr()[resolveIndex(index)] = NPtr;
    else
        relativeOffsetPtr()[resolveIndex(index)] = reinterpret_cast<Byte *>(obj) - reinterpret_cast<Byte *>(this);
}

size_t ShmemAccessor::ShmemList::makeSpace(size_t listCapacity, ShmemHeap *heapPtr)
{
    size_t offset = heapPtr->shmalloc(sizeof(ShmemList));
    size_t listSpaceOffset = heapPtr->shmalloc(listCapacity * sizeof(ptrdiff_t));
    ShmemList *ptr = static_cast<ShmemList *>(resolveOffset(offset, heapPtr));

    ptr->type = List;
    ptr->size = listCapacity; // The size in ShmemObj is the capacity, the dynamic size is recorded in listSize

    ptr->listSize = 0;
    ptr->listSpaceOffset = listSpaceOffset - offset; // The offset provided by shmalloc is relative to the heap head, we need to convert it to the offset relative to the list object

    // init the list space
    ptrdiff_t *basePtr = ptr->relativeOffsetPtr();
    std::fill(basePtr, basePtr + listCapacity, NPtr);
    return offset;
}

// ShmemList methods
inline size_t ShmemAccessor::ShmemList::listCapacity() const
{
    return this->ShmemObj::size;
}

inline size_t ShmemAccessor::ShmemList::potentialCapacity() const
{
    return (reinterpret_cast<const ShmemHeap::BlockHeader *>(reinterpret_cast<const Byte *>(this->relativeOffsetPtr()) - sizeof(ShmemHeap::BlockHeader))->size() - sizeof(ShmemHeap::BlockHeader)) / sizeof(ptrdiff_t);
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

inline ptrdiff_t *ShmemAccessor::ShmemList::relativeOffsetPtr()
{
    return reinterpret_cast<ptrdiff_t *>(reinterpret_cast<Byte *>(this) + this->listSpaceOffset);
}

inline const ptrdiff_t *ShmemAccessor::ShmemList::relativeOffsetPtr() const
{
    return reinterpret_cast<const ptrdiff_t *>(reinterpret_cast<const Byte *>(this) + this->listSpaceOffset);
}

inline int ShmemAccessor::ShmemList::resolveIndex(int index) const
{
    if (index < 0)
    { // negative index is relative to the end
        index += this->listSize;
    }
    if (index >= this->listSize || index < 0)
    {
        throw IndexError("List index out of bounds");
    }
    return index;
}

ShmemAccessor::ShmemObj *ShmemAccessor::ShmemList::at(int index)
{
    return getObj(index);
}

void ShmemAccessor::ShmemList::add(ShmemObj *newObj, ShmemHeap *heapPtr)
{
    if (this->listSize >= this->listCapacity())
    {
        // TODO: extend the list
        throw std::runtime_error("List is full");
    }
    relativeOffsetPtr()[this->listSize] = reinterpret_cast<Byte *>(newObj) - reinterpret_cast<Byte *>(this);
    this->listSize++;
}

void ShmemAccessor::ShmemList::assign(int index, ShmemObj *newObj, ShmemHeap *heapPtr)
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

ShmemAccessor::ShmemObj *ShmemAccessor::ShmemList::pop()
{
    ShmemObj *originalObj = this->getObj(this->listSize);
    setObj(this->listSize, nullptr);
    return originalObj;
}

std::string ShmemAccessor::ShmemList::toString(ShmemAccessor::ShmemList *list, int indent, int maxElements)
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
