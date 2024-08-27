#include "ShmemHeap.h"
#include <cstring>
#include <stdexcept>

ShmemHeap::ShmemHeap(const std::string &name, size_t staticSpaceSize, size_t heapSize)
    : ShmemBase(name)
{
    // Disable the ShmemBase logger
    this->ShmemBase::getLogger()->set_level(spdlog::level::err);

    // init logger
    this->logger = spdlog::default_logger()->clone("ShmHeap:" + name);
    std::string pattern = "[" + std::to_string(getpid()) + "] [%n] %v";
    this->logger->set_pattern(pattern);

    this->setSCap(staticSpaceSize);
    this->setHCap(heapSize);
    this->logger->info("Initialized to: Static space capacity: {} heap capacity: {}", this->SCap, this->HCap);

    this->setCapacity(this->SCap + this->HCap);
}

void ShmemHeap::create()
{
    if (this->SCap < this->minStaticSize || this->HCap == 0)
    {
        if (this->SCap < this->minStaticSize)
            this->logger->error("SCap is too small. SCap: {} < minStaticSize: {}", this->SCap, this->minStaticSize);
        if (this->HCap == 0)
            this->logger->error("HCap is too small. HCap: {}", this->HCap);
        throw std::runtime_error("Capacity is too small to hold static space or heap space");
    }
    ShmemBase::create();

    // Init the static information
    this->staticCapacity_unsafe() = this->SCap;
    this->heapCapacity_unsafe() = this->HCap;

    // Init the heap
    this->entranceOffset_unsafe() = NPtr;

    BlockHeader *firstBlock = reinterpret_cast<BlockHeader *>(this->heapHead_unsafe());

    // Prev allocated bit set to 1
    firstBlock->val() = this->HCap | 0b010;
    firstBlock->getFooterPtr()->val() = this->HCap;
    this->freeBlockListOffset_unsafe() = reinterpret_cast<Byte *>(firstBlock) - this->heapHead_unsafe();

    this->logger->info("Shared memory heap created. Static space capacity: {} heap capacity: {}", this->SCap, this->HCap);
}

void ShmemHeap::resize(size_t heapSize)
{
    this->checkConnection();
    this->resize(this->staticCapacity_unsafe(), heapSize);
}

void ShmemHeap::resize(size_t staticSpaceSize, size_t heapSize)
{
    this->checkConnection();
    if (staticSpaceSize == static_cast<size_t>(-1))
    { // Don't change static space capacity
        this->setSCap(this->staticCapacity_unsafe());
    }
    else
    {
        this->setSCap(staticSpaceSize);
    }
    if (heapSize == static_cast<size_t>(-1))
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
        this->logger->info("Resized to: Static space capacity: {}->{} heap capacity: {}->{}. Additional heap space({} Byte) is merged into last block", oldStaticSpaceCapacity, newStaticSpaceCapacity, oldHeapCapacity, newHeapCapacity, newHeapCapacity - oldHeapCapacity);
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

        this->logger->info("Resized to: Static space capacity: {}->{} heap capacity: {}->{}. Additional heap space({} Byte) is convert to a new free block", oldStaticSpaceCapacity, newStaticSpaceCapacity, oldHeapCapacity, newHeapCapacity, newHeapCapacity - oldHeapCapacity);
    }

    delete[] tempStaticSpace;
    delete[] tempHeap;
}

size_t &ShmemHeap::staticCapacity()
{
    checkConnection();
    return reinterpret_cast<size_t *>(this->shmPtr)[0];
}

size_t &ShmemHeap::heapCapacity()
{
    checkConnection();
    return reinterpret_cast<size_t *>(this->shmPtr)[1];
}

size_t &ShmemHeap::freeBlockListOffset()
{
    checkConnection();
    return reinterpret_cast<size_t *>(this->shmPtr)[2];
}

size_t &ShmemHeap::entranceOffset()
{
    checkConnection();
    return entranceOffset_unsafe();
}

Byte *ShmemHeap::staticSpaceHead()
{
    checkConnection();
    return this->shmPtr;
}

Byte *ShmemHeap::entrance()
{
    checkConnection();
    return this->entrance_unsafe();
}

Byte *ShmemHeap::heapHead()
{
    checkConnection();
    return this->shmPtr + this->staticCapacity_unsafe();
}

Byte *ShmemHeap::heapTail()
{
    checkConnection();
    return this->shmPtr + this->staticCapacity_unsafe() + this->heapCapacity_unsafe();
}

ShmemHeap::BlockHeader *ShmemHeap::freeBlockList()
{
    checkConnection();
    return reinterpret_cast<BlockHeader *>(this->heapHead_unsafe() + this->freeBlockListOffset_unsafe());
}

size_t ShmemHeap::shmalloc(size_t size)
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
        // BlockHeader* bckBlockHeader = best->getBckPtr();

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

            this->logger->debug("shmalloc(size={}) split the best fit block at offset {}: {}E->{}A+{}E", size, reinterpret_cast<Byte *>(best) - this->heapHead_unsafe(), bestSize, requiredSize, bestSize - requiredSize);
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
        size_t resultOffset = reinterpret_cast<Byte *>(best + 1) - this->heapHead_unsafe();

        this->logger->info("shmalloc(size={}) succeeded. Payload Offset: {}, Block Size: {}", size, resultOffset, requiredSize);

        return resultOffset;
    }
    else
    {
        this->logger->info("shmalloc(size={}) failed", size);
        return 0;
    }
}

size_t ShmemHeap::shrealloc(size_t offset, size_t size)
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

    // Set Busy bit
    header->wait();
    header->setB(true);

    // TODO: check if it is a valid header

    // Header + size + Padding
    size_t requiredSize = pad(size + sizeof(BlockHeader), unitSize);

    // At least 4 bytes for a block
    if (requiredSize < 4 * unitSize)
        requiredSize = 4 * unitSize;

    size_t oldSize = header->size();

    if (oldSize == requiredSize)
    {
        this->logger->debug("shrealloc(offset={}, size={}) succeeded. Current block size: {} already satisfied the requirement", offset, size, oldSize);
        
        header->setB(false);
        return offset;
    }
    else if (oldSize > requiredSize)
    {
        if (oldSize - 4 * unitSize < requiredSize)
        {
            // a free block is at least 4 bytes, we cannot split the block
            // TODO: the following block is a free block, we can merge the bytes into it
            this->logger->warn("A reallocation request is ignored as a free block is at least 4 bytes (Not enough space to split a free block). Request size: {}, old size: {}", requiredSize, oldSize);
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

            this->logger->debug("shrealloc(offset={}, size={}) shrink current block from: {}A->{}A+{}E", offset, size, oldSize, requiredSize, oldSize - requiredSize);
        }

        header->setB(false);
        return offset;
    }
    else
    { // The new size is larger than the old size
        size_t oldPayloadSize = oldSize - sizeof(BlockHeader);

        Byte *tempPayload = new Byte[oldPayloadSize];
        // Copy the payload
        std::memcpy(tempPayload, this->heapHead_unsafe() + offset, oldPayloadSize);

        header->setB(false); // shrealloc is done with the current block
        // Free the old payload
        this->shfree(offset);

        // Re-allocate the new payload
        size_t newPayloadOffset = this->shmalloc(size);

        std::memcpy(this->heapHead_unsafe() + newPayloadOffset, tempPayload, oldPayloadSize);

        delete[] tempPayload;

        this->logger->debug("shrealloc(offset={}, size={}) move payload offset: {}->{}, block size: {}->{}", offset, size, offset, newPayloadOffset, oldSize, requiredSize);


        return newPayloadOffset;
    }
}

int ShmemHeap::shfree(size_t offset)
{
    this->checkConnection();
    return this->shfreeHelper(this->heapHead_unsafe() + offset);
}

// Debug
void ShmemHeap::printShmHeap()
{
    checkConnection();
    size_t staticCapacity = this->staticCapacity_unsafe();
    size_t heapCapacity = this->heapCapacity_unsafe();
    size_t firstFreeBlockOffset = this->freeBlockListOffset_unsafe();
    size_t entranceOffset = this->entranceOffset_unsafe();
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
    this->logger->info("Free block list offset: {}", firstFreeBlockOffset == NPtr ? "NPtr" : std::to_string(firstFreeBlockOffset));
    this->logger->info("Entrance offset: {}", entranceOffset == NPtr ? "null" : std::to_string(entranceOffset));
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

std::vector<size_t> ShmemHeap::briefLayout()
{
    checkConnection();
    std::vector<size_t> layout = {};

    Byte *heapHead = this->heapHead_unsafe();
    Byte *heapTail = this->heapTail_unsafe();

    BlockHeader *current = reinterpret_cast<BlockHeader *>(heapHead);
    while (reinterpret_cast<Byte *>(current) < heapTail)
    {
        layout.push_back(current->size() - sizeof(BlockHeader));
        current = current->getNextPtr();
    }

    return layout;
}

std::string ShmemHeap::briefLayoutStr()
{
    checkConnection();
    // Get the raw layout
    std::vector<size_t> layout = {};

    Byte *heapHead = this->heapHead_unsafe();
    Byte *heapTail = this->heapTail_unsafe();

    BlockHeader *current = reinterpret_cast<BlockHeader *>(heapHead);
    while (reinterpret_cast<Byte *>(current) < heapTail)
    {
        layout.push_back(current->size_BPA);
        current = current->getNextPtr();
    }

    // Convert to string
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

// Helper Methods
int ShmemHeap::shfreeHelper(Byte *ptr)
{
    // Safely handle special cases
    if (ptr == nullptr)
        return 0;

    this->checkConnection();
    Byte *headPtr = this->heapHead_unsafe();

    if (!verifyPayloadPtr(ptr))
    {
        this->logger->warn("shfree(payloadOffset={}) find the offset invalid, which should never be passed to shfree", ptr - headPtr);
        return -1;
    }

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

        size_t prevBlockSize = prevBlockHeader->size();
        size_t newSize = prevBlockSize + coalesceTarget->size();

        // Size: newSize; Busy: 1; Previous Allocated: not changed; Allocated: 0
        prevBlockHeader->size_BPA = newSize | 0b100 | (prevBlockHeader->size_BPA & 0b010) & ~0b001;
        newFooter->val() = newSize;

        // update free list ptr
        this->removeFreeBlock(coalesceTarget);

        // Write log
        this->logger->debug("shfree(payloadOffset={}) coalesced with prev: [{}, {}] <- [{}, {}]", ptr - headPtr, reinterpret_cast<Byte *>(prevBlockHeader) - headPtr, reinterpret_cast<Byte *>(coalesceTarget) - headPtr, reinterpret_cast<Byte *>(coalesceTarget) - headPtr, reinterpret_cast<Byte *>(newFooter) - headPtr);

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

        // Write log
        this->logger->debug("shfree(payloadOffset={}) coalesced with next: [{}, {}] <- [{}, {}]", ptr - headPtr, reinterpret_cast<Byte *>(coalesceTarget) - headPtr, reinterpret_cast<Byte *>(nextBlockHeader) - headPtr, reinterpret_cast<Byte *>(nextBlockHeader) - headPtr, reinterpret_cast<Byte *>(newFooter) - headPtr);
        // coalesceTarget unchanged;
    }

    // Reset Busy bit
    coalesceTarget->setB(false);

    this->logger->info("shfree(payloadOffset={}) succeeded", ptr - headPtr);
    return 0;
}
// Utility functions
bool ShmemHeap::verifyPayloadPtr(Byte *ptr)
{
    if (ptr - unitSize < this->heapHead_unsafe() || ptr + 3 * unitSize > this->heapTail_unsafe())
    {
        return false;
    }
    // Check if the pointer is aligned
    if ((ptr - this->heapHead_unsafe()) % unitSize != 0)
    {
        return false;
    }
    // Further check
    return true;
}

// Free list pointer manipulators
void ShmemHeap::removeFreeBlock(BlockHeader *block)
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

void ShmemHeap::insertFreeBlock(BlockHeader *block, BlockHeader *prevBlock)
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

// Protected/Private Methods

inline size_t &ShmemHeap::staticCapacity_unsafe()
{
    return reinterpret_cast<size_t *>(this->shmPtr)[0];
}

inline size_t &ShmemHeap::heapCapacity_unsafe()
{
    return reinterpret_cast<size_t *>(this->shmPtr)[1];
}

inline size_t &ShmemHeap::freeBlockListOffset_unsafe()
{
    return reinterpret_cast<size_t *>(this->shmPtr)[2];
}

inline size_t &ShmemHeap::entranceOffset_unsafe()
{
    return reinterpret_cast<size_t *>(this->staticSpaceHead_unsafe())[3];
}

inline Byte *ShmemHeap::staticSpaceHead_unsafe()
{
    return this->shmPtr;
}

inline Byte *ShmemHeap::entrance_unsafe()
{
    size_t offset = this->entranceOffset_unsafe();
    if (offset == NPtr)
        return nullptr;
    else
        return this->heapHead_unsafe() + this->entranceOffset_unsafe();
}

inline Byte *ShmemHeap::heapHead_unsafe()
{
    return this->shmPtr + this->staticCapacity_unsafe();
}

inline Byte *ShmemHeap::heapTail_unsafe()
{
    return this->shmPtr + this->staticCapacity_unsafe() + this->heapCapacity_unsafe();
}

inline ShmemHeap::BlockHeader *ShmemHeap::freeBlockList_unsafe()
{
    return reinterpret_cast<BlockHeader *>(this->heapHead_unsafe() + this->freeBlockListOffset_unsafe());
}

void ShmemHeap::setHCap(size_t size)
{
    size_t pageSize = sysconf(_SC_PAGESIZE);

    // pad heap capacity to a multiple of page size
    size_t newHeapCapacity = pad(size, pageSize);

    if (this->isConnected())
    {
        size_t currentHeapCapacity = this->heapCapacity_unsafe();
        if (newHeapCapacity < currentHeapCapacity)
            throw std::runtime_error("Heap capacity cannot be shrinked");
    }

    this->HCap = newHeapCapacity;
    this->logger->info("Request heap size: {}, new heap capacity: {}", size, newHeapCapacity);
}

void ShmemHeap::setSCap(size_t size)
{
    // pad static space capacity to a multiple of 8
    size_t newStaticSpaceCapacity = pad(size, unitSize);

    if (size < this->minStaticSize * unitSize)
        newStaticSpaceCapacity = this->minStaticSize * unitSize;

    if (this->isConnected())
    {
        size_t currentStaticSpaceCapacity = this->staticCapacity_unsafe();
        if (newStaticSpaceCapacity < currentStaticSpaceCapacity)
            throw std::runtime_error("Static space capacity cannot be shrinked");
    }

    this->SCap = newStaticSpaceCapacity;
    this->logger->info("Request static space size: {}, new static space capacity: {}", size, newStaticSpaceCapacity);
}

std::shared_ptr<spdlog::logger> &ShmemHeap::getLogger()
{
    return this->logger;
}
