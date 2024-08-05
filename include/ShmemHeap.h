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
    struct BlockHeader
    {
        /**
         * size of the block / Busy bit / Previous Allocated bit / Allocated bit
         */
        size_t size_BPA;

        /**
         * @brief Provide {size | B bit | P bit | A bit} as a uintptr_t reference
         * Similar to reinterpret cast BlockHeader ptr to a uintptr_t ptr
         *
         * @return reference to the 8 bytes {size | B bit | P bit | A bit} at *(this)
         */
        uintptr_t &val();

        /**
         * @brief For free block, return the offset to the previous free block in linked list
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

        this->setStaticCapacity(staticSpaceSize);
        this->setHeapCapacity(heapSize);
        this->logger->info("Initialized to: Static space capacity: {} heap capacity: {}", this->SCap, this->HCap);

        this->setCapacity(this->SCap + this->HCap);
    }
    void create()
    {
        if (this->SCap < 3 || this->HCap == 0)
        {
            throw std::runtime_error("Capacity is too small to hold static space or heap space");
        }
        ShmemBase::create();

        // Init the static information
        this->staticCapacity() = this->SCap;
        this->heapCapacity() = this->HCap;

        BlockHeader *firstBlock = reinterpret_cast<BlockHeader *>(this->heapHead());

        // Prev allocated bit set to 1
        firstBlock->val() = this->HCap | 0b010;
        firstBlock->getFooterPtr()->val() = this->HCap;
        this->freeBlockListOffset() = reinterpret_cast<Byte *>(firstBlock) - this->heapHead();
    }

    void resize(size_t newCapacity, bool keepContent = true)
    {
        if (!keepContent)
            this->logger->warn("Resizing without content is not allowed in ShmHeap, the keepContent parameter is ignored");
        if (newCapacity < this->staticCapacity())
        {
            throw std::runtime_error("Capacity is too small to hold static space");
        }
        this->resize(this->staticCapacity(), newCapacity - this->staticCapacity());
    };

    /**
     * @brief Resize both static and heap space
     *
     * @param staticSpaceSize required size of static space, might be padded. -1 means not changed
     * @param heapSize required size of heap space, might be padded. -1 means not changed
     */
    void resize(size_t staticSpaceSize, size_t heapSize)
    {
        if (staticSpaceSize == -1)
        {
            staticSpaceSize = this->staticCapacity();
        }
        else
        {
            if (staticSpaceSize < this->staticCapacity())
                throw std::runtime_error("Shrinking static space is not supported");

            if (staticSpaceSize < 3)
            {
                throw std::runtime_error("Static space size must be at least 3 to store important metrics");
            }
        }
        if (heapSize == -1)
        {
            heapSize = this->heapCapacity();
        }
        else
        {
            if (heapSize < this->heapCapacity())
                throw std::runtime_error("Shrinking heap space is not supported");
        }

        size_t pageSize = sysconf(_SC_PAGESIZE);

        if (pageSize % 8 != 0)
            throw std::runtime_error("Page size must be a multiple of 8");

        if (this->staticCapacity() > staticSpaceSize)
            throw std::runtime_error("Shrinking static space is not supported");

        // pad static space capacity to a multiple of 8
        size_t newStaticSpaceCapacity = (staticSpaceSize + unitSize) & ~unitSize;

        // pad heap capacity to a multiple of page size
        size_t newHeapCapacity = (heapSize + pageSize - 1) & ~(pageSize - 1);

        // Save everything
        size_t oldStaticSpaceCapacity = this->staticCapacity();
        size_t oldHeapCapacity = this->heapCapacity();
        Byte *tempStaticSpace = new Byte[oldStaticSpaceCapacity];
        std::memcpy(tempStaticSpace, this->shmPtr, this->staticCapacity());
        Byte *tempHeap = new Byte[oldHeapCapacity];
        std::memcpy(tempHeap, this->heapHead(), this->heapCapacity());

        // resize static space (without copy content)
        ShmemBase::resize(newStaticSpaceCapacity + this->heapCapacity(), false);

        // Restore everything
        this->staticCapacity() = newStaticSpaceCapacity;
        this->heapCapacity() = newHeapCapacity;
        std::memcpy(this->shmPtr, tempStaticSpace, oldStaticSpaceCapacity);
        std::memcpy(this->heapHead(), tempHeap, oldHeapCapacity);

        delete[] tempStaticSpace;
        delete[] tempHeap;
    }

    size_t &staticCapacity()
    {
        return reinterpret_cast<size_t *>(this->shmPtr)[0];
    }
    size_t &heapCapacity()
    {
        return reinterpret_cast<size_t *>(this->shmPtr)[1];
    }
    size_t &freeBlockListOffset()
    {
        return reinterpret_cast<size_t *>(this->shmPtr)[2];
    }
    Byte *heapHead()
    {
        return this->shmPtr + this->staticCapacity();
    }
    Byte *heapTail()
    {
        return this->shmPtr + this->staticCapacity() + this->heapCapacity();
    }
    BlockHeader *freeBlockList()
    {
        return reinterpret_cast<BlockHeader *>(this->heapHead() + this->freeBlockListOffset());
    }
    void removeFreeBlock(BlockHeader *block)
    {
        if (this->freeBlockListOffset() == reinterpret_cast<Byte *>(block) - this->heapHead())
        {
            if (block->getBckPtr() == block)
            {
                // Block is the only element in the list
                this->freeBlockListOffset() = NPtr; // NPtr is an impossible byte offset of a block
            }
            else
            {
                this->freeBlockListOffset() = reinterpret_cast<Byte *>(block->getBckPtr()) - this->heapHead();
            }
        }
        block->remove();
    }
    void insertFreeBlock(BlockHeader *block, BlockHeader *prevBlock = nullptr)
    {
        if (this->freeBlockListOffset() == NPtr)
        {
            // Don't care the previous block when the list is empty, it must be an outdated block
            this->freeBlockListOffset() = reinterpret_cast<Byte *>(block) - this->heapHead();
            block->insert(nullptr);
        }
        else
        {
            if (prevBlock == nullptr)
                block->insert(this->freeBlockList());
            else
                block->insert(prevBlock);
        }
    }
    /**
     * @brief Aligned the start of the heap to the nearest 8 bytes
     *
     * @return start of the heap
     */
    uintptr_t shmalloc(size_t size)
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

        BlockHeader *listHead = this->freeBlockList();
        BlockHeader *current = listHead;

        BlockHeader *best = nullptr;
        size_t bestSize = std::numeric_limits<size_t>::max();

        // Best fit logic
        // TODO: this is just a simple logic, it should be improved to use free block linked list
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
            if (bestSize < requiredSize + 4)
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
            if (reinterpret_cast<Byte *>(best->getNextPtr()) < this->heapTail())
            {
                best->getNextPtr()->setP(true);
            }

            // Return the offset from the heap head (+1 make the offset is on start of payload)
            return reinterpret_cast<Byte *>(best + 1) - this->heapHead();
        }
        else
        {
            return 0;
        }
    }
    int shfree(uintptr_t offset)
    {
        this->checkConnection();
        return this->shfree(this->heapHead() + offset);
    }
    int shfree(Byte *ptr)
    {
        this->checkConnection();

        if (ptr < this->heapHead() || ptr >= this->heapTail())
            return -1;

        // Since there must be a header, a payload ptr should be at least unitSize bytes
        if (ptr - this->heapHead() < unitSize)
            return -1;

        if ((ptr - this->heapHead()) % unitSize != 0)
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
        if (reinterpret_cast<Byte *>(header->getNextPtr()) < this->heapTail())
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

        while (reinterpret_cast<Byte *>(coalesceTarget->getNextPtr()) < this->heapTail() && !coalesceTarget->getNextPtr()->A())
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
        size_t staticCapacity = this->staticCapacity();
        size_t heapCapacity = this->heapCapacity();
        size_t firstFreeBlockOffset = this->freeBlockListOffset();
        Byte *heapHead = this->heapHead();
        Byte *heapTail = this->heapTail();
        std::vector<BlockHeader *> freeBlockList = {};
        if (this->freeBlockListOffset() != NPtr)
        {
            freeBlockList.push_back(this->freeBlockList());
            BlockHeader *current = this->freeBlockList()->getBckPtr();
            while (current != this->freeBlockList())
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
            begin = reinterpret_cast<uintptr_t>(current);
            end = begin + current->size();
            size = end - begin;
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
    // Setters
    void setHeapCapacity(size_t size)
    {
        size_t pageSize = sysconf(_SC_PAGESIZE);

        // pad heap capacity to a multiple of page size
        size_t newHeapCapacity = (size + pageSize - 1) & ~(pageSize - 1);
        this->HCap = newHeapCapacity;
        this->logger->info("Request heap size: {}, new heap capacity: {}", size, newHeapCapacity);
    }

    void setStaticCapacity(size_t size)
    { // pad static space capacity to a multiple of 8
        size_t newStaticSpaceCapacity = (size + unitSize) & ~unitSize;

        this->SCap = newStaticSpaceCapacity;
        this->logger->info("Request static space size: {}, new static space capacity: {}", size, newStaticSpaceCapacity);
    }

    // Utility Functions
    std::shared_ptr<spdlog::logger> &getLogger()
    {
        return this->logger;
    }

private:
    // capacity
    size_t SCap = 0;
    size_t HCap = 0;

    // logger
    std::shared_ptr<spdlog::logger> logger;
};

#endif // SHMEM_HEAP_H
