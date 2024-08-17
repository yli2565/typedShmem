#ifndef SHMEM_PRIMITIVE_TCC
#define SHMEM_PRIMITIVE_TCC

#include "ShmemObj.h"
#include "ShmemPrimitive.h"

template <typename T>
inline size_t ShmemPrimitive_::makeSpace(size_t size, ShmemHeap *heapPtr)
{
    size_t offset = heapPtr->shmalloc(sizeof(ShmemPrimitive_) + size * sizeof(T));
    ShmemObj *ptr = reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + offset);
    ptr->type = TypeEncoding<T>::value;
    ptr->size = size;
    return offset;
}

template <typename T>
size_t ShmemPrimitive_::construct(T val, ShmemHeap *heapPtr)
{
    if constexpr (isPrimitive<T>())
    {
        if constexpr (isVector<T>::value)
        { // a vector of primitive, such as vector<int>, vector<float>
            using vecDataType = typename unwrapVectorType<T>::type;

            size_t size = val.size();
            size_t offset = makeSpace<vecDataType>(size, heapPtr);
            ShmemPrimitive_ *ptr = reinterpret_cast<ShmemPrimitive_ *>(heapPtr->heapHead() + offset);
            memcpy(ptr->getBytePtr(), val.data(), size * sizeof(vecDataType));
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
        throw std::runtime_error("Code should not reach here Primitive object doesn't accept nested type");
    }
    else
    {
        throw std::runtime_error("Cannot convert non-primitive object to primitive");
    }
}

template <typename T>
T ShmemPrimitive_::getter(const ShmemPrimitive_ *obj, int index)
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
            std::vector<vecDataType> result(obj->size);

#define SHMEM_PRIMITIVE_CONVERT_VEC(TYPE)                                                                                                            \
    std::transform(reinterpret_cast<const TYPE *>(obj->getBytePtr()), reinterpret_cast<const TYPE *>(obj->getBytePtr()) + obj->size, result.begin(), \
                   [](TYPE value) { return static_cast<vecDataType>(value); });                                                                      \
    return result;
            SWITCH_PRIMITIVE_TYPES(obj->type, SHMEM_PRIMITIVE_CONVERT_VEC)

#undef SHMEM_PRIMITIVE_CONVERT_VEC
        }
        else
        { // Single element
#define SHMEM_PRIMITIVE_CONVERT(TYPE) \
    return static_cast<T>(reinterpret_cast<const TYPE *>(obj->getBytePtr())[obj->resolveIndex(index)]);

            SWITCH_PRIMITIVE_TYPES(obj->type, SHMEM_PRIMITIVE_CONVERT)

#undef SHMEM_PRIMITIVE_CONVERT
        }
    }
    if constexpr (isString<T>())
    {
        if (obj->type != Char)
        {
            throw std::runtime_error("Cannot convert non-char type to a string type");
        }

        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, const std::string>)
        {
            return std::string(reinterpret_cast<const char *>(obj->getBytePtr()), obj->size);
        }
        else if constexpr (std::is_same_v<T, char *> || std::is_same_v<T, const char *>)
        { // TODO: Should I just allocate some mem and return it?
            throw std::runtime_error("Cannot return char* type yet");
        }
        else
        {
            throw std::runtime_error("Code should not reach here");
        }
    }
    else
    { // Cannot convert
        throw std::runtime_error("Cannot convert");
    }
}

template <typename T>
inline T ShmemPrimitive_::getter(int index) const
{
    return getter<T>(this, index);
}

template <typename T>
void ShmemPrimitive_::setter(ShmemPrimitive_ *obj, T value, int index)
{
    if constexpr (isPrimitive<T>())
    {
        if constexpr (isVector<T>::value)
        {
            throw std::runtime_error("Setting " + typeNames.at(obj->type) + " with " + typeName<T>() + " is not allowed");
        }
        else
        {
#define SHMEM_CONVERT_PRIMITIVE(TYPE) \
    reinterpret_cast<TYPE *>(obj->getBytePtr())[obj->resolveIndex(index)] = static_cast<TYPE>(value);

            SWITCH_PRIMITIVE_TYPES(obj->type, SHMEM_CONVERT_PRIMITIVE)

#undef SHMEM_CONVERT_PRIMITIVE
        }
    }
    else
    {
        throw std::runtime_error("Setting " + typeNames.at(obj->type) + " with " + typeName<T>() + " is not allowed");
    }
}

template <typename T>
inline void ShmemPrimitive_::setter(T value, int index)
{
    setter<T>(this, value, index);
}

// interface

template <typename T>
ShmemPrimitive_::operator T() const
{
    return this->getter<T>();
}

// __getitem__
template <typename T>
inline T ShmemPrimitive_::get(int index) const
{
    return this->getter<T>(index);
}

template <typename T>
inline T ShmemPrimitive_::operator[](int index) const
{
    return this->getter<T>(index);
}

// __setitem__
template <typename T>
inline void ShmemPrimitive_::set(T value, int index)
{
    setter(this, value, index);
}

// __contains__
template <typename T>
int ShmemPrimitive_::contains(T value) const
{
    Byte *ptr = this->getBytePtr();
#define SHMEM_PRIMITIVE_COMP(TYPE)                                         \
    for (int i = 0; i < this->size; i++)                                   \
    {                                                                      \
        try                                                                \
        {                                                                  \
            if (static_cast<T>(reinterpret_cast<TYPE *>(ptr)[i]) == value) \
                return i;                                                  \
        }                                                                  \
        catch (std::bad_cast & e)                                          \
        {                                                                  \
            continue;                                                      \
        }                                                                  \
    }

    SWITCH_PRIMITIVE_TYPES(this->type, SHMEM_PRIMITIVE_COMP)

#undef SHMEM_PRIMITIVE_COMP
    return -1;
}

// __str__
inline std::string ShmemPrimitive_::toString(int indent = 0, int maxElements = 4) const
{
    return ShmemPrimitive_::toString(this, indent, maxElements);
}

inline std::string ShmemPrimitive_::elementToString(int index) const
{
    return ShmemPrimitive_::elementToString(this, index);
}

// Arithmetic Interface
// TODO

// ShmemPrimitive<T>
template <typename T>
inline size_t ShmemPrimitive<T>::makeSpace(size_t size, ShmemHeap *heapPtr)
{
    return ShmemPrimitive_::makeSpace<T>(size, heapPtr);
}

template <typename T>
inline ShmemPrimitive<T>::operator T() const
{
    return this->ShmemPrimitive_::getter<T>();
}

template <typename T>
inline ShmemPrimitive<T>::operator std::vector<T>() const
{
    return this->ShmemPrimitive_::getter<std::vector<T>>();
}

template <typename T>
inline ShmemPrimitive<T>::operator std::string() const
{
    return this->ShmemPrimitive_::getter<std::string>();
}

template <typename T>
T ShmemPrimitive<T>::get(int index) const
{
    return this->ShmemPrimitive_::getter<T>(index);
}

template <typename T>
T ShmemPrimitive<T>::operator[](int index) const
{
    return this->ShmemPrimitive_::getter<T>(index);
}

template <typename T>
inline void ShmemPrimitive<T>::set(T value, int index)
{
    this->ShmemPrimitive_::setter<T>(value, index);
}
template <typename T>
int ShmemPrimitive<T>::contains(T value) const
{
    return this->ShmemPrimitive_::contains<T>(value);
}

template <typename T>
inline const T *ShmemPrimitive<T>::getPtr() const
{
    if (TypeEncoding<T>::value != this->type)
    {
        throw std::runtime_error("Type mismatch");
    }

    return reinterpret_cast<T *>(getBytePtr());
}

#endif // SHMEM_PRIMITIVE_TCC
