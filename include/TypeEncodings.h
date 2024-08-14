#ifndef TYPEENCODINGS_H
#define TYPEENCODINGS_H

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <type_traits>
#include <unordered_map>

// Constants
static const int Bool = 1;
static const int Char = 2;
static const int UChar = 3;
static const int Short = 4;
static const int UShort = 5;
static const int Int = 6;
static const int UInt = 7;
static const int Long = 8;
static const int ULong = 9;
static const int LongLong = 10;
static const int ULongLong = 11;

static const int Float = 21;
static const int Double = 22;

static const int PrimitiveThreshold = 100;

static const int String = 101;
static const int List = 102;
static const int DictNode = 103;
static const int Dict = 104;

extern const std::unordered_map<int, std::string> typeNames;

template <typename T>
struct TypeEncoding
{
    static constexpr int value = -1;
    operator int() const
    {
        return value;
    }
};

#define DEFINE_TYPE_ENCODING(TYPE, NUMBER)              \
    template <>                                         \
    struct TypeEncoding<TYPE>                           \
    {                                                   \
        static constexpr int value = NUMBER;            \
        static constexpr std::string_view name = #TYPE; \
        static constexpr int size = sizeof(TYPE);       \
    };

// Declare specializations for specific types
DEFINE_TYPE_ENCODING(bool, Bool)
DEFINE_TYPE_ENCODING(char, Char)
DEFINE_TYPE_ENCODING(unsigned char, UChar)
DEFINE_TYPE_ENCODING(short, Short)
DEFINE_TYPE_ENCODING(unsigned short, UShort)
DEFINE_TYPE_ENCODING(int, Int)
DEFINE_TYPE_ENCODING(unsigned int, UInt)
DEFINE_TYPE_ENCODING(long, Long)
DEFINE_TYPE_ENCODING(unsigned long, ULong)
DEFINE_TYPE_ENCODING(long long, LongLong)
DEFINE_TYPE_ENCODING(unsigned long long, ULongLong)
DEFINE_TYPE_ENCODING(float, Float)
DEFINE_TYPE_ENCODING(double, Double)

template <typename T>
struct isVector : std::false_type
{
};

template <typename T, typename Alloc>
struct isVector<std::vector<T, Alloc>> : std::true_type
{
};

template <typename T>
struct isMap : std::false_type
{
};

template <typename Key, typename T>
struct isMap<std::map<Key, T>> : std::true_type
{
};

template <typename T>
struct unwrapVectorType
{
};

template <typename T, typename Alloc>
struct unwrapVectorType<std::vector<T, Alloc>>
{
    using type = T;
};

template <typename T>
struct unwrapMapType
{
};

template <typename Key, typename T>
struct unwrapMapType<std::map<Key, T>>
{
    using keyType = Key;
    using type = T;
};

template <typename T>
constexpr bool isPrimitive()
{
    return TypeEncoding<T>::value < PrimitiveThreshold;
}

template <typename T>
constexpr bool isString()
{
    return TypeEncoding<T>::value == String;
}

bool isPrimitive(int type);

template <typename T>
struct TypeEncoding<std::vector<T>>
{
    static constexpr int value = isPrimitive<T>() ? TypeEncoding<T>::value : List;
};

template <>
struct TypeEncoding<std::string>
{
    static constexpr int value = String;
};

template <typename Key, typename T>
struct TypeEncoding<std::map<Key, T>>
{
    static constexpr int value = Dict;
};

#endif // TYPEENCODINGS_H
