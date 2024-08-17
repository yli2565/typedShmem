#include "ShmemObj.h"
// Please keep this inclusion before header guard, which make the order of include correct

#ifndef SHMEM_LIST_H
#define SHMEM_LIST_H

#include "ShmemHeap.h"
#include <vector>
#include <stdexcept>
#include <string>

class ShmemList : public ShmemObj
{
protected:
    uint listSize;
    ptrdiff_t listSpaceOffset;

    ptrdiff_t *relativeOffsetPtr();
    const ptrdiff_t *relativeOffsetPtr() const;

    int resolveIndex(int index) const;

    static size_t makeSpace(size_t listCapacity, ShmemHeap *heapPtr);
    static size_t makeListSpace(size_t listCapacity, ShmemHeap *heapPtr);

    // __str__ implementation
    static std::string toString(ShmemList *list, int indent = 0, int maxElements = 4);

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

    /**
     * @brief Empty constructor for ShmemList
     *
     * @param capacity The initial capacity
     * @param heapPtr The heap pointer
     * @return  Offset of the list from heap head
     */
    static size_t construct(size_t capacity, ShmemHeap *heapPtr);

    static void deconstruct(size_t offset, ShmemHeap *heapPtr);

    // Type (Special interface)
    int typeId() const;
    std::string typeStr() const;

    // __len__
    size_t len() const;

    // __getitem__
    template <typename T>
    T get(int index) const;

    // __setitem__ (only for assign)
    template <typename T>
    void set(const T &val, int index);

    // __delitem__ (for remove/pop)
    void del(int index);

    // __contains__
    template <typename T>
    bool contains(T value) const;

    // __index__
    template <typename T>
    int index(T value, int start = 0, int end = -1) const;

    // __str__
    std::string toString(int indent = 0, int maxElements = 4) const;

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
    ShmemList *remove(int index);

    // __pop__
    ShmemList *pop(int index = -1);

    // __clear__
    ShmemList *clear();

    // Aliases
    template <typename T>
    T operator[](int index) const; // get alias
};

// Include the template implementation file
#include "ShmemList.tcc"

#endif // SHMEM_LIST_H
