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
inline size_t ShmemPrimitive<T>::makeSpace(size_t size, ShmemHeap *heapPtr)
{
    return ShmemPrimitive_::makeSpace<T>(size, heapPtr);
}

template <typename T>
size_t ShmemPrimitive<T>::construct(T val, ShmemHeap *heapPtr)
{
    if constexpr (isPrimitive<T>())
    { // signal value, such as int, bool, float or char
        size_t offset = makeSpace(1, heapPtr);
        ShmemPrimitive<T> *ptr = reinterpret_cast<ShmemPrimitive<T> *>(heapPtr->heapHead() + offset);
        ptr->getPtr()[0] = val;
        return offset;
    }
    else if constexpr (isString<T>())
    { // it is const char * or char *, as std::string will go to construct(std::string str, ...)
        return ShmemPrimitive_::construct(val, heapPtr);
    }
    else if constexpr (isVector<T>::value)
    {
        throw std::runtime_error("Code should not reach here, it should go to the better match construct(std::vector<T> vec,...)");
    }
    else
    {
        throw std::runtime_error("Cannot convert non-primitive object to primitive");
    }
}

template <typename T>
size_t ShmemPrimitive<T>::construct(std::vector<T> vec, ShmemHeap *heapPtr)
{
    size_t size = vec.size();
    size_t offset = makeSpace(size, heapPtr);
    ShmemPrimitive<T> *ptr = reinterpret_cast<ShmemPrimitive<T> *>(heapPtr->heapHead() + offset);
    memcpy(ptr->getPtr(), vec.data(), size * sizeof(T));
    return offset;
}

template <typename T>
void ShmemPrimitive<T>::checkType() const
{
    if (isPrimitive<T>() && TypeEncoding<T>::value != this->type)
    {
        throw std::runtime_error("Type mismatch");
    }
}

template <typename T>
T ShmemPrimitive<T>::operator[](int index) const
{
    checkType();
    if (index < 0)
    { // negative index is relative to the end
        index += this->size;
    }
    if (index >= this->size || index < 0)
    {
        throw std::runtime_error("Index out of bounds");
    }
    return this->getPtr()[index];
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
void ShmemPrimitive<T>::set(int index, T value)
{
    checkType();
    if (index < 0)
    { // negative index is relative to the end
        index += this->size;
    }
    if (index >= this->size || index < 0)
    {
        throw std::runtime_error("Index out of bounds");
    }
    this->getPtr()[index] = value;
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

template <typename T>
std::string ShmemPrimitive<T>::toString(int maxElements) const
{
    std::string result;
    result.reserve(40);
    result.append("[P:").append(typeNames.at(this->type)).append(":").append(std::to_string(this->size)).append("]");
    const T *ptr = this->getPtr();
    for (int i = 0; i < std::min(this->size, maxElements); i++)
    {
        result += " " + std::to_string(ptr[i]);
    }
    if (this->size > 3)
    {
        result += " ...";
    }
    return result;
}

#endif // SHMEM_PRIMITIVE_TCC
