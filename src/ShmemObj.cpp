#include "ShmemObj.h"
#include <cstring>
#include <iostream>

// ShmemObj methods

int ShmemObj::typeId() const
{
    return this->type;
}

std::string ShmemObj::typeStr() const
{
    return typeNames.at(this->type);
}

// Constructor relies on template, it is defined in header file

void ShmemObj::deconstruct(size_t offset, ShmemHeap *heapPtr)
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

// __str__
std::string ShmemObj::toString(int indent, int maxElements) const
{
    if (this == nullptr)
    {
        return "nullptr";
    }

    int type = this->type;
    if (isPrimitive(type))
    {
        return static_cast<const ShmemPrimitive_ *>(this)->toString(indent, maxElements);
    }
    else if (type == List)
    {
        return static_cast<const ShmemList *>(this)->toString(indent, maxElements);
    }
    else if (type == Dict)
    {
        return static_cast<const ShmemDict *>(this)->toString(indent, maxElements);
    }
    else
    {
        throw std::runtime_error("Encounter unknown type in deconstruction");
    }
}

// Iterator
KeyType ShmemObj::beginIdx() const
{
    if (isPrimitive(this->type))
        return 0;
    else if (this->type == List)
        return 0;
    else if (this->type == Dict)
        return static_cast<const ShmemDict *>(this)->beginIdx();
    else
        throw std::runtime_error("Unknown type");
}

KeyType ShmemObj::endIdx() const
{
    if (isPrimitive(this->type))
        return static_cast<int>(static_cast<const ShmemPrimitive_ *>(this)->len());
    else if (this->type == List)
        return static_cast<int>(static_cast<const ShmemList *>(this)->len());
    else if (this->type == Dict)
        return static_cast<const ShmemDict *>(this)->endIdx();
    else
        throw std::runtime_error("Unknown type");
}

KeyType ShmemObj::nextIdx(KeyType index) const
{
    if (isPrimitive(this->type))
    {
        if (std::holds_alternative<std::string>(index))
            throw std::runtime_error("Cannot use string as index on Primitive Object");
        return static_cast<const ShmemPrimitive_ *>(this)->nextIdx(std::get<int>(index));
    }
    else if (this->type == List)
    {
        if (std::holds_alternative<std::string>(index))
            throw std::runtime_error("Cannot use string as index on List Object");
        return static_cast<const ShmemList *>(this)->nextIdx(std::get<int>(index));
    }
    else if (this->type == Dict)
    {
        return static_cast<const ShmemDict *>(this)->nextIdx(index);
    }
    else
    {
        throw std::runtime_error("Unknown type");
    }
}

// Convertors implemented in .tcc

// Arithmetic operators
bool ShmemObj::operator==(const char *val) const
{
    return static_cast<const ShmemPrimitive_ *>(this)->operator==(std::string(val));
}
