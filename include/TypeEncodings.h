#ifndef TYPEENCODINGS_H
#define TYPEENCODINGS_H

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <cxxabi.h>
#include <variant>

// Macro for simplicity
#define SWITCH_PRIMITIVE_TYPES(SWITCH_TARGET, OPERATION_FUNC) \
    switch (SWITCH_TARGET)                                    \
    {                                                         \
    case Bool:                                                \
        OPERATION_FUNC(bool)                                  \
        break;                                                \
    case Char:                                                \
        OPERATION_FUNC(char)                                  \
        break;                                                \
    case UChar:                                               \
        OPERATION_FUNC(unsigned char)                         \
        break;                                                \
    case Short:                                               \
        OPERATION_FUNC(short)                                 \
        break;                                                \
    case UShort:                                              \
        OPERATION_FUNC(unsigned short)                        \
        break;                                                \
    case Int:                                                 \
        OPERATION_FUNC(int)                                   \
        break;                                                \
    case UInt:                                                \
        OPERATION_FUNC(unsigned int)                          \
        break;                                                \
    case Long:                                                \
        OPERATION_FUNC(long)                                  \
        break;                                                \
    case ULong:                                               \
        OPERATION_FUNC(unsigned long)                         \
        break;                                                \
    case LongLong:                                            \
        OPERATION_FUNC(long long)                             \
        break;                                                \
    case ULongLong:                                           \
        OPERATION_FUNC(unsigned long long)                    \
        break;                                                \
    case Float:                                               \
        OPERATION_FUNC(float)                                 \
        break;                                                \
    case Double:                                              \
        OPERATION_FUNC(double)                                \
        break;                                                \
    default:                                                  \
        throw std::runtime_error("Unknown type");             \
        break;                                                \
    }

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

// String
DEFINE_TYPE_ENCODING(char *, String)
DEFINE_TYPE_ENCODING(const char *, String)
DEFINE_TYPE_ENCODING(std::string, String)
DEFINE_TYPE_ENCODING(const std::string, String)

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
struct isPair : std::false_type
{
};

template <typename Key, typename T>
struct isPair<std::pair<Key, T>> : std::true_type
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
struct unwrapInitializerListType
{
};

template <typename T>
struct unwrapInitializerListType<std::initializer_list<T>>
{
    using type = T;
};

template <typename T>
struct unwrapPairType
{
};

template <typename Key, typename T>
struct unwrapPairType<std::pair<Key, T>>
{
    using keyType = Key;
    using type = T;
};

template <typename T>
constexpr std::string typeName()
{
    return std::string(abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr));
}

template <typename T>
constexpr bool isPrimitive()
{
    if constexpr (isVector<T>::value)
    {
        using vecDataType = typename unwrapVectorType<T>::type;
        return (isPrimitive<vecDataType>() && !isVector<vecDataType>::value);
    }
    return TypeEncoding<T>::value < PrimitiveThreshold && TypeEncoding<T>::value >= 0;
}

template <typename T>
constexpr bool isPrimitiveBaseCase()
{
    if constexpr (isVector<T>::value)
    {
        return false;
    }
    return TypeEncoding<T>::value < PrimitiveThreshold && TypeEncoding<T>::value >= 0;
}

template <typename T>
constexpr bool isString()
{
    return TypeEncoding<T>::value == String;
}

template <typename T>
constexpr bool isVectorInitializerList()
{
    using vecDataType = typename unwrapInitializerListType<T>::type;
    if constexpr (isPair<vecDataType>::value)
    {
        return false;
    }
    return true;
}

template <typename T>
constexpr bool isMapInitializerList()
{
    using mapPairType = typename unwrapInitializerListType<T>::type;
    if constexpr (isPair<mapPairType>::value)
    {
        using keyType = typename mapPairType::first_type;
        if constexpr (std::is_same_v<keyType, std::variant<int, std::string>> || isString<keyType>() || std::is_convertible_v<keyType, int>)
        {
            return true;
        }
    }
    return false;
}

bool isPrimitive(int type);

template <typename T>
struct TypeEncoding<std::vector<T>>
{
    static constexpr int value = isPrimitiveBaseCase<T>() ? TypeEncoding<T>::value : List;
};

template <typename Key, typename T>
struct TypeEncoding<std::map<Key, T>>
{
    static constexpr int value = Dict;
};

#endif // TYPEENCODINGS_H
