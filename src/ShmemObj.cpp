#include "ShmemObj.h"
#include <cstring>
#include <iostream>

// ShmemObj methods

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
        return ShmemPrimitive_::toString(static_cast<ShmemPrimitive_ *>(obj), indent, maxElements);
    }
    else if (type == List)
    {
        return ShmemList::toString(static_cast<ShmemList *>(obj), indent, maxElements);
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
