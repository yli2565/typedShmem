#ifndef SHMEM_DICT_TCC
#define SHMEM_DICT_TCC

#include "ShmemObj.h"
#include "ShmemDict.h"

inline const ShmemDictNode *ShmemDict::search(KeyType key) const
{
    return const_cast<ShmemDict *>(this)->search(key);
}

// Macro to define the construct functions for different key types
#define SHMEM_DICT_CONSTRUCT(keyType)                                                                  \
    template <typename T>                                                                              \
    size_t ShmemDict::construct(std::map<keyType, T> map, ShmemHeap *heapPtr)                          \
    {                                                                                                  \
        size_t dictOffset = ShmemDict::construct(heapPtr);                                             \
        ShmemDict *dict = reinterpret_cast<ShmemDict *>(ShmemObj::resolveOffset(dictOffset, heapPtr)); \
        for (auto &[key, val] : map)                                                                   \
        {                                                                                              \
            dict->insert(key, val, heapPtr);                                                           \
        }                                                                                              \
        return dictOffset;                                                                             \
    }

SHMEM_DICT_CONSTRUCT(int)
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

// __getitem__ alias
template <typename T>
T ShmemDict::operator[](int index) const
{
    return get<T>(index);
}

#endif // SHMEM_DICT_TCC
