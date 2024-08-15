#include "TypeEncodings.h"

// Definition of typeNames
const std::unordered_map<int, std::string> typeNames = {
    {Bool, "bool"},
    {Char, "char"},
    {UChar, "unsigned char"},
    {Short, "short"},
    {UShort, "unsigned short"},
    {Int, "int"},
    {UInt, "unsigned int"},
    {Long, "long"},
    {ULong, "unsigned long"},
    {LongLong, "long long"},
    {ULongLong, "unsigned long long"},
    {Float, "float"},
    {Double, "double"},
    {String, "string"},
    {List, "list"},
    {DictNode, "dict node"},
    {Dict, "dict"},
};

bool isPrimitive(int type)
{
    return type < PrimitiveThreshold && type >= 0;
}
