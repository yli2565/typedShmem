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
            size_t blockSize = currentHeader->size_BPA & ~0b111;
            if (currentHeader->size_BPA & 0b001 && blockSize >= requiredSize && blockSize < bestSize)
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
            // If the block we find is bigger than the required size, split it
            if (bestSize > requiredSize)
            {
                BlockHeader *newBlockHeader = reinterpret_cast<BlockHeader *>(best + requiredSize);
                BlockHeader *newBlockFooter = reinterpret_cast<BlockHeader *>(best + bestSize - sizeof(BlockHeader));
                // Size: bestSize - requiredSize; Busy: 1; Previous Allocated: 1; Allocated: 0
                newBlockHeader->size_BPA = (bestSize - requiredSize) | 0b010;

                void *fwdPtr = reinterpret_cast<void *>(best + 1);
                void *bckPtr = reinterpret_cast<void *>(best + 2);
            }
        }
        void shfree(Byte * ptr);

    private:
    };

#endif // SHMEM_HEAP_H
