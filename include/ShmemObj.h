#ifndef SHMEM_OBJ_H
#define SHMEM_OBJ_H

#include "ShmemHeap.h"
#include "TypeEncodings.h"
#include <variant>
#include <vector>
#include <string>
#include <stdexcept>
#include <functional>

// Forward declarations
template <typename T>
class ShmemPrimitive;
class ShmemList;
class ShmemDict;
class ShmemAccessor;

class IndexError : public std::runtime_error
{
public:
    explicit IndexError(const std::string &message) : std::runtime_error(message){};
};

class ShmemObj
{
    friend class ShmemAccessor;
protected:
    size_t capacity() const;
    bool isBusy() const;
    void setBusy(bool b = true);
    int wait(int timeout = -1) const;
    ShmemHeap::BlockHeader *getHeader() const;

    static ShmemObj *resolveOffset(size_t offset, ShmemHeap *heapPtr);

public:
    int type;
    int size;

    ShmemObj() = delete;

    // Type (Special interface)
    int typeId() const;
    std::string typeStr() const;

    template <typename T>
    static size_t construct(const T &value, ShmemHeap *heapPtr);
    static void deconstruct(size_t offset, ShmemHeap *heapPtr);

    static std::string toString(ShmemObj *obj, int indent = 0, int maxElements = 4);
};
// #include "ShmemPrimitive.h"
// #include "ShmemDict.h"
// #include "ShmemList.h"

// Include the template implementation file
#include "ShmemObj.tcc"
#include "ShmemPrimitive.tcc"
#include "ShmemList.tcc"
#include "ShmemDict.tcc"

#endif // SHMEM_OBJ_H
