#include "ShmemPrimitive.h"
// Utility functions
int ShmemPrimitive_::resolveIndex(int index) const
{
    // Adjust negative index and use modulo to handle out-of-bounds
    index = (index < 0) ? index + this->size : index;

    if (index >= this->size)
    {
        throw IndexError("ShmemPrimitive index out of bounds");
    }

    return index;
}

// __str__ implementation
std::string ShmemPrimitive_::toString(const ShmemPrimitive_ *obj, int indent, int maxElements)
{
    std::string result;
    result.reserve(40);
    result.append("[P:").append(typeNames.at(obj->type)).append(":").append(std::to_string(obj->size)).append("]");

    const Byte *ptr = obj->getBytePtr();

    if (obj->type == Char)
    { // Handle char as a special case, as it's likely a character instead of a 1 byte integer
        result.append(" ").append(reinterpret_cast<const char *>(obj->getBytePtr()), maxElements);
        if (obj->size - 1 > maxElements)
        {
            result += "...";
        }
    }
    else
    {
#define PRINT_OBJ(type)                                                                    \
    for (int i = 0; i < std::min(obj->size, maxElements); i++)                             \
    {                                                                                      \
        result.append(" ").append(std::to_string(reinterpret_cast<const type *>(ptr)[i])); \
    }
        SWITCH_PRIMITIVE_TYPES(obj->type, PRINT_OBJ)

#undef PRINT_OBJ

        if (obj->size > maxElements)
        {
            result += " ...";
        }
    }
    return result;
}

std::string ShmemPrimitive_::elementToString(const ShmemPrimitive_ *obj, int index)
{
#define ELEMENT_TO_STRING(TYPE) \
    return std::to_string(reinterpret_cast<const TYPE *>(obj->getBytePtr())[index]);

    SWITCH_PRIMITIVE_TYPES(obj->type, ELEMENT_TO_STRING)

#undef ELEMENT_TO_STRING
}

size_t ShmemPrimitive_::construct(const std::string str, ShmemHeap *heapPtr)
{
    size_t size = str.size() + 1; // +1 for the \0
    size_t offset = makeSpace<char>(size, heapPtr);
    ShmemPrimitive_ *ptr = reinterpret_cast<ShmemPrimitive_ *>(heapPtr->heapHead() + offset);
    memcpy(reinterpret_cast<char *>(ptr->getBytePtr()), str.data(), size * sizeof(char));
    return offset;
}

size_t ShmemPrimitive_::construct(const char *str, ShmemHeap *heapPtr)
{
    size_t size = strlen(str) + 1; // +1 for the \0
    size_t offset = makeSpace<char>(size, heapPtr);
    ShmemPrimitive_ *ptr = reinterpret_cast<ShmemPrimitive_ *>(heapPtr->heapHead() + offset);
    strcpy(reinterpret_cast<char *>(ptr->getBytePtr()), str);
    return offset;
}

// Type (Special interface)
int ShmemPrimitive_::typeId() const
{
    return this->type;
}
std::string ShmemPrimitive_::typeStr() const
{
    return typeNames.at(this->type);
}

// __len__
size_t ShmemPrimitive_::len() const
{
    return this->size;
}