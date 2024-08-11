#ifndef SHMEM_HEAP_H
#define SHMEM_HEAP_H

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <limits>
#include <unistd.h>

#include "ShmemUtils.h"
#include "ShmemBase.h"

#define DHCap 0x1000 // default heap capacity is one page
#define DSCap 0x8    // default static capacity is 8 bytes
#define NPtr 0x1     // an offset that is impossible to be a block offset

static const size_t unitSize = sizeof(void *);

class ShmemHeap : public ShmemBase
{
public:
    const static int minStaticSize = 4;
    struct BlockHeader
    {
        /**
         * size of the block / Busy bit / Previous Allocated bit / Allocated bit
         */
        size_t size_BPA;

        /**
         * @brief Provide {size | B bit | P bit | A bit} as a size_t reference.
         *
         * Similar to reinterpret cast BlockHeader ptr to a size_t ptr
         *
         * @return reference to the 8 bytes {size | B bit | P bit | A bit} at *(this)
         */
        size_t &val();

        /**
         * @brief Provide {size | B bit | P bit | A bit} as an atomic reference
         * Warning: it breaks the constness as atomic always intend to modify the value
         * Just don't use it or only use .load on it
         * Warning: the memory should always be aligned to 8 bytes when you use this reference, ALWAYS.
         * @return atomic reference to the size_BPA
         */
        inline std::atomic<size_t> &atomicVal() const;

        /**
         * @brief For free block, return the offset to the previous free block in linked list.
         *
         * @return pointer to the previous free block in free list
         */
        intptr_t &fwdOffset();

        /**
         * @brief For free block, return the offset to the next free block in linked list
         *
         * @return pointer to the next free block in free list
         */
        intptr_t &bckOffset();

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
         * Only for free blocks, depends on FwdOffset in the payload
         * @return ptr to the previous free block
         */
        BlockHeader *getFwdPtr() const;

        /**
         * @brief Get the Ptr to the next free block in the free list
         * Only for free blocks, depends on BckOffset in the payload
         * @return ptr to the next free block
         */
        BlockHeader *getBckPtr() const;

        /**
         * @brief Get the Ptr to the header of the previous adjacent block
         * Only works if the previous block is a free block
         * Depends on the previous block's footer in the previous block's payload
         * @return ptr to the header of the previous adjacent block
         */
        BlockHeader *getPrevPtr() const;

        /**
         * @brief Get the Ptr to the header of the next adjacent block
         * Depends on size in header
         * @return ptr to the header of the next adjacent block
         */
        BlockHeader *getNextPtr() const;

        /**
         * @brief Remove this block from a double linked list (free list)
         * Only for free blocks
         * Depends on BckOffset and FwdOffset in payload
         */
        void remove();

        /**
         * @brief Insert this block into a double linked list just after prevBlock
         * Only for free blocks
         * PrevBlock must also be a free block
         * Depends on BckOffset and FwdOffset in payload
         * As well as BckOffset in prevBlock's payload
         * @param prevBlock the block which this block should be inserted after
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

    ShmemHeap(const std::string &name, size_t staticSpaceSize = DSCap, size_t heapSize = DHCap) : ShmemBase(name)
    {
        // init logger
        this->logger = spdlog::default_logger()->clone("ShmHeap:" + name);
        std::string pattern = "[" + std::to_string(getpid()) + "] [%n] %v";
        this->logger->set_pattern(pattern);

        this->setSCap(staticSpaceSize);
        this->setHCap(heapSize);
        this->logger->info("Initialized to: Static space capacity: {} heap capacity: {}", this->SCap, this->HCap);

        this->setCapacity(this->SCap + this->HCap);
    }
    void create()
    {
        if (this->SCap < this->minStaticSize || this->HCap == 0)
        {
            throw std::runtime_error("Capacity is too small to hold static space or heap space");
        }
        ShmemBase::create();

        // Init the static information
        this->staticCapacity_unsafe() = this->SCap;
        this->heapCapacity_unsafe() = this->HCap;

        BlockHeader *firstBlock = reinterpret_cast<BlockHeader *>(this->heapHead_unsafe());

        // Prev allocated bit set to 1
        firstBlock->val() = this->HCap | 0b010;
        firstBlock->getFooterPtr()->val() = this->HCap;
        this->freeBlockListOffset_unsafe() = reinterpret_cast<Byte *>(firstBlock) - this->heapHead_unsafe();
    }
    void resize(size_t heapSize)
    {
        this->checkConnection();
        this->resize(this->staticCapacity_unsafe(), heapSize);
    };

    /**
     * @brief Resize both static and heap space
     *
     * @param staticSpaceSize required size of static space, might be padded. -1 means not changed
     * @param heapSize required size of heap space, might be padded. -1 means not changed
     */
    void resize(size_t staticSpaceSize, size_t heapSize)
    {
        this->checkConnection();
        if (staticSpaceSize == -1)
        { // Don't change static space capacity
            this->setSCap(this->staticCapacity_unsafe());
        }
        else
        {
            this->setSCap(staticSpaceSize);
        }
        if (heapSize == -1)
        { // Don't change heap capacity
            this->setHCap(this->heapCapacity_unsafe());
        }
        else
        {
            this->setHCap(heapSize);
        }

        size_t newStaticSpaceCapacity = this->SCap;
        size_t newHeapCapacity = this->HCap;

        // find the last block
        BlockHeader *lastBlock = this->freeBlockListOffset_unsafe() != NPtr ? this->freeBlockList_unsafe() : reinterpret_cast<BlockHeader *>(this->heapHead_unsafe()); // Start from a free block in the middle (hopefully close to the end)
        while (reinterpret_cast<Byte *>(lastBlock->getNextPtr()) != this->heapTail_unsafe())
        {
            lastBlock = lastBlock->getNextPtr();
        }
        uintptr_t lastBlockOffset = reinterpret_cast<Byte *>(lastBlock) - this->heapHead_unsafe();
        bool lastBlockAllocated = lastBlock->A();

        // Save everything
        size_t oldStaticSpaceCapacity = this->staticCapacity_unsafe();
        size_t oldHeapCapacity = this->heapCapacity_unsafe();
        Byte *tempStaticSpace = new Byte[oldStaticSpaceCapacity];
        std::memcpy(tempStaticSpace, this->shmPtr, oldStaticSpaceCapacity);
        Byte *tempHeap = new Byte[oldHeapCapacity];
        std::memcpy(tempHeap, this->heapHead_unsafe(), oldHeapCapacity);

        // resize static space (without copy content)
        ShmemBase::resize(newStaticSpaceCapacity + newHeapCapacity, false);

        // Restore the static space content
        std::memcpy(this->shmPtr, tempStaticSpace, oldStaticSpaceCapacity);

        // Overwrite the old capacity
        this->staticCapacity_unsafe() = newStaticSpaceCapacity;
        this->heapCapacity_unsafe() = newHeapCapacity;

        // Restore the heap content
        std::memcpy(this->heapHead_unsafe(), tempHeap, oldHeapCapacity);

        // Get ptr to the new last block
        lastBlock = reinterpret_cast<BlockHeader *>(this->heapHead_unsafe() + lastBlockOffset);

        // Check if there's enough space to create a new free block
        if (newHeapCapacity + 4 * unitSize < oldHeapCapacity || !lastBlockAllocated)
        {
            // There's not enough space to create a new free block, or the last block is not allocated,
            // just give all the space to last block
            lastBlock->setSize(lastBlock->size() + newHeapCapacity - oldHeapCapacity);
            if (!lastBlockAllocated)
            {
                lastBlock->getFooterPtr()->setSize(newHeapCapacity - oldHeapCapacity);
            }
        }
        else
        {
            // Create a new free block
            BlockHeader *newBlock = lastBlock->getNextPtr();
            // Size: new heap capacity - old heap capacity, Busy 0, PrevAllocated 1, Allocated 0
            newBlock->val() = (newHeapCapacity - oldHeapCapacity) | 0b010;
            newBlock->getFooterPtr()->setSize(newHeapCapacity - oldHeapCapacity);

            // Add the new block to the free block list
            this->insertFreeBlock(newBlock);
        }

        delete[] tempStaticSpace;
        delete[] tempHeap;
    }

    size_t &staticCapacity()
    {
        checkConnection();
        return reinterpret_cast<size_t *>(this->shmPtr)[0];
    }
    size_t &heapCapacity()
    {
        checkConnection();
        return reinterpret_cast<size_t *>(this->shmPtr)[1];
    }
    size_t &freeBlockListOffset()
    {
        checkConnection();
        return reinterpret_cast<size_t *>(this->shmPtr)[2];
    }
    Byte *heapHead()
    {
        checkConnection();
        return this->shmPtr + this->staticCapacity_unsafe();
    }
    Byte *heapTail()
    {
        checkConnection();
        return this->shmPtr + this->staticCapacity_unsafe() + this->heapCapacity_unsafe();
    }
    BlockHeader *freeBlockList()
    {
        checkConnection();
        return reinterpret_cast<BlockHeader *>(this->heapHead_unsafe() + this->freeBlockListOffset_unsafe());
    }
    void removeFreeBlock(BlockHeader *block)
    {
        if (this->freeBlockListOffset_unsafe() == reinterpret_cast<Byte *>(block) - this->heapHead_unsafe())
        {
            if (block->getBckPtr() == block)
            {
                // Block is the only element in the list
                this->freeBlockListOffset_unsafe() = NPtr; // NPtr is an impossible byte offset of a block
            }
            else
            {
                this->freeBlockListOffset_unsafe() = reinterpret_cast<Byte *>(block->getBckPtr()) - this->heapHead_unsafe();
            }
        }
        block->remove();
    }
    void insertFreeBlock(BlockHeader *block, BlockHeader *prevBlock = nullptr)
    {
        if (this->freeBlockListOffset_unsafe() == NPtr)
        {
            // Don't care the previous block when the list is empty, it must be an outdated block
            this->freeBlockListOffset_unsafe() = reinterpret_cast<Byte *>(block) - this->heapHead_unsafe();
            block->insert(nullptr);
        }
        else
        {
            if (prevBlock == nullptr)
                block->insert(this->freeBlockList_unsafe());
            else
                block->insert(prevBlock);
        }
    }
    /**
     * @brief Aligned the start of the heap to the nearest 8 bytes
     *
     * @return offset of the payload
     */
    size_t shmalloc(size_t size)
    {
        this->checkConnection();
        if (size < 1)
            return 0;

        // Calculate the padding size
        size_t padSize = 0;
        padSize = size % unitSize == 0 ? 0 : unitSize - (size % unitSize);
        // The payload should be at least 3 units
        if (size < 3 * unitSize)
            padSize = 3 * unitSize - size;

        // Header + size + Padding
        size_t requiredSize = unitSize + size + padSize;

        BlockHeader *listHead = this->freeBlockList_unsafe();
        BlockHeader *current = listHead;

        BlockHeader *best = nullptr;
        size_t bestSize = std::numeric_limits<size_t>::max();

        // Best fit logic
        do
        {
            size_t blockSize = current->size();
            if (!current->A() && blockSize >= requiredSize && blockSize < bestSize)
            {
                best = current;
                bestSize = blockSize;

                // Break if exact match
                if (blockSize == requiredSize)
                    break;
            }
            current = current->getBckPtr();
        } while (current != listHead);

        if (best != nullptr)
        {
            // Lock the best fit block (Busy bit <- 1)
            best->wait();
            best->setB(true);

            // Check if the best size < required size + 4
            // In that case, we cannot split the block as a free block is minimum 4 bytes, we just increase the required size
            if (bestSize < requiredSize + 4 * unitSize)
            {
                requiredSize = bestSize;
            }

            BlockHeader *fwdBlockHeader = best->getFwdPtr();
            // BlockHeader *bckBlockHeader = best->getBckPtr();

            // Set allocated bit to 0
            best->setA(false);
            // Remove the best block from free list
            this->removeFreeBlock(best);

            // If the block we find is bigger than the required size, split it
            if (bestSize > requiredSize)
            {
                BlockHeader *newBlockHeader = reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(best) + requiredSize);

                // update the new block
                // Size: bestSize - requiredSize; Busy: 1; Previous Allocated: 1; Allocated: 0
                newBlockHeader->val() = (bestSize - requiredSize) | 0b010;
                newBlockHeader->getFooterPtr()->val() = bestSize - requiredSize;

                this->insertFreeBlock(newBlockHeader, fwdBlockHeader);

                // Reset busy bit
                newBlockHeader->setB(false);
            }

            // update the best fit block
            // Size: requiredSize; Busy: 0; Previous Allocated: not changed; Allocated: 1
            best->val() = requiredSize & ~0b100 | (best->size_BPA & 0b010) | 0b001;

            // update next block's p bit
            if (reinterpret_cast<Byte *>(best->getNextPtr()) < this->heapTail_unsafe())
            {
                best->getNextPtr()->setP(true);
            }

            // Return the offset from the heap head (+1 make the offset is on start of payload)
            return reinterpret_cast<Byte *>(best + 1) - this->heapHead_unsafe();
        }
        else
        {
            return 0;
        }
    }
    size_t shrealloc(size_t offset, size_t size)
    {
        this->checkConnection();

        // special cases
        if (size == 0)
        {
            this->shfree(offset);
            return 0;
        }
        if (offset == 0)
            return this->shmalloc(size);

        BlockHeader *header = reinterpret_cast<BlockHeader *>(this->heapHead_unsafe() + offset) - 1;
        // TODO: check if it is a vlid header

        // Header + size + Padding
        size_t requiredSize = pad(size + sizeof(BlockHeader), unitSize);

        // At least 4 bytes for a block
        if (requiredSize < 4 * unitSize)
            requiredSize = 4 * unitSize;

        size_t oldSize = header->size();

        if (oldSize == requiredSize)
        {
            return offset;
        }
        else if (oldSize > requiredSize)
        {
            if (oldSize - 4 * unitSize < requiredSize)
            {
                // a free block is at least 4 bytes, we cannot split the block
                // TODO: the following block is a free bloc, we can merge the bytes into it

                this->logger->warn("A reallocation request is ignore as a free block is at least 4 bytes. Request size: {}, old size: {}", requiredSize, oldSize);
            }
            else
            {
                // split a new free block
                BlockHeader *newBlockHeader = reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(header) + requiredSize);
                // update the new block
                // Size: bestSize - requiredSize; Busy: 1; Previous Allocated: 1; Allocated: 0
                newBlockHeader->val() = (header->size() - requiredSize) | 0b010;
                newBlockHeader->getFooterPtr()->val() = header->size() - requiredSize;

                this->insertFreeBlock(newBlockHeader);

                // Reset busy bit
                newBlockHeader->setB(false);

                // update the header block
                // Size: requiredSize; Busy: 0; Previous Allocated: not changed; Allocated: 1
                header->val() = requiredSize & ~0b100 | (header->size_BPA & 0b010) | 0b001;
            }
            return offset;
        }
        else
        { // The new size is larger than the old size
            size_t oldPayloadSize = oldSize - sizeof(BlockHeader);

            Byte *tempPayload = new Byte[oldPayloadSize];
            // Copy the payload
            std::memcpy(tempPayload, this->heapHead_unsafe() + offset, oldPayloadSize);

            // Free the old payload
            this->shfree(offset);

            // Re-allocate the new payload
            size_t newPayloadOffset = this->shmalloc(size);

            std::memcpy(this->heapHead_unsafe() + newPayloadOffset, tempPayload, oldPayloadSize);
            
            delete[] tempPayload;

            return newPayloadOffset;
        }
    }
    int shfree(size_t offset)
    {
        this->checkConnection();
        return this->shfree(this->heapHead_unsafe() + offset);
    }
    int shfree(Byte *ptr)
    {
        this->checkConnection();

        if (ptr < this->heapHead_unsafe() || ptr >= this->heapTail_unsafe())
            return -1;

        // Since there must be a header, a payload ptr should be at least unitSize bytes
        if (ptr - this->heapHead_unsafe() < unitSize)
            return -1;

        if ((ptr - this->heapHead_unsafe()) % unitSize != 0)
            return -1;

        BlockHeader *header = reinterpret_cast<BlockHeader *>(ptr) - 1;

        // Set Busy bit
        header->wait();
        header->setB(true);

        if (!header->A())
            return -1;

        // Set Allocated bit to 0
        header->setA(false);

        this->insertFreeBlock(header);

        // Create Footer
        BlockHeader *newFooter = header->getFooterPtr();
        newFooter->val() = header->size();

        // Remove next block's P bit
        if (reinterpret_cast<Byte *>(header->getNextPtr()) < this->heapTail_unsafe())
        {
            header->getNextPtr()->setP(false);
        }

        // Immediate coalescing
        BlockHeader *coalesceTarget = header;

        // First coalesce with the previous block

        while (!coalesceTarget->P())
        { // Each iteration coalesce two adjacent blocks
            BlockHeader *prevBlockHeader = coalesceTarget->getPrevPtr();
            if (prevBlockHeader->A())
                throw std::runtime_error("previous block is allocated, but following block's P bit is set");

            // Wait Busy bit
            prevBlockHeader->wait();
            prevBlockHeader->setB(true);

            size_t newSize = prevBlockHeader->size() + coalesceTarget->size();

            // Size: newSize; Busy: 1; Previous Allocated: not changed; Allocated: 0
            prevBlockHeader->size_BPA = newSize | 0b100 | (prevBlockHeader->size_BPA & 0b010) & ~0b001;
            newFooter->val() = newSize;

            // update free list ptr
            this->removeFreeBlock(coalesceTarget);

            coalesceTarget = prevBlockHeader;
        }

        while (reinterpret_cast<Byte *>(coalesceTarget->getNextPtr()) < this->heapTail_unsafe() && !coalesceTarget->getNextPtr()->A())
        {
            BlockHeader *nextBlockHeader = coalesceTarget->getNextPtr();

            // Wait Busy bit
            nextBlockHeader->wait();

            newFooter = nextBlockHeader->getFooterPtr();

            size_t newSize = coalesceTarget->size() + nextBlockHeader->size();

            // Size: newSize; Busy: 1; Previous Allocated: not changed; Allocated: 0
            coalesceTarget->size_BPA = newSize | (coalesceTarget->size_BPA & 0b111);
            newFooter->val() = newSize;

            // update free list ptr
            this->removeFreeBlock(nextBlockHeader);

            // coalesceTarget unchanged;
        }

        // Reset Busy bit
        coalesceTarget->setB(false);

        return 0;
    }
    void printShmHeap()
    {
        checkConnection();
        size_t staticCapacity = this->staticCapacity_unsafe();
        size_t heapCapacity = this->heapCapacity_unsafe();
        size_t firstFreeBlockOffset = this->freeBlockListOffset_unsafe();
        Byte *heapHead = this->heapHead_unsafe();
        Byte *heapTail = this->heapTail_unsafe();
        std::vector<BlockHeader *> freeBlockList = {};
        if (this->freeBlockListOffset_unsafe() != NPtr)
        {
            freeBlockList.push_back(this->freeBlockList_unsafe());
            BlockHeader *current = this->freeBlockList_unsafe()->getBckPtr();
            while (current != this->freeBlockList_unsafe())
            {
                freeBlockList.push_back(current);
                current = current->getBckPtr();
            }
        }
        this->logger->info("********************************* Static Space ****************************");
        this->logger->info("Static Space Capacity: {}", staticCapacity);
        this->logger->info("Heap Capacity: {}", heapCapacity);
        this->logger->info("Free block list offset: {}", firstFreeBlockOffset);
        this->logger->info("********************************** Block List *****************************");
        // this->logger->info("Offset\tStatus\tPrev\tBusy\tt_Begin\tt_End\tt_Size");
        this->logger->info("{:<8} {:<6} {:<6} {:<6} {:<14} {:<14} {:<6}", "Offset", "Status", "Prev", "Busy", "Begin", "End", "Size");

        uintptr_t begin, end, size;
        BlockHeader *current = reinterpret_cast<BlockHeader *>(heapHead);
        while (reinterpret_cast<Byte *>(current) < heapTail)
        {
            size = current->size();
            begin = reinterpret_cast<uintptr_t>(current);
            end = begin + size;
            this->logger->info("{:#08x} {:<6d} {:<6d} {:<6d} {:#014x} {:#014x} {:<6d}",
                               reinterpret_cast<Byte *>(current) - heapHead,
                               current->A() ? 1 : 0,
                               current->P() ? 1 : 0,
                               current->B() ? 1 : 0,
                               begin,
                               end,
                               size);
            current = current->getNextPtr();
        }
        this->logger->info("********************************** Free List ******************************");
        for (BlockHeader *block : freeBlockList)
        {
            this->logger->info("Block: {:#08x} Size: {}", reinterpret_cast<Byte *>(block) - heapHead, block->size());
        }
        this->logger->info("---------------------------------------------------------------------------");
    }
    std::vector<size_t> briefLayout()
    {
        checkConnection();
        std::vector<size_t> layout = {};

        Byte *heapHead = this->heapHead_unsafe();
        Byte *heapTail = this->heapTail_unsafe();

        BlockHeader *current = reinterpret_cast<BlockHeader *>(heapHead);
        while (reinterpret_cast<Byte *>(current) < heapTail)
        {
            layout.push_back(current->size_BPA);
            current = current->getNextPtr();
        }

        return layout;
    }
    std::string briefLayoutStr()
    {
        checkConnection();
        std::vector<size_t> layout = briefLayout();
        std::string layoutStr = "";
        for (int i = 0; i < layout.size(); i++)
        {
            size_t size = layout[i];
            // (<size>A) or (<size>E) the second element is allocated bit
            layoutStr += std::to_string((size & ~0b111) - unitSize) + (size & 0b001 ? "A" : "E");
            if (i < layout.size() - 1)
                layoutStr += ", ";
        }
        return layoutStr;
    }
    // Getters

    /**
     * @brief Check the current purposed heap capacity
     *
     * @return size_t purposed heap capacity
     */
    size_t getHCap() { return this->HCap; }
    /**
     * @brief Check the current purposed static space capacity
     *
     * @return size_t purposed static space capacity
     */
    size_t getSCap() { return this->SCap; }
    // Setters

    /**
     * @brief Repurpose the heap capacity
     *
     * @param size purposed heap capacity
     */
    void setHCap(size_t size)
    {
        size_t pageSize = sysconf(_SC_PAGESIZE);

        // pad heap capacity to a multiple of page size
        size_t newHeapCapacity = pad(size, pageSize);

        if (this->isConnected())
        {
            size_t currentHeapCapacity = this->heapCapacity_unsafe();
            if (newHeapCapacity < currentHeapCapacity)
                throw std::runtime_error("Heap capacity cannot be shrink");
        }

        this->HCap = newHeapCapacity;
        this->logger->info("Request heap size: {}, new heap capacity: {}", size, newHeapCapacity);
    }
    /**
     * @brief Repurpose the static space capacity
     *
     * @param size purposed static space capacity
     */
    void setSCap(size_t size)
    { // pad static space capacity to a multiple of 8
        size_t newStaticSpaceCapacity = pad(size, unitSize);

        if (size < this->minStaticSize * unitSize)
            newStaticSpaceCapacity = this->minStaticSize * unitSize;

        if (this->isConnected())
        {
            size_t currentStaticSpaceCapacity = this->staticCapacity_unsafe();
            if (newStaticSpaceCapacity < currentStaticSpaceCapacity)
                throw std::runtime_error("Static space capacity cannot be shrink");
        }

        this->SCap = newStaticSpaceCapacity;
        this->logger->info("Request static space size: {}, new static space capacity: {}", size, newStaticSpaceCapacity);
    }

    // Utility Functions
    std::shared_ptr<spdlog::logger> &getLogger()
    {
        return this->logger;
    }

protected:
    inline size_t &staticCapacity_unsafe()
    {
        return reinterpret_cast<size_t *>(this->shmPtr)[0];
    }
    inline size_t &heapCapacity_unsafe()
    {
        return reinterpret_cast<size_t *>(this->shmPtr)[1];
    }
    inline size_t &freeBlockListOffset_unsafe()
    {
        return reinterpret_cast<size_t *>(this->shmPtr)[2];
    }
    inline Byte *heapHead_unsafe()
    {
        return this->shmPtr + this->staticCapacity_unsafe();
    }
    inline Byte *heapTail_unsafe()
    {
        return this->shmPtr + this->staticCapacity_unsafe() + this->heapCapacity_unsafe();
    }
    inline BlockHeader *freeBlockList_unsafe()
    {
        return reinterpret_cast<BlockHeader *>(this->heapHead_unsafe() + this->freeBlockListOffset_unsafe());
    }

private:
    // preset capacity

    /**
     * @brief Purposed capacity for static space, only used to initially
     * create a shared memory heap. After fist connect/create, the
     * capacity is tracked in the shared memory. Access by
     * staticSpaceCapacity()
     *
     * Aligned to a multiple of unitSize, and >= minStaticSize
     */
    size_t SCap = 0;
    /**
     * @brief Purposed capacity for heap space, only used to initially
     * create a shared memory heap. After fist connect/create, the
     * capacity is tracked in the shared memory. Access by
     * heapCapacity_unsafe()
     *
     * Aligned to a multiple of page size
     */
    size_t HCap = 0;

    // logger
    std::shared_ptr<spdlog::logger> logger;
};

#endif // SHMEM_HEAP_H
