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
    if constexpr (std::is_pointer_v<T> || std::is_same_v<T, std::nullptr_t>)
    {
        if (value == nullptr)
        {
            return NPtr;
        }
    }

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
    else if constexpr (std::is_same_v<T, pybind11::int_> || std::is_same_v<T, pybind11::float_> || std::is_same_v<T, pybind11::bool_> || std::is_same_v<T, pybind11::str> || std::is_same_v<T, pybind11::bytes>)
    {
        return ShmemPrimitive_::construct(value, heapPtr);
    }
    else if constexpr (std::is_same_v<T, pybind11::list>)
    {   // There are two cases: pure int/float/bool, or mixed/nested
        // Case 1: construct Primitive
        // Case 2: construct List
        bool allInt = true;
        bool allFloat = true;
        bool allBool = true;
        for (const auto &item : value)
        {
            if (!pybind11::isinstance<pybind11::int_>(item))
            {
                allInt = false;
            }
            if (!pybind11::isinstance<pybind11::float_>(item))
            {
                allFloat = false;
            }
            if (!pybind11::isinstance<pybind11::bool_>(item))
            {
                allBool = false;
            }
            if (!(allInt || allFloat || allBool))
            {
                break;
            }
        }
        if (allInt || allFloat || allBool)
        {
            return ShmemPrimitive_::construct(value, heapPtr);
        }
        else
        {
            return ShmemList::construct(value, heapPtr);
        }
    }
    else if constexpr (std::is_same_v<T, pybind11::object>)
    { // Check underlying type
        try
        {
#define CONSTRUCT_WITH_UNDERLYING_TYPE(TYPE, VALUE) \
    return construct(VALUE, heapPtr);
            SWITCH_PYTHON_OBJECT_TO_PRIMITIVE(value, CONSTRUCT_WITH_UNDERLYING_TYPE);
#undef CONSTRUCT_WITH_UNDERLYING_TYPE
        }
        catch (std::runtime_error &e)
        { // Cannot construct to Primitive, fall back to list/dict
            if (pybind11::isinstance<pybind11::list>(value))
            {
                return ShmemList::construct(value, heapPtr);
            }
            else if (pybind11::isinstance<pybind11::dict>(value))
            {
                return ShmemDict::construct(value, heapPtr);
            }
            else
            {
                throw std::runtime_error("Cannot construct object of type " + typeName<T>());
            }
        }
    }
    else
    {
        throw std::runtime_error("Cannot construct object of type " + typeName<T>());
    }
}

// Converters
template <typename T>
ShmemObj::operator T() const
{
    if (isPrimitive(this->type))
    {
        return reinterpret_cast<const ShmemPrimitive_ *>(this)->operator T();
    }
    else if (this->type == List)
    {
        return reinterpret_cast<const ShmemList *>(this)->operator T();
    }
    else if (this->type == Dict)
    {
        return reinterpret_cast<const ShmemDict *>(this)->operator T();
    }
    else
    {
        throw ConversionError("Cannot convert " + typeNames.at(this->type) + "[" + std::to_string(this->size) + "]" + " to " + typeName<T>());
    }
}

// Arithmetic
template <typename T>
bool ShmemObj::operator==(const T &val) const
{
    if constexpr (std::is_same_v<T, nullptr_t>)
    {
        return this == nullptr;
    }
    else if constexpr (isObjPtr<T>::value)
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

inline void ShmemObj::acquire(int timeout)
{
    this->wait(timeout);
    this->setBusy(true);
}

inline void ShmemObj::release()
{
    this->setBusy(false);
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
