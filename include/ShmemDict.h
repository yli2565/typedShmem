#include "ShmemObj.h"
// Please keep this inclusion before header guard, which make the order of include correct

#ifndef SHMEM_DICT_H
#define SHMEM_DICT_H
#include <string>
#include <variant>
#include <map>
#include <stdexcept>

using KeyType = std::variant<int, std::string>;

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
    void colorRed();
    void colorBlack();

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

    int hashedKey() const;
    std::string keyToString() const;
};

class ShmemDict : public ShmemObj
{
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

    void insert(KeyType key, ShmemObj *data, ShmemHeap *heapPtr);

    ShmemDictNode *search(KeyType key);

protected:
    std::string toStringHelper(ShmemDictNode *node, int indent = 0);
    static void deconstructHelper(ShmemDictNode *node, ShmemHeap *heapPtr);
    ShmemDictNode *searchHelper(ShmemDictNode *node, int key);

public:
    static size_t construct(ShmemHeap *heapPtr);
    
    template <typename T>
    static size_t construct(std::map<int, T> map, ShmemHeap *heapPtr);
    
    template <typename T>
    static size_t construct(std::map<std::string, T> map, ShmemHeap *heapPtr);
    
    template <typename T>
    static size_t construct(std::map<KeyType, T> map, ShmemHeap *heapPtr);
    
    static void deconstruct(size_t offset, ShmemHeap *heapPtr);

    template <typename T>
    void insert(KeyType key, T val, ShmemHeap *heapPtr);

    ShmemObj *get(KeyType key);

    static std::string toString(ShmemDict *dict, int indent = 0);
};

// Include the template implementation file
#include "ShmemDict.tcc"

#endif // SHMEM_DICT_H
