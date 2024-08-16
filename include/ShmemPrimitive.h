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

    static std::string toString(ShmemPrimitive_ *obj, int indent = 0, int maxElements = 4);

    static size_t construct(const char *str, ShmemHeap *heapPtr);

    static size_t construct(std::string str, ShmemHeap *heapPtr);
};

template <typename T>
class ShmemPrimitive : public ShmemPrimitive_
{
protected:
    static size_t makeSpace(size_t size, ShmemHeap *heapPtr);

public:
    using ShmemPrimitive_::construct; // Bring base class construct methods into scope

    static size_t construct(T val, ShmemHeap *heapPtr);

    static size_t construct(std::vector<T> vec, ShmemHeap *heapPtr);

    void checkType() const;

    T operator[](int index) const;

    int find(T value) const;

    void set(int index, T value);

    operator T() const;

    operator std::vector<T>() const;

    operator std::string() const;

    void copyTo(T *target, int size = -1) const;

    const T *getPtr() const;

    T *getPtr();

    std::string toString(int maxElements = 4) const;
};

#include "ShmemObj.tcc"
#include "ShmemPrimitive.tcc"

#endif // SHMEM_PRIMITIVE_H
