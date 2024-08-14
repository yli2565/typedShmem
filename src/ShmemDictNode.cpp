#include "ShmemObj.h"

// ShmemDictNode methods
size_t ShmemAccessor::ShmemDictNode::construct(KeyType key, ShmemHeap *heapPtr)
{
    int hashKey = hashIntOrString(key);
    size_t offset = heapPtr->shmalloc(sizeof(ShmemDictNode));
    ShmemDictNode *ptr = static_cast<ShmemDictNode *>(resolveOffset(offset, heapPtr));
    ptr->type = DictNode;
    ptr->size = -1; // DictNode doesn't need to record size
    ptr->colorRed();
    ptr->setLeft(nullptr);
    ptr->setRight(nullptr);
    ptr->setParent(nullptr);
    ptr->setData(nullptr);
    if (std::holds_alternative<std::string>(key))
        ptr->keyOffset = ShmemPrimitive<char>::construct(std::get<std::string>(key), heapPtr) - offset;
    else
        ptr->keyOffset = ShmemPrimitive<int>::construct(std::get<int>(key), heapPtr) - offset;
    return offset;
}

// void ShmemAccessor::ShmemDictNode::deconstruct(ShmemDictNode *node, ShmemHeap *heapPtr)
// {
//     Byte *heapHead = heapPtr->heapHead();
//     ShmemObj::deconstruct(reinterpret_cast<const Byte *>(node->key()) - heapHead, heapPtr);
//     ShmemObj::deconstruct(reinterpret_cast<const Byte *>(node->data()) - heapHead, heapPtr);
//     heapPtr->shfree(reinterpret_cast<Byte *>(node));
// }

void ShmemAccessor::ShmemDictNode::deconstruct(size_t offset, ShmemHeap *heapPtr)
{
    ShmemDictNode *ptr = static_cast<ShmemDictNode *>(resolveOffset(offset, heapPtr));
    Byte *heapHead = heapPtr->heapHead();

    ShmemObj::deconstruct(reinterpret_cast<const Byte *>(ptr->key()) - heapHead, heapPtr);
    ShmemObj::deconstruct(reinterpret_cast<const Byte *>(ptr->data()) - heapHead, heapPtr);
    heapPtr->shfree(reinterpret_cast<Byte *>(ptr));
}

bool ShmemAccessor::ShmemDictNode::isRed() const
{
    return color == 0;
}

bool ShmemAccessor::ShmemDictNode::isBlack() const
{
    return color == 1;
}

void ShmemAccessor::ShmemDictNode::colorRed()
{
    this->color = 0;
}

void ShmemAccessor::ShmemDictNode::colorBlack()
{
    this->color = 1;
}

ShmemAccessor::ShmemDictNode *ShmemAccessor::ShmemDictNode::left() const
{
    if (leftOffset == NPtr)
        return nullptr;
    else
        return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + leftOffset);
}

void ShmemAccessor::ShmemDictNode::setLeft(ShmemDictNode *node)
{
    if (node == nullptr)
        leftOffset = NPtr; // Set offset to an impossible value to indicate nullptr
    else
        leftOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
}

ShmemAccessor::ShmemDictNode *ShmemAccessor::ShmemDictNode::right() const
{
    if (rightOffset == NPtr)
        return nullptr;
    else
        return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + rightOffset);
}

void ShmemAccessor::ShmemDictNode::setRight(ShmemDictNode *node)
{
    if (node == nullptr)
        rightOffset = NPtr; // Set offset to an impossible value to indicate nullptr
    else
        rightOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
}

ShmemAccessor::ShmemDictNode *ShmemAccessor::ShmemDictNode::parent() const
{
    if (parentOffset == NPtr)
        return nullptr;
    else
        return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + parentOffset);
}

void ShmemAccessor::ShmemDictNode::setParent(ShmemDictNode *node)
{
    if (node == nullptr)
        parentOffset = NPtr; // Set offset to an impossible value to indicate nullptr
    else
        parentOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
}

const ShmemAccessor::ShmemObj *ShmemAccessor::ShmemDictNode::key() const
{
    return reinterpret_cast<ShmemObj *>(reinterpret_cast<uintptr_t>(this) + keyOffset);
}

KeyType ShmemAccessor::ShmemDictNode::keyVal() const
{
    int keyType = key()->type;
    if (keyType == String)
        return reinterpret_cast<const ShmemPrimitive<char> *>(key())->operator std::string();
    else if (keyType == Int)
        return reinterpret_cast<const ShmemPrimitive<int> *>(key())->operator int();
    else
        throw std::runtime_error("Unknown key type");
}

ShmemAccessor::ShmemObj *ShmemAccessor::ShmemDictNode::data() const
{
    if (dataOffset == NPtr)
        return nullptr;
    else
        return reinterpret_cast<ShmemObj *>(reinterpret_cast<uintptr_t>(this) + dataOffset);
}

void ShmemAccessor::ShmemDictNode::setData(ShmemObj *obj)
{
    if (obj == nullptr)
        dataOffset = NPtr; // Set offset to an impossible value to indicate nullptr
    else
        dataOffset = reinterpret_cast<uintptr_t>(obj) - reinterpret_cast<uintptr_t>(this);
}

int ShmemAccessor::ShmemDictNode::hashedKey() const
{
    int keyType = key()->type;
    if (keyType == String)
        return std::hash<std::string>{}(reinterpret_cast<const ShmemPrimitive<char> *>(key())->operator std::string());
    else if (keyType == Int)
        return std::hash<int>{}(reinterpret_cast<const ShmemPrimitive<int> *>(key())->operator int());
    else
        throw std::runtime_error("Unknown key type");
}

std::string ShmemAccessor::ShmemDictNode::keyToString() const
{
    int keyType = key()->type;
    if (keyType == String)
        return reinterpret_cast<const ShmemPrimitive<char> *>(key())->operator std::string();
    else if (keyType == Int)
        return std::to_string(reinterpret_cast<const ShmemPrimitive<int> *>(key())->operator int());
    else
        throw std::runtime_error("Unknown key type");
}
