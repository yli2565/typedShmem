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
        uintptr_t &getVal()
        {
            return *reinterpret_cast<uintptr_t *>(this);
        }
        uintptr_t &getFwdOffset()
        {
            return *(reinterpret_cast<uintptr_t *>(this) + 1);
        }
        uintptr_t &getBckOffset()
        {
            return *(reinterpret_cast<uintptr_t *>(this) + 2);
        }

        BlockHeader *getFwdPtr() const
        {
            return reinterpret_cast<BlockHeader *>(this->size_BPA - (this + 1)->size_BPA);
        }

        BlockHeader *getBckPtr() const
        {
            return reinterpret_cast<BlockHeader *>(this->size_BPA + (this + 2)->size_BPA);
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

            uintptr_t bestOffset = reinterpret_cast<uintptr_t>(best) - reinterpret_cast<uintptr_t>(shmPtr);
            // If the block we find is bigger than the required size, split it
            if (bestSize > requiredSize)
            {
                BlockHeader *newBlockHeader = reinterpret_cast<BlockHeader *>(best + requiredSize);
                BlockHeader *newBlockFooter = reinterpret_cast<BlockHeader *>(best + bestSize - sizeof(BlockHeader));

                // Just for simplicity, .size_BPA is equivalent to static_cast<uintptr_t>(*(best + 1))
                uintptr_t fwdOffset = best->getFwdOffset();
                uintptr_t bckOffset = best->getBckOffset();

                BlockHeader *fwdBlockHeader = best->getFwdPtr();
                BlockHeader *bckBlockHeader = best->getBckPtr();

                // If the fwd or bck block is best block itself, update to the new free block
                if (fwdOffset == 0)
                    fwdBlockHeader = newBlockHeader;
                if (bckOffset == 0)
                    bckBlockHeader = newBlockHeader;

                // update the new block
                // Size: bestSize - requiredSize; Busy: 1; Previous Allocated: 1; Allocated: 0
                newBlockHeader->getVal() = (bestSize - requiredSize) | 0b010;
                newBlockHeader->getFwdOffset() = newBlockHeader - fwdBlockHeader;
                newBlockHeader->getBckOffset() = bckBlockHeader - newBlockHeader;
                newBlockFooter->getVal() = bestSize - requiredSize;

                // update fwd and bck block's ptr
                fwdBlockHeader->getBckOffset() = newBlockHeader - fwdBlockHeader;
                bckBlockHeader->getFwdOffset() = bckBlockHeader - newBlockHeader;
            }

            // update the best bit block
            
        }
        void shfree(Byte * ptr);
    }
};

#endif // SHMEM_HEAP_H
