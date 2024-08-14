#include "ShmemObj.h"
#include <cstring>
#include <iostream>

// IndexError constructor
IndexError::IndexError(const std::string &message)
    : std::runtime_error(message) {}

// hashIntOrString function
int hashIntOrString(KeyType key)
{
    int hashCode = 0;
    if (std::holds_alternative<std::string>(key))
    {
        hashCode = std::hash<std::string>{}(std::get<std::string>(key));
    }
    else
    {
        hashCode = std::hash<int>{}(std::get<int>(key));
    }
    return hashCode;
}

// ShmemObj methods

// Constructor relies on template, it is defined in header file

void ShmemAccessor::ShmemObj::deconstruct(size_t offset, ShmemHeap *heapPtr)
{
    int type = resolveOffset(offset, heapPtr)->type;
    if (isPrimitive(type))
    {
        ShmemPrimitive_::deconstruct(offset, heapPtr);
    }
    else if (type == List)
    {
        ShmemList::deconstruct(offset, heapPtr);
    }
    else if (type == Dict)
    {
        ShmemDict::deconstruct(offset, heapPtr);
    }
    else
    {
        throw std::runtime_error("Encounter unknown type in deconstruction");
    }
}

size_t ShmemAccessor::ShmemObj::capacity() const
{
    return this->getHeader()->size() - sizeof(ShmemHeap::BlockHeader);
}

bool ShmemAccessor::ShmemObj::isBusy() const
{
    return this->getHeader()->B();
}

void ShmemAccessor::ShmemObj::setBusy(bool b)
{
    this->getHeader()->setB(b);
}

int ShmemAccessor::ShmemObj::wait(int timeout) const
{
    return this->getHeader()->wait(timeout);
}

inline ShmemHeap::BlockHeader *ShmemAccessor::ShmemObj::getHeader() const
{
    return reinterpret_cast<ShmemHeap::BlockHeader *>(reinterpret_cast<uintptr_t>(this) - sizeof(ShmemHeap::BlockHeader));
}

inline ShmemAccessor::ShmemObj *ShmemAccessor::ShmemObj::resolveOffset(size_t offset, ShmemHeap *heapPtr)
{
    return reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + offset);
}

std::string ShmemAccessor::ShmemObj::toString(ShmemAccessor::ShmemObj *obj, int indent)
{
    if (obj == nullptr)
    {
        return "nullptr";
    }
    int type = obj->type;
    if (isPrimitive(type))
    {
        return ShmemPrimitive_::toString(static_cast<ShmemPrimitive_ *>(obj), indent);
    }
    else if (type == List)
    {
        return ShmemList::toString(static_cast<ShmemList *>(obj), indent);
    }
    else if (type == Dict)
    {
        return ShmemDict::toString(static_cast<ShmemDict *>(obj), indent);
    }
    else
    {
        throw std::runtime_error("Encounter unknown type in deconstruction");
    }
}
