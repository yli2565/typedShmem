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
    friend class ShmemAccessor;

protected:
    template <typename T>
    static size_t makeSpace(size_t size, ShmemHeap *heapPtr);

    inline Byte *getBytePtr()
    {
        return reinterpret_cast<Byte *>(reinterpret_cast<Byte *>(this) + sizeof(ShmemPrimitive_));
    }

    inline const Byte *getBytePtr() const
    {
        return const_cast<ShmemPrimitive_ *>(this)->getBytePtr();
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
        ShmemPrimitive_ *target = static_cast<ShmemPrimitive_ *>(resolveOffset(offset, heapPtr));
        heapPtr->shfree(target);
    }

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
    std::string toString(int indent = 0, int maxElements = -1) const;

    std::string elementToString(int index) const;

    // Iterator related
    int nextIdx(int index) const;

    // Converters
    template <typename T>
    operator T() const;

    // operator pybind11::object() const;

    pybind11::object elementToPyObject(int index) const;

    // Arithmetic operators
    template <typename T>
    bool operator==(const T &val) const;
};

template <typename T>
class ShmemPrimitive : public ShmemPrimitive_
{
    static_assert(isPrimitive<T>() && !isVector<T>::value, "ShmemPrimitive can only be instantiated with int, double, or float.");

    friend class ShmemAccessor;

protected:
    // Calls base class template function with T
    static size_t makeSpace(size_t size, ShmemHeap *heapPtr)
    {
        return ShmemPrimitive_::makeSpace<T>(size, heapPtr);
    }

    // Retrieves a pointer to the data (protected)
    const T *getPtr() const
    {
        return reinterpret_cast<const T *>(getBytePtr());
    }

public:
    using ShmemPrimitive_::construct;

    // __getitem__
    T get(int index) const
    {
        return ShmemPrimitive_::get<T>(index);
    }

    T operator[](int index) const
    {
        return ShmemPrimitive_::operator[]<T>(index);
    }

    // __setitem__
    void set(T value, int index)
    {
        ShmemPrimitive_::set<T>(value, index);
    }

    // __delitem__

    // __contains__
    int contains(T value) const
    {
        return ShmemPrimitive_::contains<T>(value);
    }

    // __index__
    int index(T value) const
    {
        return ShmemPrimitive_::index<T>(value);
    }

    // Converters
    operator T() const
    {
        return this->ShmemPrimitive_::operator T();
    }

    operator std::vector<T>() const
    {
        return this->ShmemPrimitive_::operator std::vector<T>();
    }

    operator std::string() const
    {
        return this->ShmemPrimitive_::operator std::string();
    }
};

bool isConversableToShmemPrimitive(const pybind11::object &obj);

#include "ShmemObj.tcc"
#include "ShmemPrimitive.tcc"

#endif // SHMEM_PRIMITIVE_H
