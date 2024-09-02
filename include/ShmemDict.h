#include "ShmemObj.h"
// Please keep this inclusion before header guard, which make the order of include correct

#ifndef SHMEM_DICT_H
#define SHMEM_DICT_H
#include <string>
#include <variant>
#include <map>
#include <stdexcept>

int hashIntOrString(KeyType key);

const static KeyType NILKey = "NILKey:js82nfd-";
const static int hashedNILKey = hashIntOrString(NILKey);

class ShmemDictNode : public ShmemObj
{
public:
    ptrdiff_t leftOffset, rightOffset, parentOffset;
    ptrdiff_t keyOffset;
    ptrdiff_t dataOffset;
    int color;

    static size_t construct(KeyType key, ShmemHeap *heapPtr);

    static void deconstruct(size_t offset, ShmemHeap *heapPtr);

    bool isRed() const;
    bool isBlack() const;
    int getColor() const;
    void colorRed();
    void colorBlack();
    void setColor(int color);

    ShmemDictNode *left() const;
    void setLeft(ShmemDictNode *node);
    ShmemDictNode *right() const;
    void setRight(ShmemDictNode *node);
    ShmemDictNode *parent() const;
    void setParent(ShmemDictNode *node);

    const ShmemObj *key() const;
    KeyType keyVal() const;
    ShmemObj *data() const;
    void setData(ShmemObj *obj);

    int keyType() const;
    int dataType() const;
    int hashedKey() const;
    std::string keyToString() const;
};

class ShmemDict : public ShmemObj
{
    friend class ShmemAccessor;

protected:
    ptrdiff_t rootOffset;
    ptrdiff_t NILOffset;

    ShmemDictNode *root() const;
    void setRoot(ShmemDictNode *node);
    ShmemDictNode *NIL() const;
    void setNIL(ShmemDictNode *node);

    void leftRotate(ShmemDictNode *nodeX);
    void rightRotate(ShmemDictNode *nodeX);
    void fixInsert(ShmemDictNode *nodeK);
    void transplant(ShmemDictNode *nodeU, ShmemDictNode *nodeV);
    ShmemDictNode *minimum(ShmemDictNode *node) const;
    ShmemDictNode *maximum(ShmemDictNode *node) const;
    ShmemDictNode *findSuccessor(const ShmemDictNode *node) const;
    void fixDelete(ShmemDictNode *nodeX);

    void insert(KeyType key, ShmemObj *data, ShmemHeap *heapPtr);

    ShmemDictNode *search(KeyType key);
    const ShmemDictNode *search(KeyType key) const;

    // Traversal helpers
    void toStringHelper(const ShmemDictNode *node, int indent, std::ostringstream &resultStream, int currentElement, int maxElements) const;
    static void deconstructHelper(ShmemDictNode *node, ShmemHeap *heapPtr);
    ShmemDictNode *searchHelper(ShmemDictNode *node, int key);
    template <typename T>
    ShmemDictNode *searchKeyHelper(ShmemDictNode *node, const T &value);
    void keysHelper(const ShmemDictNode *node, std::vector<KeyType> &result, bool &allInt, bool &allString) const;
    template <typename T>
    void convertHelper(ShmemDictNode *node, std::map<KeyType, T> &result, bool &allInt, bool &allString) const;
    void toPyObjectHelper(ShmemDictNode *node, pybind11::dict &result) const;

public:
    static size_t construct(ShmemHeap *heapPtr);

    template <typename keyType, typename T>
    static size_t construct(std::map<keyType, T> map, ShmemHeap *heapPtr);

    static size_t construct(pybind11::dict pythonDict, ShmemHeap *heapPtr);
    static size_t construct(pybind11::object pythonObj, ShmemHeap *heapPtr);

    static void deconstruct(size_t offset, ShmemHeap *heapPtr);

    // __len__
    size_t len() const;

    // __getitem__
    ShmemObj *get(KeyType key) const;

    // __setitem__ (only for assign)
    template <typename T>
    void set(const T &value, KeyType key, ShmemHeap *heapPtr);

    // __delitem__
    void del(KeyType key, ShmemHeap *heapPtr);

    // __contains__
    bool contains(KeyType key) const;

    // __key__ (similar to __index__)
    template <typename T>
    KeyType key(const T &value) const;

    // __str__
    std::string toString(int indent = 0, int maxElements = -1) const;

    // Dict interface

    std::vector<KeyType> keys(bool *allInt = nullptr, bool *allStr = nullptr) const;

    // std::vector<ShmemObj *> values() const;
    // Not implemented in this way, as providing direct ptr to ShmemObj is dangerous
    // We should provide a list of keys and let ShmemAccessor generate a key based ShmemAccessor List

    // Iterator Interface
    KeyType beginIdx() const;
    KeyType endIdx() const;
    KeyType nextIdx(KeyType index) const;

    // Converters
    template <typename T>
    operator T() const;

    operator pybind11::dict() const;
    operator pybind11::object() const;

    // Aliases
    template <typename T>
    T operator[](int index) const; // get alias

    template <typename T>
    T get(KeyType key) const; // get + convert alias

    // Arithmetic Interface
    template <typename T>
    bool operator==(const T &val) const;
};

// Include the template implementation file
#include "ShmemDict.tcc"

#endif // SHMEM_DICT_H
