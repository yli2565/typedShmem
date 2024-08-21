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
        throw std::runtime_error("Cannot construct object of type " + typeName<T>());
    }
}

// template <typename T>
// size_t ShmemObj::construct(const std::initializer_list<T> &value, ShmemHeap *heapPtr)
// {
//     if constexpr (isVectorInitializerList<std::initializer_list<T>>())
//     {
//         if constexpr (isPrimitiveBaseCase<T>())
//         {
//             return ShmemPrimitive_::construct(value, heapPtr);
//         }
//         else
//         {
//             return ShmemList::construct(value, heapPtr);
//         }
//     }
//     if constexpr (isMapInitializerList<std::initializer_list<T>>())
//     {
//         return ShmemDict::construct(value, heapPtr);
//     }
//     else
//     {
//         throw std::runtime_error("Cannot construct object of type " + typeName<T>());
//     }
// }

// Converters
template <typename T>
ShmemObj::operator T() const
{
    if constexpr (isPrimitive<T>())
    {
        return ShmemPrimitive_::operator T();
    }
    else if constexpr (isString<T>())
    {
        return ShmemPrimitive_::operator T();
    }
    else if constexpr (isVector<T>::value)
    {
        return ShmemList::operator T();
    }
    else if constexpr (isMap<T>::value)
    {
        return ShmemDict::operator T();
    }
    else
    {
        throw std::runtime_error("Cannot convert " + typeNames.at(this->type) + " to type " + typeName<T>());
    }
}

// Arithmetic
template <typename T>
bool ShmemObj::operator==(const T &val) const
{
    if constexpr (isObjPtr<T>::value)
    {
        if (isPrimitive(this->type) && isPrimitive(val->type))
            return reinterpret_cast<const ShmemPrimitive_ *>(this)->operator==(val);
        else if (this->type == List && val->type == List)
            return reinterpret_cast<const ShmemList *>(this)->operator==(val);
        else if (this->type == Dict && val->type == Dict)
            return reinterpret_cast<const ShmemDict *>(this)->operator==(val);
        else
            throw std::runtime_error("Unsupported type comparison: this->type = " + std::to_string(this->type) + "; val->type = " + std::to_string(val->type));
    }
    else if constexpr (isPrimitive<T>())
    {
        return reinterpret_cast<const ShmemPrimitive_ *>(this)->operator==(val);
    }
    else if constexpr (isString<T>())
    {
        return reinterpret_cast<const ShmemPrimitive_ *>(this)->operator==(val);
    }
    else if constexpr (isVector<T>::value)
    { // Already exclude the primitive vector case
        return reinterpret_cast<const ShmemList *>(this)->operator==(val);
    }
    else if constexpr (isMap<T>::value)
    {
        return reinterpret_cast<const ShmemDict *>(this)->operator==(val);
    }
    else
    {
        throw std::runtime_error("Comparison of " + typeNames.at(this->type) + " with " + typeName<T>() + " is not allowed");
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
