#include "ShmemObj.h"
// Please keep this inclusion before header guard, which make the order of include correct

#ifndef SHMEM_PRIMITIVE_H
#define SHMEM_PRIMITIVE_H

#include "TypeEncodings.h"
#include <cstring>

// The ShmemPrimitive_ provide all the implementations on the dynamic type side
// So, for those dynamic type dominated functions, ShmemPrimitive_ will handle them separately

class ShmemPrimitive_ : public ShmemObj
{
protected:
    template <typename T>
    static size_t makeSpace(size_t size, ShmemHeap *heapPtr);

    inline Byte *getBytePtr()
    {
        return reinterpret_cast<Byte *>(reinterpret_cast<Byte *>(this) + sizeof(ShmemObj));
    }

    inline const Byte *getBytePtr() const
    {
        return getBytePtr();
    }

    int resolveIndex(int index) const;

public:
    // Constructors
    template <typename T>
    static size_t construct(const T &val, ShmemHeap *heapPtr);

    static size_t construct(const char *str, ShmemHeap *heapPtr);

    static size_t construct(const std::string str, ShmemHeap *heapPtr);

    // Destructors
    static inline void deconstruct(size_t offset, ShmemHeap *heapPtr)
    {
        heapPtr->shfree(heapPtr->heapHead() + offset);
    }

    // Type (Special interface)
    int typeId() const;
    std::string typeStr() const;

    // __len__
    size_t len() const;

    // __getitem__
    template <typename T>
    T get(int index) const;

    template <typename T>
    T operator[](int index) const;

    // __setitem__
    template <typename T>
    void set(const T &val, int index);

    // __delitem__

    /**
     * @brief remove element at index
     * 
     * @param index element index to delete
     * @note poor performance due to array shifting
     */
    void del(int index);

    // __contains__
    template <typename T>
    bool contains(T value) const;

    // __index__
    template <typename T>
    int index(T value) const;

    // __str__
    std::string toString(int indent = 0, int maxElements = 4) const;

    std::string elementToString(int index) const;

    // Converters
    template <typename T>
    operator T() const;

    // Arithmetic interface
};

template <typename T>
class ShmemPrimitive : public ShmemPrimitive_
{
    static_assert(isPrimitive<T>() && !isVector<T>::value, "ShmemPrimitive can only be instantiated with int, double, or float.");

protected:
    static size_t makeSpace(size_t size, ShmemHeap *heapPtr);

    const T *getPtr() const;

public:
    using ShmemPrimitive_::construct;

    operator T() const;

    operator std::vector<T>() const;

    operator std::string() const;

    T get(int index) const;

    T operator[](int index) const;

    void set(T value, int index);

    int contains(T value) const;
};

#include "ShmemObj.tcc"
#include "ShmemPrimitive.tcc"

#endif // SHMEM_PRIMITIVE_H
