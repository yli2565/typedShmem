#ifndef SHMEM_DICT_TCC
#define SHMEM_DICT_TCC

// #include "ShmemDict.h"

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

template <typename T>
void ShmemDict::insert(KeyType key, T val, ShmemHeap *heapPtr)
{
    insert(key, heapPtr->heapHead() + ShmemObj::construct<T>(val, heapPtr), heapPtr);
}

#endif // SHMEM_DICT_TCC
