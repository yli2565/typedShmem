#include "ShmemList.h"

// Protected methods

// Core methods
ShmemObj *ShmemList::getObj(int index) const
{
    ptrdiff_t offset = relativeOffsetPtr()[resolveIndex(index)];
    if (offset == NPtr)
        return nullptr;
    return const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + offset));
}

void ShmemList::setObj(int index, ShmemObj *obj)
{
    if (obj == nullptr)
        relativeOffsetPtr()[resolveIndex(index)] = NPtr;
    else
        relativeOffsetPtr()[resolveIndex(index)] = reinterpret_cast<Byte *>(obj) - reinterpret_cast<Byte *>(this);
}

// relativeOffsetPtr() inlined in tcc

// resolveIndex(int index) inlined in tcc

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

// listCapacity() inlined in tcc

// potentialCapacity() inlined in tcc

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
        ShmemObj *obj = ptr->getObj(i);
        ShmemObj::deconstruct(reinterpret_cast<Byte *>(obj) - heapHead, heapPtr);
    }
    heapPtr->shfree(ptr);
}

size_t ShmemList::len() const
{
    return this->listSize;
}

void ShmemList::del(int index, ShmemHeap *heapPtr)
{
    ptrdiff_t *basePtr = relativeOffsetPtr();
    int resolvedIndex = resolveIndex(index);

    ptrdiff_t offset = basePtr[resolvedIndex];

    if (offset != NPtr)
    {
        ShmemObj *victim = const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + offset));
        ShmemObj::deconstruct(reinterpret_cast<Byte *>(victim) - heapPtr->heapHead(), heapPtr);
    }

    for (int i = resolvedIndex; i < this->listSize - 1; i++)
    {
        basePtr[i] = basePtr[i + 1];
    }
    basePtr[this->listSize - 1] = NPtr;

    this->listSize--;
}

std::string ShmemList::toString(int indent, int maxElements) const
{
    std::string result;
    std::string indentStr = std::string(indent, ' ');

    result += "[\n";
    for (int i = 0; i < std::min(static_cast<int>(this->listSize), maxElements); i++)
    {
        result += ShmemObj::toString(this->getObj(i), indent + 1) + "\n";
    }
    if (this->listSize > maxElements)
    {
        result += indentStr + "...\n";
    }
    result += indentStr + "]";

    return result;
}

void ShmemList::resize(int newSize, ShmemHeap *heapPtr)
{
    if (potentialCapacity() >= newSize)
        return; // No need to resize

    uintptr_t oldListSpaceOffset = reinterpret_cast<Byte *>(this) - heapPtr->heapHead() + this->listSpaceOffset;
    size_t newListSpaceOffset = heapPtr->shrealloc(oldListSpaceOffset, newSize * sizeof(ptrdiff_t));

    this->listSpaceOffset += newListSpaceOffset - oldListSpaceOffset;
}

ShmemList *ShmemList::remove(int index, ShmemHeap *heapPtr)
{
    del(index, heapPtr);
}

// pop() implemented in tcc

ShmemList *ShmemList::clear(ShmemHeap *heapPtr)
{
    ptrdiff_t *basePtr = relativeOffsetPtr();

    for (int i = 0; i < this->listSize; i++)
    {
        if (basePtr[i] != NPtr)
        {
            ShmemObj *victim = const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + basePtr[i]));
            ShmemObj::deconstruct(reinterpret_cast<Byte *>(victim) - heapPtr->heapHead(), heapPtr);
        }
        basePtr[i] = NPtr;
    }

    this->listSize = 0;
}
