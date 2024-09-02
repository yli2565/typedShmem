#include "ShmemObj.h"
// Please keep this inclusion before header guard, which make the order of include correct

#ifndef SHMEM_LIST_H
#define SHMEM_LIST_H

#include "TypeEncodings.h"
#include "ShmemHeap.h"
#include <vector>
#include <stdexcept>
#include <string>

class ShmemList : public ShmemObj
{
    friend class ShmemAccessor;

protected:
    uint listSize;
    ptrdiff_t listSpaceOffset;

    // Core methods
    ShmemObj *getObj(int index) const;
    void setObj(int index, ShmemObj *obj);

    ptrdiff_t *relativeOffsetPtr();
    const ptrdiff_t *relativeOffsetPtr() const;

    int resolveIndex(int index) const;

    static size_t makeSpace(size_t listCapacity, ShmemHeap *heapPtr);
    static size_t makeListSpace(size_t listCapacity, ShmemHeap *heapPtr);

    /**
     * @brief Capacity of the list
     *
     * @return capacity
     * @note Depend on ShmemObj::size
     */
    size_t listCapacity() const;

    /**
     * @brief Potential capacity of the list, it check the size of the memory block
     *
     * @return max capacity without reallocating
     * @note Depend on ShmemHeap::BlockHeader::size()
     * @warning Include some dangerous ptr operations
     */
    size_t potentialCapacity() const;

public:
    /**
     * @brief Generic constructor for ShmemList, accepts a vector as input
     *
     * @tparam T Content type, can be nested. Vector of primitive is not recommended, please directly use constructor of ShmemPrimitive
     * @param vec The initial value of the list
     * @param heapPtr The heap pointer
     * @return  Offset of the list from heap head
     */
    template <typename T>
    static size_t construct(std::vector<T> vec, ShmemHeap *heapPtr);

    static size_t construct(pybind11::list pyList, ShmemHeap *heapPtr);

    /**
     * @brief Empty constructor for ShmemList
     *
     * @param capacity The initial capacity
     * @param heapPtr The heap pointer
     * @return  Offset of the list from heap head
     */
    static size_t construct(size_t capacity, ShmemHeap *heapPtr);

    static void deconstruct(size_t offset, ShmemHeap *heapPtr);

    // __len__
    size_t len() const;

    // __getitem__
    template <typename T>
    T get(int index) const;

    // __setitem__ (only for assign)
    template <typename T>
    void set(const T &val, int index, ShmemHeap *heapPtr);

    // __delitem__ (for remove/pop)
    void del(int index, ShmemHeap *heapPtr);

    // __contains__
    template <typename T>
    bool contains(T value) const;

    // __index__
    template <typename T>
    int index(T value, int start = 0, int end = -1) const;

    // __str__
    std::string toString(int indent = 0, int maxElements = -1) const;

    // List interface

    // Resize utility
    void resize(int newSize, ShmemHeap *heapPtr); // use makeListSpace(newSize)

    // __append__
    template <typename T>
    ShmemList *append(const T &val, ShmemHeap *heapPtr);

    // __extend__
    template <typename T>
    ShmemList *extend(const ShmemList *another, ShmemHeap *heapPtr);

    // __insert__
    template <typename T>
    ShmemList *insert(int index, const T &val, ShmemHeap *heapPtr);

    // __remove__ (del alias)
    ShmemList *remove(int index, ShmemHeap *heapPtr);

    // __pop__
    template <typename T>
    T pop(int index, ShmemHeap *heapPtr);

    // __clear__
    ShmemList *clear(ShmemHeap *heapPtr);

    // Iterator related
    int nextIdx(int index) const;

    // Converters
    template <typename T>
    operator T() const;

    // Aliases
    template <typename T>
    T operator[](int index) const; // get alias

    // Arithmetic
    template <typename T>
    bool operator==(const T &val) const;
};

// Include the template implementation file
#include "ShmemList.tcc"

#endif // SHMEM_LIST_H
