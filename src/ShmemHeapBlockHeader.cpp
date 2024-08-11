#include "ShmemHeap.h"
#include <stdexcept>
#include <cstring>
#include <thread>
#include <chrono>

using Byte = unsigned char;

size_t &ShmemHeap::BlockHeader::val()
{
    return *reinterpret_cast<size_t *>(this);
}

inline std::atomic<size_t> &ShmemHeap::BlockHeader::atomicVal() const
{
    return *const_cast<std::atomic<size_t> *>(reinterpret_cast<const std::atomic<size_t> *>(&this->size_BPA));
}

intptr_t &ShmemHeap::BlockHeader::fwdOffset()
{
    return reinterpret_cast<intptr_t *>(this)[1];
}

intptr_t &ShmemHeap::BlockHeader::bckOffset()
{
    return reinterpret_cast<intptr_t *>(this)[2];
}

size_t ShmemHeap::BlockHeader::size() const
{
    return this->atomicVal().load(std::memory_order_relaxed) & ~0b111;
}

void ShmemHeap::BlockHeader::setSize(size_t size)
{
    size_t current = this->atomicVal().load(std::memory_order_relaxed);
    size_t newSize;
    // Ensure correct update is done
    do
    {
        newSize = size | (current & 0b111);
    } while (!this->atomicVal().compare_exchange_weak(current, newSize, std::memory_order_acquire, std::memory_order_relaxed));
}

bool ShmemHeap::BlockHeader::B() const
{
    return this->atomicVal().load(std::memory_order_relaxed) & 0b100;
}

bool ShmemHeap::BlockHeader::P() const
{
    return this->atomicVal().load(std::memory_order_relaxed) & 0b010;
}

bool ShmemHeap::BlockHeader::A() const
{
    return this->atomicVal().load(std::memory_order_relaxed) & 0b001;
}

void ShmemHeap::BlockHeader::setB(bool b)
{
    if (b)
        this->atomicVal().fetch_or(0b100, std::memory_order_relaxed);
    else
        this->atomicVal().fetch_and(~0b100, std::memory_order_relaxed);
}

void ShmemHeap::BlockHeader::setP(bool p)
{
    if (p)
        this->atomicVal().fetch_or(0b010, std::memory_order_relaxed);
    else
        this->atomicVal().fetch_and(~0b010, std::memory_order_relaxed);
}

void ShmemHeap::BlockHeader::setA(bool a)
{
    if (a)
        this->atomicVal().fetch_or(0b001, std::memory_order_relaxed);
    else
        this->atomicVal().fetch_and(~0b001, std::memory_order_relaxed);
}

ShmemHeap::BlockHeader *ShmemHeap::BlockHeader::getFooterPtr() const
{
    return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) + this->size() - sizeof(BlockHeader));
}

ShmemHeap::BlockHeader *ShmemHeap::BlockHeader::getFwdPtr() const
{
    return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) + reinterpret_cast<const intptr_t *>(this)[1]);
}

ShmemHeap::BlockHeader *ShmemHeap::BlockHeader::getBckPtr() const
{
    return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) + reinterpret_cast<const intptr_t *>(this)[2]);
}

ShmemHeap::BlockHeader *ShmemHeap::BlockHeader::getPrevPtr() const
{
    if (this->P())
    {
        throw std::runtime_error("Previous block is allocated. We should not call this function");
    }
    return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) - (reinterpret_cast<const BlockHeader *>(this) - 1)->size_BPA);
}

ShmemHeap::BlockHeader *ShmemHeap::BlockHeader::getNextPtr() const
{
    return reinterpret_cast<BlockHeader *>(reinterpret_cast<uintptr_t>(this) + this->size());
}

void ShmemHeap::BlockHeader::remove()
{
    if (this->A())
    {
        throw std::runtime_error("Cannot remove allocated block");
    }
    BlockHeader *bckBlockHeader = this->getBckPtr();
    BlockHeader *fwdBlockHeader = this->getFwdPtr();
    fwdBlockHeader->bckOffset() = fwdBlockHeader->offset(bckBlockHeader);
    bckBlockHeader->fwdOffset() = bckBlockHeader->offset(fwdBlockHeader);
}

void ShmemHeap::BlockHeader::insert(BlockHeader *prevBlock)
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

        intptr_t fwdOffset = this->offset(fwdBlockHeader);
        intptr_t bckOffset = this->offset(bckBlockHeader);

        // Insert this block after the prevBlock
        this->fwdOffset() = fwdOffset;
        this->bckOffset() = bckOffset;
        fwdBlockHeader->bckOffset() = -fwdOffset;
        bckBlockHeader->fwdOffset() = -bckOffset;
    }
}

intptr_t ShmemHeap::BlockHeader::offset(Byte *desPtr) const
{
    return reinterpret_cast<uintptr_t>(desPtr) - reinterpret_cast<uintptr_t>(this);
}

intptr_t ShmemHeap::BlockHeader::offset(BlockHeader *desPtr) const
{
    return reinterpret_cast<uintptr_t>(desPtr) - reinterpret_cast<uintptr_t>(this);
}

int ShmemHeap::BlockHeader::wait(int timeout) const
{
    while (this->B())
    {
        if (timeout == 0)
        {
            return ETIMEDOUT;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (timeout > 0)
        {
            timeout--;
        }
    }
    return 0;
}
