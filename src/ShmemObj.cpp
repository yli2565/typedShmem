#include "ShmemObj.h"
#include <cstring>
#include <iostream>

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

std::string ShmemObj::toString(ShmemObj *obj, int indent)
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
