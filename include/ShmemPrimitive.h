#include "ShmemObj.h"
// Please keep this inclusion before header guard, which make the order of include correct

#ifndef SHMEM_PRIMITIVE_H
#define SHMEM_PRIMITIVE_H

#include "TypeEncodings.h"
#include <cstring>

class ShmemPrimitive_ : public ShmemObj
{
protected:
    template <typename T>
    static size_t makeSpace(size_t size, ShmemHeap *heapPtr);

public:
    static inline void deconstruct(size_t offset, ShmemHeap *heapPtr)
    {
        heapPtr->shfree(heapPtr->heapHead() + offset);
    }

    inline const Byte *getBytePtr() const
    {
        return reinterpret_cast<const Byte *>(reinterpret_cast<const Byte *>(this) + sizeof(ShmemObj));
    }

    inline Byte *getBytePtr()
    {
        return reinterpret_cast<Byte *>(reinterpret_cast<Byte *>(this) + sizeof(ShmemObj));
    }

    int resolveIndex(int index) const;

    static std::string toString(const ShmemPrimitive_ *obj, int indent = 0, int maxElements = 4);

    static const char *toString(const ShmemPrimitive_ *obj, int indent = 0, int maxElements = 4)
    {
        return const_cast<char *>(ShmemPrimitive_::toString(obj, indent, maxElements).c_str());
    }

    std::string toString(int indent = 0, int maxElements = 4) const
    {
        return ShmemPrimitive_::toString(this, indent, maxElements);
    }

    const char *toString(int indent = 0, int maxElements = 4) const
    {
        return ShmemPrimitive_::toString(this, indent, maxElements).c_str();
    }

    static std::string elementToString(const ShmemPrimitive_ *obj, int index);

    static size_t construct(const char *str, ShmemHeap *heapPtr);

    static size_t construct(const std::string str, ShmemHeap *heapPtr);

    template <typename T>
    static size_t construct(T val, ShmemHeap *heapPtr);

    template <typename T>
    static size_t construct(std::vector<T> vec, ShmemHeap *heapPtr);

    template <typename T>
    static T convert(ShmemPrimitive_ *obj, int index = 0);

    template <typename T>
    T operator[](int index) const;
};

template <typename T>
class ShmemPrimitive : public ShmemPrimitive_
{
protected:
    static size_t makeSpace(size_t size, ShmemHeap *heapPtr);

public:
    using ShmemPrimitive_::construct; // Bring base class construct methods into scope

    // static size_t construct(T val, ShmemHeap *heapPtr);

    // static size_t construct(std::vector<T> vec, ShmemHeap *heapPtr);

    // void checkType() const;

    // T operator[](int index) const;

    int find(T value) const;

    void set(int index, T value);

    operator T() const;

    operator std::vector<T>() const;

    operator std::string() const;

    void copyTo(T *target, int size = -1) const;

    const T *getPtr() const;

    T *getPtr();
};

#include "ShmemObj.tcc"
#include "ShmemPrimitive.tcc"

#endif // SHMEM_PRIMITIVE_H
