#ifndef SHMEM_PRIMITIVE_TCC
#define SHMEM_PRIMITIVE_TCC

#include "ShmemObj.h"
#include "ShmemPrimitive.h"

template <typename T>
inline size_t ShmemPrimitive_::makeSpace(size_t size, ShmemHeap *heapPtr)
{
    // Alignment will be automatically done
    size_t offset = heapPtr->shmalloc(sizeof(ShmemPrimitive_) + size * sizeof(T));
    ShmemObj *ptr = reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + offset);
    ptr->type = TypeEncoding<T>::value;
    ptr->size = size;
    return offset;
}

template <typename T>
inline size_t ShmemPrimitive_::construct(const T &val, ShmemHeap *heapPtr)
{
    if constexpr (std::is_pointer_v<T> || std::is_same_v<T, std::nullptr_t>)
    {
        if (val == nullptr)
        {
            return NPtr;
        }
    }
    if constexpr (isPrimitive<T>())
    {
        if constexpr (isVector<T>::value)
        { // a vector of primitive, such as vector<int>, vector<float>
            using vecDataType = typename unwrapVectorType<T>::type;

            size_t size = val.size();
            size_t offset = makeSpace<vecDataType>(size, heapPtr);
            ShmemPrimitive_ *ptr = reinterpret_cast<ShmemPrimitive_ *>(heapPtr->heapHead() + offset);
            if constexpr (std::is_same_v<vecDataType, bool>)
            { // Handle bool vector separately as it doesn't have .data()
                for (size_t i = 0; i < size; i++)
                {
                    reinterpret_cast<vecDataType *>(ptr->getBytePtr())[i] = val[i];
                }
            }
            else
            {
                memcpy(ptr->getBytePtr(), val.data(), size * sizeof(vecDataType));
            }
            return offset;
        }
        else
        { // signal value, such as int, bool, float or char
            size_t offset = makeSpace<T>(1, heapPtr);
            ShmemPrimitive_ *ptr = reinterpret_cast<ShmemPrimitive_ *>(heapPtr->heapHead() + offset);
            reinterpret_cast<T *>(ptr->getBytePtr())[0] = val;
            return offset;
        }
    }
    else if constexpr (isString<T>())
    { // Call the non-template implementation
        printf("Code reach a strange place\n");
        return ShmemPrimitive_::construct(std::string(val), heapPtr); // Convert to std::string and call the non-template implementation
    }
    else if constexpr (isVector<T>::value)
    {
        throw std::runtime_error("Code should not reach here. Primitive object doesn't accept nested type");
    }
    else if constexpr (std::is_same_v<T, pybind11::object> || std::is_same_v<T, pybind11::str> || std::is_same_v<T, pybind11::bytes> || std::is_same_v<T, pybind11::int_> || std::is_same_v<T, pybind11::float_> || std::is_same_v<T, pybind11::bool_> || std::is_same_v<T, pybind11::list>)
    {
#define CONSTRUCT_PRIMITIVE_WITH_PYTHON_OBJECT(TYPE, VALUE) \
    return ShmemPrimitive_::construct(VALUE, heapPtr);

        SWITCH_PYTHON_OBJECT_TO_PRIMITIVE(val, CONSTRUCT_PRIMITIVE_WITH_PYTHON_OBJECT);

#undef CONSTRUCT_PRIMITIVE_WITH_PYTHON_OBJECT
    }
    else
    {
        throw std::runtime_error("Cannot convert non-primitive object to primitive");
    }
}

// Collection interface

// __getitem__
template <typename T>
inline T ShmemPrimitive_::get(int index) const
{
    if constexpr (isPrimitive<T>())
    {
        if constexpr (isVector<T>::value)
        { // All elements
            if (index != 0)
            {
                printf("It's nonsense to provide an index when converting to a vector\n");
            }
            using vecDataType = typename unwrapVectorType<T>::type;
            std::vector<vecDataType> result(this->size);

#define SHMEM_PRIMITIVE_CONVERT_VEC(TYPE)                                                                                                               \
    std::transform(reinterpret_cast<const TYPE *>(this->getBytePtr()), reinterpret_cast<const TYPE *>(this->getBytePtr()) + this->size, result.begin(), \
                   [](TYPE value) { return static_cast<vecDataType>(value); });                                                                         \
    return result;
            SWITCH_PRIMITIVE_TYPES(this->type, SHMEM_PRIMITIVE_CONVERT_VEC)

#undef SHMEM_PRIMITIVE_CONVERT_VEC
        }
        else
        { // Single element
#define SHMEM_PRIMITIVE_CONVERT(TYPE) \
    return static_cast<T>(reinterpret_cast<const TYPE *>(this->getBytePtr())[this->resolveIndex(index)]);

            SWITCH_PRIMITIVE_TYPES(this->type, SHMEM_PRIMITIVE_CONVERT)

#undef SHMEM_PRIMITIVE_CONVERT
        }
    }
    if constexpr (isString<T>())
    {
        if (index != 0)
            printf("It's nonsense to provide an index when converting to a string\n");

        if (this->type != Char)
        {
            throw ConversionError("Cannot convert non-char type to a string");
        }

        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, const std::string>)
        {
            size_t endPoint = this->index('\0');
            if (endPoint == -1)
                return std::string(reinterpret_cast<const char *>(this->getBytePtr()), this->size);
            else
                return std::string(reinterpret_cast<const char *>(this->getBytePtr()), endPoint);
        }
        else if constexpr (std::is_same_v<T, char *> || std::is_same_v<T, const char *>)
        { // TODO: Should I just allocate some mem and return it?
            throw ConversionError("Cannot return char* type yet");
        }
        else
        {
            throw std::runtime_error("Code should not reach here");
        }
    }
    // Based on the type asked for
    else if constexpr (std::is_same_v<T, pybind11::str>)
    {
        if (this->type == Char)
        {
            return pybind11::str(this->operator std::string());
        }
    }
    else if constexpr (std::is_same_v<T, pybind11::bytes>)
    {
        if (this->type == UChar)
        {
            std::vector<unsigned char> vec = this->operator std::vector<unsigned char>();
            return pybind11::bytes(reinterpret_cast<char *>(vec.data()), vec.size());
        }
    }
    else if constexpr (std::is_same_v<T, pybind11::list>)
    {
        const Byte *ptr = this->getBytePtr();
        size_t i = 0;
        pybind11::list result;
#define PRIMITIVE_TO_PYTHON_LIST(TYPE, PY_TYPE)                                             \
    for (; i < this->size; i++)                                                             \
    {                                                                                       \
        if constexpr (std::is_same_v<TYPE, std::string>)                                    \
        {                                                                                   \
            result.append(pybind11::int_(reinterpret_cast<const char *>(ptr)[i]));          \
        }                                                                                   \
        else if constexpr (std::is_same_v<TYPE, unsigned char>)                             \
        {                                                                                   \
            result.append(pybind11::int_(reinterpret_cast<const unsigned char *>(ptr)[i])); \
        }                                                                                   \
        else                                                                                \
            result.append(PY_TYPE(reinterpret_cast<const TYPE *>(ptr)[i]));                 \
    }

        SWITCH_PRIMITIVE_TYPES_TO_PY(this->type, PRIMITIVE_TO_PYTHON_LIST);
        return result;

#undef PRIMITIVE_TO_PYTHON_LIST
    }
    else if constexpr (std::is_same_v<T, pybind11::bool_>)
    {
        return pybind11::bool_(this->operator bool());
    }
    else if constexpr (std::is_same_v<T, pybind11::int_>)
    {
        return pybind11::int_(this->operator int());
    }
    else if constexpr (std::is_same_v<T, pybind11::float_>)
    {
        return pybind11::float_(this->operator double());
    }
    else if constexpr (std::is_same_v<T, pybind11::object>)
    { // Base on the inner type
        if (this->type == Char)
        {
            return pybind11::str(this->operator std::string());
        }
        else if (this->size == 1)
        {
#define PRIMITIVE_TO_PYTHON_OBJECT(TYPE, PY_TYPE) \
    return PY_TYPE(this->operator TYPE());

            SWITCH_PRIMITIVE_TYPES_TO_PY(this->type, PRIMITIVE_TO_PYTHON_OBJECT);

#undef PRIMITIVE_TO_PYTHON_OBJECT
        }
        else
        {
            const Byte *ptr = this->getBytePtr();
            size_t i = 0;

            pybind11::list result;
#define PRIMITIVE_TO_PYTHON_LIST(TYPE, PY_TYPE)                                             \
    for (; i < this->size; i++)                                                             \
    {                                                                                       \
        if constexpr (std::is_same_v<TYPE, std::string>)                                    \
        {                                                                                   \
            result.append(pybind11::int_(reinterpret_cast<const char *>(ptr)[i]));          \
        }                                                                                   \
        else if constexpr (std::is_same_v<TYPE, unsigned char>)                             \
        {                                                                                   \
            result.append(pybind11::int_(reinterpret_cast<const unsigned char *>(ptr)[i])); \
        }                                                                                   \
        else                                                                                \
            result.append(PY_TYPE(reinterpret_cast<const TYPE *>(ptr)[i]));                 \
    }

            SWITCH_PRIMITIVE_TYPES_TO_PY(this->type, PRIMITIVE_TO_PYTHON_LIST);
            return result;
#undef PRIMITIVE_TO_PYTHON_LIST
        }
    }
    throw ConversionError("Cannot convert");
}

template <typename T>
inline T ShmemPrimitive_::operator[](int index) const
{
    return this->get<T>(index);
}

// __setitem__
template <typename T>
inline void ShmemPrimitive_::set(const T &value, int index)
{
    if constexpr (isPrimitiveBaseCase<T>())
    {
#define SHMEM_CONVERT_PRIMITIVE(TYPE) \
    reinterpret_cast<TYPE *>(this->getBytePtr())[this->resolveIndex(index)] = static_cast<TYPE>(value);

        SWITCH_PRIMITIVE_TYPES(this->type, SHMEM_CONVERT_PRIMITIVE)

#undef SHMEM_CONVERT_PRIMITIVE
    }
    else if constexpr (std::is_base_of_v<pybind11::object,T>)
    {
#define SET_PYTHON_OBJECT(TYPE, VALUE) \
    this->set(VALUE, index)

        SWITCH_PYTHON_OBJECT_TO_PRIMITIVE_BASE_CASE(value, SET_PYTHON_OBJECT)

#undef SET_PYTHON_OBJECT
    }
    else
    {
        throw std::runtime_error("Setting " + typeNames.at(this->type) + " with " + typeName<T>() + " is not allowed");
    }
}

// __delitem__
inline void ShmemPrimitive_::del(int index)
{
    index = this->resolveIndex(index);
    Byte *ptr = this->getBytePtr();
#define SHMEM_DEL_PRIMITIVE(TYPE)                                                \
    for (int i = index; i < this->size - 1; i++)                                 \
    {                                                                            \
        reinterpret_cast<TYPE *>(ptr)[i] = reinterpret_cast<TYPE *>(ptr)[i + 1]; \
    }                                                                            \
    reinterpret_cast<TYPE *>(ptr)[this->size - 1] = 0;

    SWITCH_PRIMITIVE_TYPES(this->type, SHMEM_DEL_PRIMITIVE)

#undef SHMEM_DEL_PRIMITIVE

    this->size--;
}

// __contains__
template <typename T>
inline bool ShmemPrimitive_::contains(T value) const
{
    if constexpr (isPrimitiveBaseCase<T>())
    {
        const Byte *ptr = this->getBytePtr();
#define SHMEM_PRIMITIVE_COMP(TYPE)                                                     \
    for (int i = 0; i < this->size; i++)                                               \
    {                                                                                  \
        try                                                                            \
        {                                                                              \
            if (static_cast<const T>(reinterpret_cast<const TYPE *>(ptr)[i]) == value) \
                return true;                                                           \
        }                                                                              \
        catch (std::bad_cast & e)                                                      \
        {                                                                              \
            continue;                                                                  \
        }                                                                              \
    }

        SWITCH_PRIMITIVE_TYPES(this->type, SHMEM_PRIMITIVE_COMP)

#undef SHMEM_PRIMITIVE_COMP
    }
    else if constexpr (std::is_same_v<T, pybind11::object>)
    {
#define CONTAINS_PYTHON_OBJECT(TYPE, VALUE) \
    return this->contains(VALUE)

        SWITCH_PYTHON_OBJECT_TO_PRIMITIVE_BASE_CASE(value, CONTAINS_PYTHON_OBJECT)

#undef CONTAINS_PYTHON_OBJECT
    }

    return false;
}

// __index__
template <typename T>
inline int ShmemPrimitive_::index(T value) const
{
    if constexpr (isPrimitiveBaseCase<T>())
    {
        const Byte *ptr = this->getBytePtr();
#define SHMEM_PRIMITIVE_COMP(TYPE)                                               \
    for (int i = 0; i < this->size; i++)                                         \
    {                                                                            \
        try                                                                      \
        {                                                                        \
            if (static_cast<T>(reinterpret_cast<const TYPE *>(ptr)[i]) == value) \
                return i;                                                        \
        }                                                                        \
        catch (std::bad_cast & e)                                                \
        {                                                                        \
            continue;                                                            \
        }                                                                        \
    }

        SWITCH_PRIMITIVE_TYPES(this->type, SHMEM_PRIMITIVE_COMP)

#undef SHMEM_PRIMITIVE_COMP
    }
    else if constexpr (std::is_same_v<T, pybind11::object>)
    {
#define INDEX_PYTHON_OBJECT(TYPE, VALUE) \
    return this->index(VALUE)

        // SWITCH_PYTHON_OBJECT_TO_PRIMITIVE_BASE_CASE(value, INDEX_PYTHON_OBJECT);

#undef INDEX_PYTHON_OBJECT
    }

    return -1;
}

// __str__
// Type independent, in ShmemPrimitive.cpp

// Convert
template <typename T>
inline ShmemPrimitive_::operator T() const
{
    if constexpr (isPrimitive<T>())
    {
        if constexpr (isVector<T>::value)
        {
            return this->ShmemPrimitive_::get<T>(0);
        }
        else
        {
            if (this->size != 1)
                throw std::runtime_error("Cannot convert an array to a single value");
            return this->ShmemPrimitive_::get<T>(0);
        }
    }
    else if constexpr (isString<T>())
    {
        return this->ShmemPrimitive_::get<T>(0);
    }
    // Based on the type asked for
    else
        return this->ShmemPrimitive_::get<T>(0);
}

// Arithmetic Interface
template <typename T>
inline bool ShmemPrimitive_::operator==(const T &val) const
{
    if constexpr (isObjPtr<T>::value)
    {
        if (val->type != this->type)
            return false;
        // Ensure T = const ShmemPrimitive<this->type>*

        if (this->size != val->size)
            return false;

        const Byte *ptr = this->getBytePtr();
        const Byte *valPtr = reinterpret_cast<const ShmemPrimitive_ *>(val)->getBytePtr();

#define SHMEM_CMP_PRIMITIVE(TYPE)                                                                \
    for (size_t i = 0; i < this->size; i++)                                                      \
    {                                                                                            \
        if (reinterpret_cast<const TYPE *>(ptr)[i] != reinterpret_cast<const TYPE *>(valPtr)[i]) \
            return false;                                                                        \
    }

        SWITCH_PRIMITIVE_TYPES(this->type, SHMEM_CMP_PRIMITIVE)

        return true;

#undef SHMEM_CMP_PRIMITIVE
    }
    else if constexpr (isString<T>())
    {
        return this->operator std::string() == std::string(val);
    }
    else if constexpr (isPrimitive<T>())
    {
        if constexpr (isVector<T>::value)
        {
            using vecDataType = typename unwrapVectorType<T>::type;

            if (this->size != val.size())
                return false;

            for (int i = 0; i < this->size; i++)
            {
                if (this->get<vecDataType>(i) != val[i])
                    return false;
            }

            return true;
        }
        else
        {
            if (this->size != 1)
                return false;
            return this->operator T() == val;
        }
    }
    else
    {
        throw std::runtime_error("Comparison of " + typeNames.at(this->type) + " with " + typeName<T>() + " is not allowed");
    }
}
#endif // SHMEM_PRIMITIVE_TCC
