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
                fwdBlockHeader->wait();
                fwdBlockHeader->val() |= 0b100;
                bckBlockHeader->wait();
                bckBlockHeader->val() |= 0b100;

                uintptr_t fwdOffset = this->offset(fwdBlockHeader);
                uintptr_t bckOffset = bckBlockHeader->offset(this);

                // Insert this block after the prevBlock
                this->fwdOffset() = fwdOffset;
                this->bckOffset() = bckOffset;
                fwdBlockHeader->bckOffset() = fwdOffset;
                bckBlockHeader->fwdOffset() = bckOffset;

                // Reset busy bit
                fwdBlockHeader->val() &= ~0b100;
                bckBlockHeader->val() &= ~0b100;
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
            return size_BPA & 0b1;
        }
        bool P() const
        {
            return size_BPA & 0b10;
        }
        bool A() const
        {
            return size_BPA & 0b100;
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
    ShmemHeap(const std::string &name, size_t staticSpaceSize = DSCap, size_t heapSize = DHCap) : ShmemBase(name)
    {
        size_t pageSize = sysconf(_SC_PAGESIZE);

        if (pageSize % 8 != 0)
            throw std::runtime_error("Page size must be a multiple of 8");

        if (this->staticCapacity() > staticSpaceSize)
            throw std::runtime_error("Shrinking static space is not supported");

        // pad static space capacity to a multiple of 8
        size_t newStaticSpaceCapacity = (staticSpaceSize + unitSize) & ~unitSize;

        // pad heap capacity to a multiple of page size
        size_t newHeapCapacity = (heapSize + pageSize - 1) & ~(pageSize - 1);

        this->setCapacity(newStaticSpaceCapacity + newHeapCapacity);
    }
    /**
     * @brief Resize both static and heap space
     *
     * @param staticSpaceSize required size of static space, might be padded. -1 means not changed
     * @param heapSize required size of heap space, might be padded. -1 means not changed
     */
    void resizeSH(size_t staticSpaceSize, size_t heapSize)
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
            if (prevBlock != nullptr)
                throw std::runtime_error("The list is empty, but provided with a previous block which is not in the list");
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
        padSize = unitSize - (size % unitSize);
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
            if (current->A() && blockSize >= requiredSize && blockSize < bestSize)
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

            uintptr_t fwdOffset = best->fwdOffset();
            uintptr_t bckOffset = best->bckOffset();

            BlockHeader *fwdBlockHeader = best->getFwdPtr();
            BlockHeader *bckBlockHeader = best->getBckPtr();

            // Set busy bit
            fwdBlockHeader->wait();
            fwdBlockHeader->val() |= 0b100;
            bckBlockHeader->wait();
            bckBlockHeader->val() |= 0b100;

            // If the block we find is bigger than the required size, split it
            if (bestSize > requiredSize)
            {
                BlockHeader *newBlockHeader = reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(best) + requiredSize);
                BlockHeader *newBlockFooter = reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(best) + bestSize - sizeof(BlockHeader));

                // Set busy bit (No need to wait as it's a new block)
                newBlockHeader->val() |= 0b100;

                // If the fwd or bck block is best block itself, update to the new free block
                if (fwdOffset == 0)
                    fwdBlockHeader = newBlockHeader;
                if (bckOffset == 0)
                    bckBlockHeader = newBlockHeader;

                // update the new block
                // Size: bestSize - requiredSize; Busy: 1; Previous Allocated: 1; Allocated: 0
                newBlockHeader->val() = (bestSize - requiredSize) | 0b110;
                newBlockHeader->fwdOffset() = newBlockHeader - fwdBlockHeader;
                newBlockHeader->bckOffset() = bckBlockHeader - newBlockHeader;
                newBlockFooter->val() = bestSize - requiredSize;

                // update fwd and bck block's ptr
                fwdBlockHeader->bckOffset() = newBlockHeader - fwdBlockHeader;
                bckBlockHeader->fwdOffset() = bckBlockHeader - newBlockHeader;

                // Reset busy bit
                newBlockHeader->val() &= ~0b100;
            }
            else
            {
                // Remove the best block from the double linked list
                fwdBlockHeader->bckOffset() = bckBlockHeader - fwdBlockHeader;
                bckBlockHeader->fwdOffset() = bckBlockHeader - fwdBlockHeader;
            }

            // Reset busy bit
            fwdBlockHeader->val() &= ~0b100;
            bckBlockHeader->val() &= ~0b100;

            // update the best fit block
            // Size: requiredSize; Busy: 0; Previous Allocated: not changed; Allocated: 1
            best->val() = requiredSize & ~0b100 | (best->size_BPA & 0b010) | 0b001;

            // update next block's p bit
            if (reinterpret_cast<Byte *>(best->getNextPtr()) < this->heapTail())
            {
                best->getNextPtr()->val() |= 0b010;
            }

            // Return the offset from the heap head
            return reinterpret_cast<Byte *>(best) + sizeof(BlockHeader) - this->heapHead();
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

        if (reinterpret_cast<uintptr_t>(ptr) % 8 != 0)
            return -1;

        if (ptr < this->heapHead() || ptr >= this->heapTail())
            return -1;

        BlockHeader *header = reinterpret_cast<BlockHeader *>(ptr) - sizeof(BlockHeader);

        // Set Busy bit
        header->wait();
        header->val() |= 0b100;

        if (!header->A())
            return -1;

        // Set Allocated bit to 0
        header->val() &= ~0b001;

        // Immediate coalescing
        BlockHeader *coalesceTarget = header;

        // First coalesce with the previous block

        // When coalescing with previous block, footer is unchanged
        BlockHeader *newFooter = header->getFooterPtr();

        while (!coalesceTarget->P())
        {
            BlockHeader *prevBlockHeader = coalesceTarget->getPrevPtr();
            if (prevBlockHeader->A())
                throw std::runtime_error("previous block is allocated, but following block's P bit is set");

            // Set Busy bit
            prevBlockHeader->wait();
            prevBlockHeader->val() |= 0b100;

            size_t newSize = prevBlockHeader->size() + sizeof(BlockHeader) + coalesceTarget->size();

            // Size: newSize; Busy: 1; Previous Allocated: not changed; Allocated: 0
            prevBlockHeader->size_BPA = newSize & 0b100 | (prevBlockHeader->size_BPA & 0b010) & ~0b001;
            newFooter->val() = newSize;

            // update free list ptr
            prevBlockHeader->removeFromFreeList();

            coalesceTarget = prevBlockHeader;
        }

        while (reinterpret_cast<uintptr_t>(coalesceTarget->getNextPtr()) < reinterpret_cast<uintptr_t>(this->heapTail()) && !coalesceTarget->getNextPtr()->A())
        {
            BlockHeader *nextBlockHeader = coalesceTarget->getNextPtr();

            // Set Busy bit
            nextBlockHeader->wait();
            nextBlockHeader->val() |= 0b100;

            newFooter = nextBlockHeader->getFooterPtr();

            size_t newSize = coalesceTarget->size() + nextBlockHeader->size() + sizeof(BlockHeader);

            // Size: newSize; Busy: 1; Previous Allocated: not changed; Allocated: 0
            coalesceTarget->size_BPA = newSize | (coalesceTarget->size_BPA & 0b111);
            newFooter->val() = newSize;

            // update free list ptr
            nextBlockHeader->removeFromFreeList();

            // coalesceTarget unchanged;
        }

        return 0;
    }
};

#endif // SHMEM_HEAP_H
