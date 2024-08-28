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

// __str__
std::string ShmemPrimitive_::toString(int indent, int maxElements) const
{
    maxElements = maxElements > 0 ? maxElements : this->size;

    std::string result;
    result.reserve(40);
    result.append("(P:").append(typeNames.at(this->type)).append(":").append(std::to_string(this->size)).append(")[");

    const Byte *ptr = this->getBytePtr();

    if (this->type == Char)
    { // Handle char as a special case, as it's likely a character instead of a 1 byte integer
        result.append(reinterpret_cast<const char *>(this->getBytePtr()), maxElements);
        if (this->size - 1 > maxElements)
        {
            result += "...";
        }
    }
    else
    {
#define PRINT_OBJ(type)                                                        \
    for (int i = 0; i < std::min(this->size, maxElements); i++)                \
    {                                                                          \
        result.append(std::to_string(reinterpret_cast<const type *>(ptr)[i])); \
        if (i < this->size - 1)                                                \
        {                                                                      \
            result.append(", ");                                               \
        }                                                                      \
    }
        SWITCH_PRIMITIVE_TYPES(this->type, PRINT_OBJ)

#undef PRINT_OBJ

        if (this->size > maxElements)
        {
            result += "...";
        }
    }

    result.append("]");
    return result;
}

std::string ShmemPrimitive_::elementToString(int index) const
{
#define ELEMENT_TO_STRING(TYPE) \
    return std::to_string(reinterpret_cast<const TYPE *>(this->getBytePtr())[this->resolveIndex(index)]);

    SWITCH_PRIMITIVE_TYPES(this->type, ELEMENT_TO_STRING)

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

// __len__
size_t ShmemPrimitive_::len() const
{
    return this->size;
}

// Iterator related
int ShmemPrimitive_::nextIdx(int index) const
{
    int resolvedIndex = resolveIndex(index);
    if (resolvedIndex >= this->size)
    {
        throw StopIteration("Primitive array index out of bounds");
    }
    return resolvedIndex + 1;
}

// Converters

// C++ convertors implemented in .tcc
ShmemPrimitive_::operator pybind11::object() const
{
#define PRIMITIVE_TO_PYTHON_OBJECT(TYPE, PY_TYPE) \
    return PY_TYPE(this->operator TYPE());

    SWITCH_PRIMITIVE_TYPES_TO_PY(this->type, PRIMITIVE_TO_PYTHON_OBJECT)
#undef PRIMITIVE_TO_PYTHON_OBJECT
}

pybind11::object ShmemPrimitive_::elementToPyObject(int index) const
{
#define PRIMITIVE_ELEMENT_TO_PYTHON_OBJECT(TYPE, PY_TYPE) \
    return PY_TYPE(this->operator[]<TYPE>(this->resolveIndex(index)));

    SWITCH_PRIMITIVE_TYPES_TO_PY(this->type, PRIMITIVE_ELEMENT_TO_PYTHON_OBJECT)
#undef PRIMITIVE_ELEMENT_TO_PYTHON_OBJECT
}
