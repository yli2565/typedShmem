#ifndef SHMEM_LIST_TCC
#define SHMEM_LIST_TCC

#include "ShmemObj.h"
#include "ShmemList.h"

// Inline implementations
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

inline size_t ShmemList::listCapacity() const
{
    return this->ShmemObj::size;
}

inline size_t ShmemList::potentialCapacity() const
{
    return (reinterpret_cast<const ShmemHeap::BlockHeader *>(reinterpret_cast<const Byte *>(this->relativeOffsetPtr()) - sizeof(ShmemHeap::BlockHeader))->size() - sizeof(ShmemHeap::BlockHeader)) / sizeof(ptrdiff_t);
}

template <typename T>
size_t ShmemList::construct(std::vector<T> vec, ShmemHeap *heapPtr)
{
    if constexpr (isPrimitive<T>() && !isVector<T>::value)
    {
        throw std::runtime_error("Not a good idea to construct a list for an array of primitives");
    }

    size_t listOffset = ShmemList::makeSpace(vec.capacity(), heapPtr);
    ShmemList *list = reinterpret_cast<ShmemList *>(ShmemObj::resolveOffset(listOffset, heapPtr));
    for (int i = 0; i < static_cast<int>(vec.size()); i++)
    {
        list->append(vec[i], heapPtr);
    }
    return listOffset;
}

template <typename T>
T ShmemList::get(int index) const
{
    return getObj(resolveIndex(index))->operator T();
}

template <typename T>
void ShmemList::set(const T &val, int index, ShmemHeap *heapPtr)
{
    ptrdiff_t *basePtr = relativeOffsetPtr();
    int resolvedIndex = resolveIndex(index);

    ptrdiff_t offset = basePtr[resolvedIndex];

    if (offset != NPtr)
    {
        ShmemObj *victim = const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + offset));
        ShmemObj::deconstruct(reinterpret_cast<Byte *>(victim) - heapPtr->heapHead(), heapPtr);
    }

    basePtr[resolvedIndex] = (heapPtr->heapHead() + ShmemObj::construct(val, heapPtr)) - reinterpret_cast<Byte *>(this);
}

// del() implemented in ShmemList.cpp

template <typename T>
bool ShmemList::contains(T value) const
{
    const ptrdiff_t *basePtr = relativeOffsetPtr();

    for (int i = 0; i < this->listSize; i++)
    {
        ptrdiff_t offset = basePtr[i];
        if (offset != NPtr)
        {
            ShmemObj *obj = const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + offset));
            try
            {
                if (obj->operator==(value))
                    return true;
            }
            catch (ConversionError &e)
            {
                continue;
            }
        }
    }
    return false;
}

template <typename T>
int ShmemList::index(T value, int start, int end) const
{
    ptrdiff_t *basePtr = relativeOffsetPtr();

    int trueStart = resolveIndex(start);
    int trueEnd = resolveIndex(end);

    for (int i = trueStart; i < trueEnd; i++)
    {
        ptrdiff_t offset = basePtr[i];
        if (offset != NPtr)
        {
            ShmemObj *obj = const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + offset));
            if (obj->operator==(value))
            {
                return i;
            }
        }
    }
    return -1;
}

template <typename T>
ShmemList *ShmemList::append(const T &val, ShmemHeap *heapPtr)
{
    if (this->listSize >= this->listCapacity())
        this->resize(this->listSize * 2, heapPtr);

    ptrdiff_t *basePtr = relativeOffsetPtr();

    basePtr[this->listSize] = (heapPtr->heapHead() + ShmemObj::construct(val, heapPtr)) - reinterpret_cast<Byte *>(this);

    this->listSize++;
    return this;
}

template <typename T>
ShmemList *ShmemList::extend(const ShmemList *another, ShmemHeap *heapPtr)
{
    if (another->type != List)
    {
        throw std::runtime_error("Can only extend a list with another list");
    }

    resize(this->potentialCapacity() + another->potentialCapacity(), heapPtr);

    ptrdiff_t *basePtrEnd = relativeOffsetPtr() + this->listSize;

    std::copy(another->relativeOffsetPtr(), another->relativeOffsetPtr() + another->listSize, basePtrEnd);

    this->listSize += another->listSize;
    return this;
}

template <typename T>
ShmemList *ShmemList::insert(int index, const T &val, ShmemHeap *heapPtr)
{
    if (this->listSize >= this->listCapacity())
        this->resize(this->listSize * 2, heapPtr);

    ptrdiff_t *basePtr = relativeOffsetPtr();
    int resolvedIndex = resolveIndex(index);

    for (int i = this->listSize; i > resolvedIndex; i--)
    {
        basePtr[i] = basePtr[i - 1];
    }

    basePtr[resolvedIndex] = (heapPtr->heapHead() + ShmemObj::construct(val, heapPtr)) - reinterpret_cast<Byte *>(this);

    this->listSize++;
    return this;
}

// remove() implemented in ShmemList.cpp

template <typename T>
T ShmemList::pop(int index, ShmemHeap *heapPtr)
{

    T result;

    ptrdiff_t *basePtr = relativeOffsetPtr();
    int resolvedIndex = resolveIndex(index);

    ptrdiff_t offset = basePtr[resolvedIndex];

    if (offset != NPtr)
    {
        ShmemObj *victim = const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + offset));
        result = victim->operator T();
        ShmemObj::deconstruct(reinterpret_cast<Byte *>(victim) - heapPtr->heapHead(), heapPtr);
    }

    this->listSize--;

    return result;
}

// Converters
template <typename T>
ShmemList::operator T() const
{
    const ptrdiff_t *basePtr = relativeOffsetPtr();
    if constexpr (isPrimitive<T>())
    {
        throw ConversionError("Cannot convert list to primitive");
    }
    else if constexpr (isVector<T>::value)
    {
        using vecDataType = typename unwrapVectorType<T>::type;
        T result;
        result.reserve(this->listSize);

        for (int i = 0; i < this->listSize; i++)
        {
            ShmemObj *target = const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + basePtr[i]));
            result.push_back(target->operator vecDataType());
        }

        return result;
    }
    if constexpr (std::is_same_v<T, pybind11::list>)
    {
        pybind11::list result;
        for (int i = 0; i < this->listSize; i++)
        {
            ShmemObj *target = const_cast<ShmemObj *>(reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + basePtr[i]));
            result.append(target->operator pybind11::list());
        }
        return result;
    }
    else
    {
        throw ConversionError("Cannot convert the list to " + typeName<T>() + ". List: " + this->toString());
    }
}

template <typename T>
T ShmemList::operator[](int index) const
{
    return this->get<T>(index);
}

// Arithmetic
template <typename T>
bool ShmemList::operator==(const T &val) const
{
    if constexpr (isObjPtr<T>::value)
    {
        if (val->type != this->type)
            return false;
        // Ensure T = const ShmemList*

        if (this->size != val->size)
            return false;

        return this->toString() == reinterpret_cast<const ShmemList *>(val)->toString();
    }
    else if constexpr (isVector<T>::value)
    {
        return this->operator T() == val;
    }
    else if constexpr (std::is_same_v<T, pybind11::list>)
    {
        return this->operator pybind11::list() == val;
    }
    else
    {
        throw std::runtime_error("Comparison of " + typeNames.at(this->type) + " with " + typeName<T>() + " is not allowed");
    }
}
// Non-template function implementations are in ShmemList.cpp

#endif // SHMEM_LIST_TCC
