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

std::string ShmemObj::toString(ShmemObj *obj, int indent, int maxElements)
{
    if (obj == nullptr)
    {
        return "nullptr";
    }

    int type = obj->type;
    if (isPrimitive(type))
    {
        return static_cast<ShmemPrimitive_ *>(obj)->toString(indent, maxElements);
    }
    else if (type == List)
    {
        return static_cast<ShmemList *>(obj)->toString(indent, maxElements);
    }
    else if (type == Dict)
    {
        return static_cast<ShmemDict *>(obj)->toString(indent, maxElements);
    }
    else
    {
        throw std::runtime_error("Encounter unknown type in deconstruction");
    }
}

// Arithmetic operators
bool ShmemObj::operator==(const char *val) const
{
    return static_cast<const ShmemPrimitive_ *>(this)->operator==(std::string(val));
}
