#ifndef SHMEM_OBJ_TCC
#define SHMEM_OBJ_TCC

#include "ShmemObj.h"
#include "ShmemPrimitive.h"
#include "ShmemDict.h"
#include "ShmemList.h"

// template part
template <typename T>
size_t ShmemObj::construct(const T &value, ShmemHeap *heapPtr)
{
    if constexpr (isPrimitive<T>())
    {
        return ShmemPrimitive_::construct(value, heapPtr);
    }
    else if constexpr (isString<T>())
    {
        return ShmemPrimitive_::construct(value, heapPtr);
    }
    else if constexpr (isVector<T>::value)
    {
        return ShmemList::construct(value, heapPtr);
    }
    else if constexpr (isMap<T>::value)
    {
        return ShmemDict::construct(value, heapPtr);
    }
    else
    {
        throw std::runtime_error("Cannot construct object of type " + std::string(typeid(T).name()));
    }
}

// inline part

inline size_t ShmemObj::capacity() const
{
    return this->getHeader()->size() - sizeof(ShmemHeap::BlockHeader);
}

inline bool ShmemObj::isBusy() const
{
    return this->getHeader()->B();
}

inline void ShmemObj::setBusy(bool b)
{
    this->getHeader()->setB(b);
}

inline int ShmemObj::wait(int timeout) const
{
    return this->getHeader()->wait(timeout);
}

inline ShmemHeap::BlockHeader *ShmemObj::getHeader() const
{
    return reinterpret_cast<ShmemHeap::BlockHeader *>(reinterpret_cast<uintptr_t>(this) - sizeof(ShmemHeap::BlockHeader));
}

inline ShmemObj *ShmemObj::resolveOffset(size_t offset, ShmemHeap *heapPtr)
{
    return reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + offset);
}

#endif // SHMEM_OBJ_TCC
