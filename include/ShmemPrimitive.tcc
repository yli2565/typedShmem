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
        { // a vector
            using vecDataType = typename unwrapVectorType<T>::type;

            size_t size = vec.size();
            size_t offset = makeSpace(size, heapPtr);
            ShmemPrimitive_ ptr = reinterpret_cast<ShmemPrimitive_ *>(heapPtr->heapHead() + offset);
            memcpy(ptr->getPtr(), vec.data(), size * sizeof(vecDataType));
            return offset;
        }
        else
        { // signal value, such as int, bool, float or char
            size_t offset = makeSpace(1, heapPtr);
            ShmemPrimitive<T> *ptr = reinterpret_cast<ShmemPrimitive<T> *>(heapPtr->heapHead() + offset);
            ptr->getPtr()[0] = val;
            return offset;
        }
    }
    else if constexpr (isString<T>())
    { // Call the non-template implementation
        printf("Code reach a strange place\n");
        return ShmemPrimitive_::construct(std::string(val), heapPtr);
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
T ShmemPrimitive_::getter(ShmemPrimitive_ *obj, int index)
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

#define SHMEM_PRIMITIVE_CONVERT_VEC(TYPE)                                                                                                       \
    return std::transform(reinterpret_cast<TYPE *>(obj->getBytePtr()), reinterpret_cast<TYPE *>(obj->getBytePtr()) + obj->size, result.begin(), \
                          [](TYPE value) { return dynamic_cast<vecDataType>(value); });

            SWITCH_PRIMITIVE_TYPES(obj->type, SHMEM_PRIMITIVE_CONVERT_VEC)

#undef SHMEM_PRIMITIVE_CONVERT_VEC
        }
        else
        { // Single element
#define SHMEM_PRIMITIVE_CONVERT(TYPE) \
    return dynamic_cast<T>(reinterpret_cast<TYPE *>(obj->getBytePtr())[obj->resolveIndex(index)]);

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

        if constexpr (std::is_same_v<T, char *> || std::is_same_v<T, const char *>)
        { // TODO: Should I just allocate some mem and return it?
            throw std::runtime_error("Cannot return char* type yet");
        }
    }
    else
    { // Cannot convert
        throw std::runtime_error("Cannot convert");
    }
}

template <typename T>
inline T ShmemPrimitive_::getter(int index)
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
            throw std::runtime_error("You can only set an element with a vector, instead of using a vector to assign the whole Primitive Obj.");
        }
        else
        {
#define SHMEM_CONVERT_PRIMITIVE(TYPE) \
    reinterpret_cast<TYPE *>(obj->getBytePtr())[obj->resolveIndex(index)] = dynamic_cast<TYPE>(value);

            SWITCH_PRIMITIVE_TYPES(obj->type, SHMEM_CONVERT_PRIMITIVE)

#undef SHMEM_CONVERT_PRIMITIVE
        }
    }
    else
    {
        throw std::runtime_error("You can only set an element with a string");
    }
}

template <typename T>
inline void ShmemPrimitive_::setter(T value, int index)
{
    setter<T>(this, val, index);
}

// public getter and setters
template <typename T>
inline T ShmemPrimitive_::operator[](int index) const
{
    return this->getter<T>(index);
}

template <typename T>
inline void ShmemPrimitive_::set(T value, int index)
{
    setter(this, value, index);
}
// ShmemPrimitive<T>
template <typename T>
inline size_t ShmemPrimitive<T>::makeSpace(size_t size, ShmemHeap *heapPtr)
{
    return ShmemPrimitive_::makeSpace<T>(size, heapPtr);
}

template <typename T>
int ShmemPrimitive<T>::find(T value) const
{
    checkType();
    if (this->size == 0)
    {
        return -1;
    }
    T *ptr = this->getPtr();
    for (int i = 0; i < this->size; i++)
    {
        if (ptr[i] == value)
        {
            return i;
        }
    }
    return -1;
}

template <typename T>
static size_t construct(T val, ShmemHeap *heapPtr)
{
    return ShmemPrimitive_::construct(val, heapPtr);
}

template <typename T>
void ShmemPrimitive<T>::set(int index, T value)
{
    ShmemPrimitive_::setter(this, value, index);
}

template <typename T>
ShmemPrimitive<T>::operator T() const
{
    const T *ptr = this->getPtr();
    if (this->size != 1)
    {
        printf("Size is not 1, but you are only accessing the first element\n");
    }
    return *ptr;
}

template <typename T>
ShmemPrimitive<T>::operator std::vector<T>() const
{
    const T *ptr = this->getPtr();
    std::vector<T> vec(ptr, ptr + this->size);
    return vec;
}

template <typename T>
ShmemPrimitive<T>::operator std::string() const
{
    if (this->type != Char)
    {
        throw std::runtime_error("Cannot convert non-char object to string");
    }
    const T *ptr = this->getPtr();
    std::string str(ptr, ptr + this->size);
    return str;
}

template <typename T>
void ShmemPrimitive<T>::copyTo(T *target, int size) const
{
    T *ptr = this->getPtr();
    if (size == -1)
    {
        size = this->size;
    }
    if (size > this->size)
    {
        throw std::runtime_error("Size is larger than the object");
    }
    std::copy(ptr, ptr + size, target);
}

template <typename T>
const T *ShmemPrimitive<T>::getPtr() const
{
    checkType();
    return reinterpret_cast<const T *>(getBytePtr());
}

template <typename T>
T *ShmemPrimitive<T>::getPtr()
{
    checkType();
    return reinterpret_cast<T *>(getBytePtr());
}

#endif // SHMEM_PRIMITIVE_TCC
