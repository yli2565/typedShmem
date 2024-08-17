#ifndef SHMEM_LIST_TCC
#define SHMEM_LIST_TCC

#include "ShmemObj.h"
#include "ShmemList.h"

template <typename T>
size_t ShmemList::construct(std::vector<T> vec, ShmemHeap *heapPtr)
{
    using vecDataType = typename unwrapVectorType<T>::type;
    if constexpr (isPrimitive<vecDataType>() && !isVector<vecDataType>::value)
    {
        throw std::runtime_error("Not a good idea to construct a list for an array of primitives");
    }

    size_t listOffset = ShmemList::makeSpace(vec.capacity(), heapPtr);
    ShmemList *list = reinterpret_cast<ShmemList *>(ShmemObj::resolveOffset(listOffset, heapPtr));
    for (int i = 0; i < static_cast<int>(vec.size()); i++)
    {
        list->add(vec[i], heapPtr);
    }
    return listOffset;
}

// Inline implementations

// Arithmetics
inline ptrdiff_t *ShmemList::relativeOffsetPtr()
{
    return reinterpret_cast<ptrdiff_t *>(reinterpret_cast<Byte *>(this) + this->listSpaceOffset);
}

inline const ptrdiff_t *ShmemList::relativeOffsetPtr() const
{
    return reinterpret_cast<const ptrdiff_t *>(reinterpret_cast<const Byte *>(this) + this->listSpaceOffset);
}

inline int ShmemList::resolveIndex(int index) const
{
    // Adjust negative index and use modulo to handle out-of-bounds
    index = (index < 0) ? index + this->listSize : index;

    if (static_cast<unsigned int>(index) >= this->listSize)
    {
        throw IndexError("List index out of bounds");
    }

    return index;
}

// Public methods
// Accessors
inline size_t ShmemList::listCapacity() const
{
    return this->ShmemObj::size;
}

inline size_t ShmemList::potentialCapacity() const
{
    return (reinterpret_cast<const ShmemHeap::BlockHeader *>(reinterpret_cast<const Byte *>(this->relativeOffsetPtr()) - sizeof(ShmemHeap::BlockHeader))->size() - sizeof(ShmemHeap::BlockHeader)) / sizeof(ptrdiff_t);
}
#endif // SHMEM_LIST_TCC
