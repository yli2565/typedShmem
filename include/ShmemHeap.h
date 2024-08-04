#ifndef SHMEM_HEAP_H
#define SHMEM_HEAP_H

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <limits>
#include "ShmemUtils.h"
#include "ShmemBase.h"

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

        BlockHeader *removeFromFreeList()
        {
            BlockHeader *bckBlockHeader = this->getBckPtr();
            BlockHeader *fwdBlockHeader = this->getFwdPtr();
            fwdBlockHeader->bckOffset() = bckBlockHeader - fwdBlockHeader;
            bckBlockHeader->fwdOffset() = bckBlockHeader - fwdBlockHeader;
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
    /**
     * @brief Aligned the start of the heap to the nearest 8 bytes
     *
     * @return start of the heap
     */
    Byte *shmalloc(size_t size)
    {
        this->checkConnection();
        if (size < 1)
            return nullptr;

        // Calculate the padding size
        size_t padSize = 0;
        padSize = unitSize - (size % unitSize);
        if (size < 3 * unitSize)
            padSize = 3 * unitSize - size;

        // Header + size + Padding
        size_t requiredSize = unitSize + size + padSize;

        Byte *current = this->shmPtr;

        BlockHeader *best = nullptr;
        size_t bestSize = std::numeric_limits<size_t>::max();

        // Best fit logic
        // TODO: this is just a simple logic, it should be improved to use free block linked list
        while (current - this->shmPtr < this->capacity)
        {
            BlockHeader *currentHeader = reinterpret_cast<BlockHeader *>(current);
            size_t blockSize = currentHeader->size();
            if (currentHeader->A() && blockSize >= requiredSize && blockSize < bestSize)
            {
                best = currentHeader;
                bestSize = blockSize;

                // Break if exact match
                if (blockSize == requiredSize)
                    break;
            }
            current += blockSize;
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
                BlockHeader *newBlockHeader = reinterpret_cast<BlockHeader *>(best + requiredSize);
                BlockHeader *newBlockFooter = reinterpret_cast<BlockHeader *>(best + bestSize - sizeof(BlockHeader));

                // Set busy bit
                newBlockHeader->wait();
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
            if (static_cast<void *>(best + bestSize) < static_cast<void *>(this->shmPtr + this->capacity))
            {
                BlockHeader *nextBlockHeader = best + bestSize;
                nextBlockHeader->val() |= 0b010;
            }

            return reinterpret_cast<Byte *>(best + sizeof(BlockHeader));
        }
    }
    int shfree(Byte *ptr)
    {
        if (ptr == nullptr)
            return -1;

        if (reinterpret_cast<uintptr_t>(ptr) % 8 != 0)
            return -1;

        if (ptr < this->shmPtr || ptr >= this->shmPtr + this->capacity)
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

        // First coalesce with the previous block

        BlockHeader *coalesceTarget = header;

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

        while (reinterpret_cast<uintptr_t>(coalesceTarget) < reinterpret_cast<uintptr_t>(this->shmPtr) + this->capacity && !coalesceTarget->getNextPtr()->A())
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
