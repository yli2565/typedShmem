#ifndef SHMEM_OBJ_H
#define SHMEM_OBJ_H

#include "ShmemHeap.h"
#include "TypeEncodings.h"
#include <variant>
#include <vector>
#include <string>
#include <stdexcept>
#include <functional>

using KeyType = std::variant<int, std::string>;

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

class ConversionError : public std::runtime_error
{
public:
    explicit ConversionError(const std::string &message) : std::runtime_error(message){};
};

class StopIteration : public std::runtime_error
{
public:
    explicit StopIteration(const std::string &message) : std::runtime_error(message){};
};

class PYBIND11_EXPORT ShmemObjInitializer {
public:
    const int typeId;

    ShmemObjInitializer(int typeId, const pybind11::object &initialVal = pybind11::none());
    pybind11::object getInitialVal() const;

private:
    const pybind11::object initialVal;
};

class ShmemObj
{
    friend class ShmemAccessor;

protected:
    size_t capacity() const;
    bool isBusy() const;
    void setBusy(bool b = true);
    int wait(int timeout = -1) const;
    void acquire(int timeout = -1);
    void release();
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

    // __str__
    std::string toString(int indent = 0, int maxElements = -1) const;

    // Iterator related
    KeyType beginIdx() const;
    KeyType endIdx() const;
    KeyType nextIdx(KeyType index) const;

    // Converters
    template <typename T>
    operator T() const;

    operator pybind11::object() const;

    // Arithmetic
    template <typename T>
    bool operator==(const T &val) const;

    bool operator==(const char *val) const;
};

template <typename T>
struct isObjPtr
{
    static const bool value = std::is_pointer<T>::value &&
                              std::is_base_of<ShmemObj, typename std::remove_pointer<T>::type>::value;
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
