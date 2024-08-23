#ifndef SHMEM_DICT_TCC
#define SHMEM_DICT_TCC

#include "ShmemObj.h"
#include "ShmemDict.h"

inline const ShmemDictNode *ShmemDict::search(KeyType key) const
{
    return const_cast<ShmemDict *>(this)->search(key);
}

// Macro to define the construct functions for different key types
#define SHMEM_DICT_CONSTRUCT(keyType)                                                                                 \
    template <typename T>                                                                                             \
    size_t ShmemDict::construct(std::map<keyType, T> map, ShmemHeap *heapPtr)                                         \
    {                                                                                                                 \
        size_t dictOffset = ShmemDict::construct(heapPtr);                                                            \
        ShmemDict *dict = reinterpret_cast<ShmemDict *>(ShmemObj::resolveOffset(dictOffset, heapPtr));                \
        for (auto &[key, val] : map)                                                                                  \
        {                                                                                                             \
            ShmemObj *newObj = reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + ShmemObj::construct(val, heapPtr)); \
            dict->insert(key, newObj, heapPtr);                                                                       \
        }                                                                                                             \
        return dictOffset;                                                                                            \
    }

template <typename T>
size_t ShmemDict::construct(std::map<int, T> map, ShmemHeap *heapPtr)
{
    size_t dictOffset = ShmemDict::construct(heapPtr);
    ShmemDict *dict = reinterpret_cast<ShmemDict *>(ShmemObj::resolveOffset(dictOffset, heapPtr));
    for (auto &[key, val] : map)
    {
        ShmemObj *newObj = reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + ShmemObj::construct(val, heapPtr));
        dict->insert(key, newObj, heapPtr);
    }
    return dictOffset;
}

// SHMEM_DICT_CONSTRUCT(int)
SHMEM_DICT_CONSTRUCT(std::string)
SHMEM_DICT_CONSTRUCT(KeyType)

#undef SHMEM_DICT_CONSTRUCT

// __setitem__
template <typename T>
inline void ShmemDict::set(const T &value, KeyType key, ShmemHeap *heapPtr)
{
    insert(key, reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + ShmemObj::construct(value, heapPtr)), heapPtr);
}

template <typename T>
ShmemDictNode *ShmemDict::searchKeyHelper(ShmemDictNode *node, const T &value)
{
    if (node == this->NIL())
        return nullptr;

    ShmemDictNode *left = searchKeyHelper(node->left(), value);
    if (left != nullptr)
        return left;

    if (node->data() == value)
    {
        return node;
    }

    ShmemDictNode *right = searchKeyHelper(node->right(), value);
    if (right != nullptr)
        return right;
    return nullptr;
}

// __keys__
template <typename T>
KeyType ShmemDict::key(const T &value) const
{
    ShmemDictNode *target = const_cast<ShmemDict *>(this)->searchKeyHelper(const_cast<ShmemDict *>(this)->root(), value);
    return target->keyVal();
}

// Convertors
template <typename T>
ShmemDict::operator T() const
{
    if constexpr (isMap<T>::value)
    {
        using mapDataType = typename unwrapMapType<T>::type;
        using keyDataType = typename unwrapMapType<T>::keyType;

        bool allInt = true;
        bool allString = true;
        std::map<KeyType, mapDataType> tmpResult;
        T result;
        convertHelper(this->root, tmpResult, allInt, allString);
        if constexpr (std::is_same_v<keyDataType, int>)
        {
            if (!allInt)
                throw std::runtime_error("ShmemDict: All keys must be int");
            for (auto &[key, val] : tmpResult)
            {
                result[std::get<int>(key)] = val;
            }
        }
        else if constexpr (std::is_same_v<keyDataType, std::string>)
        {
            if (!allString)
                throw std::runtime_error("ShmemDict: All keys must be string");
            for (auto &[key, val] : tmpResult)
            {
                result[std::get<std::string>(key)] = val;
            }
        }
        else if constexpr (std::is_same_v<keyDataType, KeyType>)
        {
            result = tmpResult;
        }
        else
            throw std::runtime_error("ShmemDict: Unsupported key type");

        return result;
    }
    else
    {
        throw std::runtime_error("ShmemDict: Unsupported type conversion");
    }
}

template <typename T>
void ShmemDict::convertHelper(ShmemDictNode *node, std::map<KeyType, T> &result, bool &allInt, bool &allString) const
{
    if (node == this->NIL())
        return;

    convertHelper(node->left(), result, allInt, allString);

    if (node->keyType() == Int)
        allString = false;
    else
        allInt = false;

    result[node->keyVal()] = node->data()->operator T();

    convertHelper(node->right(), result, allInt, allString);
}

// __getitem__ alias
template <typename T>
T ShmemDict::operator[](int index) const
{
    return get<T>(index);
}

template <typename T>
bool ShmemDict::operator==(const T &val) const
{
    if constexpr (isObjPtr<T>::value)
    {
        if (val->type != this->type)
            return false;
        // Ensure T = const ShmemDict*

        if (this->size != val->size)
            return false;

        return this->toString() == reinterpret_cast<const ShmemDict *>(val)->toString();
    }
    else if constexpr (isMap<T>::value)
    {
        return this->operator T == val;
    }
    else
    {
        throw std::runtime_error("Comparison of " + typeNames.at(this->type) + " with " + typeName<T>() + " is not allowed");
    }
}
#endif // SHMEM_DICT_TCC
