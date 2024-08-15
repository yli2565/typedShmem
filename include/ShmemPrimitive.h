#ifndef SHMEM_PRIMITIVE_H
#define SHMEM_PRIMITIVE_H

#include "ShmemHeap.h"
#include "ShmemObj.h"
#include "TypeEncodings.h"
#include <cstring>

class ShmemPrimitive_ : public ShmemObj
{
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
};

template <typename T>
class ShmemPrimitive : public ShmemPrimitive_
{
private:
    static size_t makeSpace(size_t size, ShmemHeap *heapPtr);

public:
    static size_t construct(T val, ShmemHeap *heapPtr);

    static size_t construct(std::vector<T> vec, ShmemHeap *heapPtr);

    static size_t construct(std::string str, ShmemHeap *heapPtr);

    static size_t construct(const char *str, ShmemHeap *heapPtr);

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
