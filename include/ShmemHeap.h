#ifndef SHMEM_HEAP_H
#define SHMEM_HEAP_H

#include <string>
#include <memory>
#include <atomic>
#include <spdlog/spdlog.h>
#include <limits>
#include <unistd.h>

#include "ShmemUtils.h"
#include "ShmemBase.h"

// Constants
#define DHCap 0x1000 // Default heap capacity is one page
#define DSCap 0x8    // Default static capacity is 8 bytes
#define NPtr 0x1     // An offset that is impossible to be a block offset, used to indicate special case

static const size_t unitSize = sizeof(void *);

class ShmemHeap : public ShmemBase
{
public:
    // Minimum static size
    const int minStaticSize = 4;

    // Inner BlockHeader structure
    struct BlockHeader
    {
        /**
         * size of the block / Busy bit / Previous Allocated bit / Allocated bit
         */
        size_t size_BPA;

        /**
         * @brief Provide {size | B bit | P bit | A bit} as a size_t reference.
         * @return reference to the 8 bytes {size | B bit | P bit | A bit} at *(this)
         * @note Similar to reinterpret cast BlockHeader ptr to a size_t ptr
         */
        size_t &val();

        /**
         * @brief Provide {size | B bit | P bit | A bit} as an atomic reference
         * @return atomic reference to the size_BPA
         * @warning It breaks the constness as atomic always intend to modify the value
         * Just don't use it or only use .load on it
         * @warning The memory should always be aligned to 8 bytes when you use this reference, ALWAYS.
         */
        inline std::atomic<size_t> &atomicVal() const;

        /**
         * @brief For free block, return the offset to the previous free block in linked list.
         *
         * @return pointer to the previous free block in free list
         */
        intptr_t &fwdOffset() const;

        /**
         * @brief For free block, return the offset to the next free block in linked list
         *
         * @return pointer to the next free block in free list
         */
        intptr_t &bckOffset() const;

        /**
         * @brief Size of the whole block (including header)
         *
         * @return size of the whole block
         */
        size_t size() const;

        /**
         * @brief Set the size bits in the header
         *
         * @param size size of the block (including header)
         */
        void setSize(size_t size);

        /**
         * @brief Is the block busy (writing/modifying by another process)
         *
         * @return true if the block is busy
         */
        bool B() const;

        /**
         * @brief Is the previous adjacent block allocated
         *
         * @return true if the previous adjacent block is allocated
         */
        bool P() const;

        /**
         * @brief Is this block allocated
         *
         * @return true if this block is allocated
         */
        bool A() const;

        /**
         * @brief Set the busy bit (default true)
         *
         * @param b busy bit
         */
        void setB(bool b = true);

        /**
         * @brief Set the previous allocated bit (default true)
         *
         * @param p previous allocated bit
         */
        void setP(bool p = true);

        /**
         * @brief Set the allocated bit (default true)
         *
         * @param a allocated bit
         */
        void setA(bool a = true);

        /**
         * @brief Get a pointer to the footer / potential footer's position.
         * Depends on size in header
         *
         * @return Pointer to this block's footer / potential footer
         */
        BlockHeader *getFooterPtr() const;

        /**
         * @brief Get the Ptr to the previous free block in the free list
         * @note Only for free blocks, depends on FwdOffset in the payload
         * @return ptr to the previous free block
         */
        BlockHeader *getFwdPtr() const;

        /**
         * @brief Get the Ptr to the next free block in the free list
         * @note Only for free blocks, depends on BckOffset in the payload
         * @return ptr to the next free block
         */
        BlockHeader *getBckPtr() const;

        /**
         * @brief Get the Ptr to the header of the previous adjacent block
         * @return ptr to the header of the previous adjacent block
         * @note Only works if the previous block is a free block
         * @note Depends on the previous block's footer in the previous block's payload
         */
        BlockHeader *getPrevPtr() const;

        /**
         * @brief Get the Ptr to the header of the next adjacent block
         * @return ptr to the header of the next adjacent block
         * @note Depends on size in header
         */
        BlockHeader *getNextPtr() const;

        /**
         * @brief Remove this block from a double linked list (free list)
         * @note Only for free blocks
         * @note Depends on BckOffset and FwdOffset in payload
         */
        void remove();

        /**
         * @brief Insert this block into a double linked list just after prevBlock
         * @param prevBlock the block which this block should be inserted after
         * @note Only for free blocks, and PrevBlock must also be a free block
         * @note Depends on BckOffset and FwdOffset in payload, as well as BckOffset in prevBlock's payload
         */
        void insert(BlockHeader *prevBlock);

        /**
         * @brief Calculate the offset from the current block to the given pointer
         *
         * @param desPtr destination pointer
         * @return positive / negative offset to the destination pointer
         */
        intptr_t offset(Byte *desPtr) const;

        /**
         * @brief Calculate the offset from the current block to the given pointer
         *
         * @param desPtr destination pointer
         * @return positive / negative offset to the destination pointer
         */
        intptr_t offset(BlockHeader *desPtr) const;

        /**
         * @brief Wait until the busy bit is cleared
         *
         * @param timeout Maximum wait time in milliseconds (-1 means no timeout)
         * @return 0 if the busy bit is cleared, ETIMEDOUT if the timeout is reached
         */
        int wait(int timeout = -1) const;
    };

    // Constructor
    ShmemHeap() : ShmemHeap("", DSCap, DHCap) {}
    ShmemHeap(const ShmemHeap &other) : ShmemHeap(other.getName(), (const_cast<ShmemHeap &>(other)).staticCapacity(), (const_cast<ShmemHeap &>(other)).heapCapacity()) {}
    ShmemHeap(const std::string &name, size_t staticSpaceSize = DSCap, size_t heapSize = DHCap);

    /**
     * @brief Create a basic shared memory and setup the heap on it
     * @note The static space and heap capacity could be set with setSCap() and setHCap()
     * before calling create(). After that, setting HCap and SCap will have no effect.
     */
    void create();

    /**
     * @brief Resize the heap space
     *
     * @param heapSize New size for the heap, padded to page size. -1 means not changed
     */
    void resize(long heapSize);

    /**
     * @brief Resize both static and heap space
     *
     * @param staticSpaceSize required size of static space, padded to unitSize. -1 means not changed
     * @param heapSize required size of heap space, padded to page size. -1 means not changed
     */
    void resize(long staticSpaceSize, long heapSize);

    /**
     * @brief Allocate a block in the heap
     * @param size size of the payload, will be padded to a multiple of unitSize
     * @return offset of the allocated block from the heap head
     */
    size_t shmalloc(size_t size);

    /**
     * @brief Reallocate a block in the heap, keep content
     * @param offset offset of original block from the heap head
     * @param size new size of the payload, will be padded to a multiple of unitSize
     * @return offset of the reallocated block from the heap head
     */
    size_t shrealloc(size_t offset, size_t size);

    /**
     * @brief Free a block in the heap
     * @param offset offset of the payload from the heap head
     * @return 0 on success, -1 on failure
     */
    int shfree(size_t offset);

    /**
     * @brief Free a block in the heap
     * @param ptr any kind of pointer to a payload
     * @return 0 on success, -1 on failure
     */
    template <typename T>
    int shfree(T *ptr)
    {
        return this->shfreeHelper(reinterpret_cast<Byte *>(ptr));
    }

    // Getters

    /**
     * @brief Check the current purposed heap capacity
     *
     * @return size_t purposed heap capacity
     */
    size_t getHCap() const;

    /**
     * @brief Check the current purposed static space capacity
     *
     * @return size_t purposed static space capacity
     */
    size_t getSCap() const;

    // Safe Getters (check shm connection)

    /**
     * @brief Get the size of the static space recorded in the first size_t(8 bytes) of the heap
     *
     * @return size_t reference to the first 8 bytes of the heap
     */
    size_t &staticCapacity();

    /**
     * @brief Get the size of the heap recorded in the second size_t(8 bytes) of the heap
     *
     * @return size_t reference to the second 8 bytes of the heap
     */
    size_t &heapCapacity();

    /**
     * @brief Get the offset(from the heap head) of the free block list, recorded in the third size_t(8 bytes) of the heap
     *
     * @return size_t reference to the offset of the free block list
     * @note the offset can be calculated into a reference to a free block in the heap, thus can be used as the head of the free block list (double linked list)
     */
    size_t &freeBlockListOffset();

    /**
     * @brief Get the offset(from the heap head) of the entrance, recorded in the fourth size_t(8 bytes) of the heap
     *
     * @return size_t reference to the offset of the entrance point obj
     * @note the offset can be calculated into a reference to a free block in the heap
     */
    size_t &entranceOffset();

    /**
     * @brief Get the head ptr of the static space. Check connection before using
     *
     * @return this->shmPtr
     */
    Byte *staticSpaceHead();

    /**
     * @brief Entrance point of the underlying data structure
     *
     * @return this->shmPtr + staticCapacity + entranceOffset
     */
    Byte *entrance();

    /**
     * @brief Get the head ptr of the heap. Check connection before using
     *
     * @return this->shmPtr + staticCapacity (skip the static space)
     */
    Byte *heapHead();

    /**
     * @brief Get the tail ptr of the heap. Check connection before using
     *
     * @return this->shmPtr + staticCapacity + +staticCapacity + heapCapacity
     */
    Byte *heapTail();

    /**
     * @brief Get the head ptr of the free block list. Check connection before using
     *
     * @return this->heapHead + freeBlockListOffset
     */
    BlockHeader *freeBlockList();

    // Setters

    /**
     * @brief Repurpose the heap capacity
     *
     * @param size purposed heap capacity
     */
    void setHCap(size_t size);

    /**
     * @brief Repurpose the static space capacity
     *
     * @param size purposed static space capacity
     */
    void setSCap(size_t size);

    // Utility Functions
    std::shared_ptr<spdlog::logger> &getLogger();
    const std::shared_ptr<spdlog::logger> &getLogger() const;

    // Debug
    /**
     * @brief Print the layout of the heap in detail
     */
    void printShmHeap();
    /**
     * @brief Get size of all blocks in the heap, no state information
     *
     * @return vector of size_BPA of all blocks
     */
    std::vector<size_t> briefLayout();
    /**
     * @brief String representation of payload size and alloc bit of all blocks in the heap
     * including state information
     * @return std::string
     * @note e.g. "256A, 128E, 64A, 32E, 16A, 8E, 4A, 2E, 1A" E-empty, A-allocated
     */
    std::string briefLayoutStr();

    // TODO: Untested functions
    void borrow(const ShmemHeap &heap);
    void steal(ShmemHeap &&heap);

protected:
    // Helpers
    int shfreeHelper(Byte *ptr);

    // Utility functions
    bool verifyPayloadPtr(Byte *ptr);

    // Free list pointer manipulators
    void insertFreeBlock(BlockHeader *block, BlockHeader *prevBlock = nullptr);
    void removeFreeBlock(BlockHeader *block);

    // Fast arithmetic, without connection check
    size_t &staticCapacity_unsafe();
    size_t &heapCapacity_unsafe();
    size_t &freeBlockListOffset_unsafe();
    size_t &entranceOffset_unsafe();

    /**
     * @brief Get the head ptr of the static space
     *
     * @return this->shmPtr
     */
    Byte *staticSpaceHead_unsafe();

    /**
     * @brief Get the entrance ptr of the heap
     *
     * @return this->shmPtr + staticCapacity + entranceOffset
     */
    Byte *entrance_unsafe();

    /**
     * @brief Get the head ptr of the heap
     *
     * @return this->shmPtr + staticCapacity (skip the static space)
     */
    Byte *heapHead_unsafe();

    /**
     * @brief Get the tail ptr of the heap
     *
     * @return this->shmPtr + staticCapacity + +staticCapacity + heapCapacity
     */
    Byte *heapTail_unsafe();

    /**
     * @brief Get the head ptr of the free block list
     *
     * @return this->heapHead + freeBlockListOffset
     */
    BlockHeader *freeBlockList_unsafe();

private:
    // preset capacity

    /**
     * @brief Purposed capacity for static space, only used to initially
     * create a shared memory heap. After first connect/create, the
     * capacity is tracked in the shared memory. Access by
     * staticSpaceCapacity()
     *
     * Aligned to a multiple of unitSize, and >= minStaticSize
     */
    size_t SCap = 0;

    /**
     * @brief Purposed capacity for heap space, only used to initially
     * create a shared memory heap. After first connect/create, the
     * capacity is tracked in the shared memory. Access by
     * heapCapacity_unsafe()
     *
     * Aligned to a multiple of page size
     */
    size_t HCap = 0;

    // Logger
    std::shared_ptr<spdlog::logger> logger;
};

#endif // SHMEM_HEAP_H
