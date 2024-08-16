#include "ShmemPrimitive.h"

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

std::string ShmemPrimitive_::toString(const ShmemPrimitive_ *obj, int indent, int maxElements)
{
#define PRINT_OBJ(type)                                                              \
    for (int i = 0; i < std::min(obj->size, maxElements); i++)                       \
    {                                                                                \
        result.append(" ").append(std::to_string(reinterpret_cast<const type *>(ptr)[i])); \
    }
    std::string result;
    result.reserve(40);
    result.append("[P:").append(typeNames.at(obj->type)).append(":").append(std::to_string(obj->size)).append("]");

    const Byte *ptr = obj->getBytePtr();

    SWITCH_PRIMITIVE_TYPES(obj->type, PRINT_OBJ)

#undef PRINT_OBJ
    if (obj->size > maxElements)
    {
        result += " ...";
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