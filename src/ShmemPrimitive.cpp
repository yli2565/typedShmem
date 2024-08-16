#include "ShmemPrimitive.h"

std::string ShmemPrimitive_::toString(ShmemPrimitive_ *obj, int indent, int maxElements)
{
#define PRINT_OBJ(type)                                                   \
    for (int i = 0; i < std::min(obj->size, maxElements); i++)            \
    {                                                                     \
        result += " " + std::to_string(reinterpret_cast<type *>(ptr)[i]); \
    }                                                                     \
    break;
    std::string result;
    result.reserve(40);
    result.append("[P:").append(typeNames.at(obj->type)).append(":").append(std::to_string(obj->size)).append("]");

    Byte *ptr = obj->getBytePtr();
    switch (obj->type)
    {
    case Bool:
        PRINT_OBJ(bool);
    case Char:
        result.append(" ").append(reinterpret_cast<const char *>(ptr));
        break;
    case UChar:
        PRINT_OBJ(unsigned char);
    case Short:
        PRINT_OBJ(short);
    case UShort:
        PRINT_OBJ(unsigned short);
    case Int:
        PRINT_OBJ(int);
    case UInt:
        PRINT_OBJ(unsigned int);
    case Long:
        PRINT_OBJ(long);
    case ULong:
        PRINT_OBJ(unsigned long);
    case LongLong:
        PRINT_OBJ(long long);
    case ULongLong:
        PRINT_OBJ(unsigned long long);
    case Float:
        PRINT_OBJ(float);
    case Double:
        PRINT_OBJ(double);
    default:
        throw std::runtime_error("Unknown type");
        break;
    }
#undef PRINT_OBJ
    return result;
}

size_t ShmemPrimitive_::construct(std::string str, ShmemHeap *heapPtr)
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