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
    ptrdiff_t *relativeOffsetPtr();
    const ptrdiff_t *relativeOffsetPtr() const;

    ShmemObj *getObj(int index) const;
    void setObj(int index, ShmemObj *obj);

    int resolveIndex(int index) const;

    static size_t makeSpace(size_t listCapacity, ShmemHeap *heapPtr);

    void add(ShmemObj *newObj, ShmemHeap *heapPtr);
    void assign(int index, ShmemObj *newObj, ShmemHeap *heapPtr);

public:
    uint listSize;
    ptrdiff_t listSpaceOffset;

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

    ShmemObj *at(int index);

    ShmemObj *pop();

    template <typename T>
    void add(T val, ShmemHeap *heapPtr);

    template <typename T>
    void assign(int index, T val, ShmemHeap *heapPtr);

    static std::string toString(ShmemList *list, int indent = 0, int maxElements = 4);
};

// Include the template implementation file
#include "ShmemList.tcc"

#endif // SHMEM_LIST_H
