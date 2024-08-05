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
         * @brief Provide *(this) as a uintptr_t reference
         * Similar to reinterpret cast BlockHeader ptr to a uintptr_t ptr
         *
         * @return reference to the 8 bytes value at *(this)
         */
        uintptr_t &val()
        {
            return *reinterpret_cast<uintptr_t *>(this);
        }
        uintptr_t &fwdOffset()
        {
            return *(reinterpret_cast<uintptr_t *>(this) + 1);
        }
        uintptr_t &bckOffset()
        {
            return *(reinterpret_cast<uintptr_t *>(this) + 2);
        }

        BlockHeader *getFooterPtr() const
        {
            return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) + this->size() - sizeof(BlockHeader));
        }

        BlockHeader *getFwdPtr() const
        {
            return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) - (this + 1)->size_BPA);
        }

        BlockHeader *getBckPtr() const
        {
            return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) + (this + 2)->size_BPA);
        }
        BlockHeader *getPrevPtr() const
        {
            if (this->P())
                throw std::runtime_error("Previous block is allocated. We should not call this function");
            else
                return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) + (this - 1)->size_BPA);
        }
        BlockHeader *getNextPtr() const
        {
            return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) + this->size());
        }
        void remove()
        {
            if (this->A())
                throw std::runtime_error("Cannot remove allocated block");
            BlockHeader *bckBlockHeader = this->getBckPtr();
            BlockHeader *fwdBlockHeader = this->getFwdPtr();
            fwdBlockHeader->bckOffset() = reinterpret_cast<uintptr_t>(bckBlockHeader) - reinterpret_cast<uintptr_t>(fwdBlockHeader);
            bckBlockHeader->fwdOffset() = fwdBlockHeader->bckOffset();
        }
        void insert(BlockHeader *prevBlock)
        {
            if (prevBlock == nullptr)
            {
                this->fwdOffset() = 0;
                this->bckOffset() = 0;
            }
            else
            {
                BlockHeader *fwdBlockHeader = prevBlock;
                BlockHeader *bckBlockHeader = fwdBlockHeader->getBckPtr();

                // Set busy bit
                // fwdBlockHeader->wait();
                // fwdBlockHeader->val() |= 0b100;
                // bckBlockHeader->wait();
                // bckBlockHeader->val() |= 0b100;

                uintptr_t fwdOffset = this->offset(fwdBlockHeader);
                uintptr_t bckOffset = bckBlockHeader->offset(this);

                // Insert this block after the prevBlock
                this->fwdOffset() = fwdOffset;
                this->bckOffset() = bckOffset;
                fwdBlockHeader->bckOffset() = fwdOffset;
                bckBlockHeader->fwdOffset() = bckOffset;

                // Reset busy bit
                // fwdBlockHeader->val() &= ~0b100;
                // bckBlockHeader->val() &= ~0b100;
            }
        }

        uintptr_t offset(Byte *ptr) const
        {
            if (reinterpret_cast<uintptr_t>(this) < reinterpret_cast<uintptr_t>(ptr))
            {
                throw std::runtime_error("Negative offset");
            }
            return reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(ptr);
        }
        uintptr_t offset(BlockHeader *basePtr) const
        {
            if (reinterpret_cast<uintptr_t>(this) < reinterpret_cast<uintptr_t>(basePtr))
            {
                throw std::runtime_error("Negative offset");
            }
            return reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(basePtr);
        }

        bool B() const
        {
            return size_BPA & 0b100;
        }
        bool P() const
        {
            return size_BPA & 0b010;
        }
        bool A() const
        {
            return size_BPA & 0b001;
        }
        size_t size() const
        {
            return size_BPA & ~0b111;
        }
        int wait(int timeout = -1) const
        {
            while (this->B())
            {
                if (timeout == 0)
                    return ETIMEDOUT;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (timeout > 0)
                    timeout--;
            }
            return 0;
        }
    };
    const size_t unitSize = sizeof(void *);
    // capacity
    size_t SCap = 0;
    size_t HCap = 0;
    // logger
    std::shared_ptr<spdlog::logger> heapLogger;
    ShmemHeap(const std::string &name, size_t staticSpaceSize = DSCap, size_t heapSize = DHCap) : ShmemBase(name)
    {
        // init logger
        this->heapLogger = spdlog::default_logger()->clone("ShmHeap:" + name);
        std::string pattern = "[" + std::to_string(getpid()) + "] [%n] %v";
        this->heapLogger->set_pattern(pattern);

        size_t pageSize = sysconf(_SC_PAGESIZE);

        if (pageSize % 8 != 0)
            throw std::runtime_error("Page size must be a multiple of 8");

        // pad static space capacity to a multiple of 8
        size_t newStaticSpaceCapacity = (staticSpaceSize + unitSize) & ~unitSize;

        // pad heap capacity to a multiple of page size
        size_t newHeapCapacity = (heapSize + pageSize - 1) & ~(pageSize - 1);

        this->heapLogger->info("Initialized to: Static space capacity: {} heap capacity: {}", newStaticSpaceCapacity, newHeapCapacity);

        this->SCap = newStaticSpaceCapacity;
        this->HCap = newHeapCapacity;

        this->setCapacity(newStaticSpaceCapacity + newHeapCapacity);
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
    }

    void resize(size_t newCapacity, bool keepContent = true)
    {
        if (!keepContent)
            this->heapLogger->warn("Resizing without content is not allowed in ShmHeap, the keepContent parameter is ignored");
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
        checkConnection();
        return reinterpret_cast<size_t *>(this->shmPtr)[0];
    }
    size_t &heapCapacity()
    {
        checkConnection();
        return reinterpret_cast<size_t *>(this->shmPtr)[1];
    }
    size_t &freeBlockListHead()
    {
        checkConnection();
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
        return reinterpret_cast<BlockHeader *>(this->heapHead() + this->freeBlockListHead());
    }
    void removeFreeBlock(BlockHeader *block)
    {
        if (this->freeBlockListHead() == reinterpret_cast<Byte *>(block) - this->heapHead())
        {
            if (block->getBckPtr() == block)
            {
                // Block is the only element in the list
                this->freeBlockListHead() = NPtr; // NPtr is an impossible byte offset of a block
            }
            else
            {
                this->freeBlockListHead() = reinterpret_cast<Byte *>(block->getBckPtr()) - this->heapHead();
            }
        }
        block->remove();
    }
    void insertFreeBlock(BlockHeader *block, BlockHeader *prevBlock = nullptr)
    {
        if (this->freeBlockListHead() == NPtr)
        {
            // Don't care the previous block when the list is empty, it must be an outdated block
            this->freeBlockListHead() = reinterpret_cast<Byte *>(block) - this->heapHead();
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
        padSize = size % 8 == 0 ? 0 : unitSize - (size % unitSize);
        // The payload should be at least 3 units
        if (size < 3 * unitSize)
            padSize = 3 * unitSize - size;

        // Header + size + Padding
        size_t requiredSize = unitSize + size + padSize;

        BlockHeader *current = reinterpret_cast<BlockHeader *>(this->heapHead());

        BlockHeader *best = nullptr;
        size_t bestSize = std::numeric_limits<size_t>::max();

        // Best fit logic
        // TODO: this is just a simple logic, it should be improved to use free block linked list
        while (reinterpret_cast<Byte *>(current) < this->heapTail())
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
            current = current->getNextPtr();
        }

        if (best != nullptr)
        {
            // Lock the best fit block (Busy bit <- 1)
            best->wait();
            best->val() |= 0b100;

            // Check if the best size < required size + 4
            // In that case, we cannot split the block as a free block is minimum 4 bytes, we just increase the required size
            if (bestSize < requiredSize + 4)
            {
                requiredSize = bestSize;
            }

            // uintptr_t fwdOffset = best->fwdOffset();
            // uintptr_t bckOffset = best->bckOffset();

            BlockHeader *fwdBlockHeader = best->getFwdPtr();
            // BlockHeader *bckBlockHeader = best->getBckPtr();

            // // Set busy bit
            // fwdBlockHeader->wait();
            // fwdBlockHeader->val() |= 0b100;
            // bckBlockHeader->wait();
            // bckBlockHeader->val() |= 0b100;

            // Set allocated bit to 0
            best->val() &= ~0b001;
            // Remove the best block from free list
            this->removeFreeBlock(best);

            // If the block we find is bigger than the required size, split it
            if (bestSize > requiredSize)
            {
                BlockHeader *newBlockHeader = reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(best) + requiredSize);
                BlockHeader *newBlockFooter = reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(best) + bestSize - sizeof(BlockHeader));

                // update the new block
                // Size: bestSize - requiredSize; Busy: 1; Previous Allocated: 1; Allocated: 0
                newBlockHeader->val() = (bestSize - requiredSize) | 0b010;

                this->insertFreeBlock(newBlockHeader, fwdBlockHeader);

                // Reset busy bit
                newBlockHeader->val() &= ~0b100;
            }

            // Reset busy bit
            // fwdBlockHeader->val() &= ~0b100;
            // bckBlockHeader->val() &= ~0b100;

            // update the best fit block
            // Size: requiredSize; Busy: 0; Previous Allocated: not changed; Allocated: 1
            best->val() = requiredSize & ~0b100 | (best->size_BPA & 0b010) | 0b001;

            // update next block's p bit
            if (reinterpret_cast<Byte *>(best->getNextPtr()) < this->heapTail())
            {
                best->getNextPtr()->val() |= 0b010;
            }

            // Return the offset from the heap head (+1 make the offset is on start of payload)
            return (best + 1)->offset(this->heapHead());
        }
        else
        {
            return 0;
        }
    }
    int shfree(Byte *ptr)
    {
        if (ptr == nullptr)
            return -1;

        if (reinterpret_cast<uintptr_t>(ptr) % unitSize != 0)
            return -1;

        if (ptr < this->heapHead() || ptr >= this->heapTail())
            return -1;

        BlockHeader *header = reinterpret_cast<BlockHeader *>(ptr) - 1;

        // Set Busy bit
        header->wait();
        header->val() |= 0b100;

        if (!header->A())
            return -1;

        // Set Allocated bit to 0
        header->val() &= ~0b001;

        this->insertFreeBlock(header);

        // Immediate coalescing
        BlockHeader *coalesceTarget = header;

        // First coalesce with the previous block

        // When coalescing with previous block, footer is unchanged
        BlockHeader *newFooter = header->getFooterPtr();

        while (!coalesceTarget->P())
        { // Each iteration coalesce two adjacent blocks
            BlockHeader *prevBlockHeader = coalesceTarget->getPrevPtr();
            if (prevBlockHeader->A())
                throw std::runtime_error("previous block is allocated, but following block's P bit is set");

            // Wait Busy bit
            prevBlockHeader->wait();

            size_t newSize = prevBlockHeader->size() + sizeof(BlockHeader) + coalesceTarget->size();

            // Size: newSize; Busy: 1; Previous Allocated: not changed; Allocated: 0
            prevBlockHeader->size_BPA = newSize & 0b100 | (prevBlockHeader->size_BPA & 0b010) & ~0b001;
            newFooter->val() = newSize;

            // update free list ptr
            this->removeFreeBlock(coalesceTarget);
            this->insertFreeBlock(prevBlockHeader);

            coalesceTarget = prevBlockHeader;
        }

        while (reinterpret_cast<Byte *>(coalesceTarget->getNextPtr()) < this->heapTail() && !coalesceTarget->getNextPtr()->A())
        {
            BlockHeader *nextBlockHeader = coalesceTarget->getNextPtr();

            // Wait Busy bit
            nextBlockHeader->wait();

            newFooter = nextBlockHeader->getFooterPtr();

            size_t newSize = coalesceTarget->size() + nextBlockHeader->size() + sizeof(BlockHeader);

            // Size: newSize; Busy: 1; Previous Allocated: not changed; Allocated: 0
            coalesceTarget->size_BPA = newSize | (coalesceTarget->size_BPA & 0b111);
            newFooter->val() = newSize;

            // update free list ptr
            this->removeFreeBlock(nextBlockHeader);

            // coalesceTarget unchanged;
        }
        // Reset Busy bit
        coalesceTarget->val() &= ~0b100;

        return 0;
    }
    void printShmHeap()
    {
        size_t staticCapacity = this->staticCapacity();
        size_t heapCapacity = this->heapCapacity();
        size_t firstFreeBlockOffset = this->freeBlockListHead();
        Byte *heapHead = this->heapHead();
        Byte *heapTail = this->heapTail();
        this->heapLogger->info("********************************* Static Space ****************************");
        this->heapLogger->info("Static Space Capacity: {}", staticCapacity);
        this->heapLogger->info("Heap Capacity: {}", heapCapacity);
        this->heapLogger->info("Free block list offset: {}", firstFreeBlockOffset);
        this->heapLogger->info("********************************** Block List *****************************");
        // this->heapLogger->info("Offset\tStatus\tPrev\tBusy\tt_Begin\tt_End\tt_Size");
        this->heapLogger->info("{:<10} {:<6} {:<6} {:<6} {:<14} {:<14} {:<6}", "Offset", "Status", "Prev", "Busy", "Begin", "End", "Size");

        uintptr_t begin, end, size;
        BlockHeader *current = reinterpret_cast<BlockHeader *>(heapHead);
        while (reinterpret_cast<Byte *>(current) < heapTail)
        {
            begin = reinterpret_cast<uintptr_t>(current);
            end = begin + current->size();
            size = end - begin;
            this->heapLogger->info("{:#010x} {:<6d} {:<6d} {:<6d} {:#014x} {:#014x} {:<6d}",
                                   reinterpret_cast<Byte *>(current) - heapHead,
                                   current->A() ? 1 : 0,
                                   current->P() ? 1 : 0,
                                   current->B() ? 1 : 0,
                                   begin,
                                   end,
                                   size);
            current = current->getNextPtr();
        }
        this->heapLogger->info("---------------------------------------------------------------------------");
    }
};

#endif // SHMEM_HEAP_H
