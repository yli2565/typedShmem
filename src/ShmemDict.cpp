#include "ShmemDict.h"

int hashIntOrString(KeyType key)
{
    int hashCode = 0;
    if (std::holds_alternative<std::string>(key))
    {
        hashCode = std::hash<std::string>{}(std::get<std::string>(key));
    }
    else
    {
        hashCode = std::hash<int>{}(std::get<int>(key));
    }
    return hashCode;
}

ShmemDictNode *ShmemDict::root() const
{
    return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + rootOffset);
}

void ShmemDict::setRoot(ShmemDictNode *node)
{
    rootOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
}

ShmemDictNode *ShmemDict::NIL() const
{
    return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + NILOffset);
}

void ShmemDict::setNIL(ShmemDictNode *node)
{
    NILOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
}

void ShmemDict::leftRotate(ShmemDictNode *nodeX)
{
    ShmemDictNode *nodeY = nodeX->right();
    if (nodeY->left() != this->NIL())
    {
        nodeY->left()->setParent(nodeX);
    }
    nodeY->setParent(nodeX->parent());
    if (nodeX->parent() == nullptr)
    {
        this->setRoot(nodeY);
    }
    else if (nodeX == nodeX->parent()->left())
    {
        nodeX->parent()->setLeft(nodeY);
    }
    else
    {
        nodeX->parent()->setRight(nodeY);
    }
    nodeX->setLeft(nodeX);
    nodeX->setParent(nodeY);
}

void ShmemDict::rightRotate(ShmemDictNode *nodeX)
{
    ShmemDictNode *nodeY = nodeX->left();
    nodeX->setLeft(nodeY->right());
    if (nodeY->right() != this->NIL())
    {
        nodeY->right()->setParent(nodeX);
    }
    nodeY->setParent(nodeX->parent());
    if (nodeX->parent() == nullptr)
    {
        this->setRoot(nodeY);
    }
    else if (nodeX == nodeX->parent()->right())
    {
        nodeX->parent()->setRight(nodeY);
    }
}

void ShmemDict::fixInsert(ShmemDictNode *nodeK)
{
    while (nodeK != root() && nodeK->parent()->isRed())
    {
        if (nodeK->parent() == nodeK->parent()->parent()->left())
        {
            ShmemDictNode *nodeU = nodeK->parent()->parent()->right(); // uncle

            if (nodeU->isRed())
            {
                nodeK->parent()->colorBlack();
                nodeU->colorBlack();
                nodeK->parent()->parent()->colorRed();
                nodeK = nodeK->parent()->parent();
            }
            else
            {
                if (nodeK == nodeK->parent()->right())
                {
                    nodeK = nodeK->parent();
                    leftRotate(nodeK);
                }
                nodeK->parent()->colorBlack();
                nodeK->parent()->parent()->colorRed();
                rightRotate(nodeK->parent()->parent());
            }
        }
        else
        {
            ShmemDictNode *nodeU = nodeK->parent()->parent()->left(); // uncle
            if (nodeU->isRed())
            {
                nodeK->parent()->colorBlack();
                nodeU->colorBlack();
                nodeK->parent()->parent()->colorRed();
                nodeK = nodeK->parent()->parent();
            }
            else
            {
                if (nodeK == nodeK->parent()->left())
                {
                    nodeK = nodeK->parent();
                    rightRotate(nodeK);
                }
                nodeK->parent()->colorBlack();
                nodeK->parent()->parent()->colorRed();
                leftRotate(nodeK->parent()->parent());
            }
        }
    }
    root()->colorBlack();
}

size_t ShmemDict::construct(ShmemHeap *heapPtr)
{
    size_t NILOffset = ShmemDictNode::construct(NILKey, heapPtr);
    ShmemDictNode *NILPtr = reinterpret_cast<ShmemDictNode *>(resolveOffset(NILOffset, heapPtr));

    size_t dictOffset = heapPtr->shmalloc(sizeof(ShmemDict));
    ShmemDict *dictPtr = reinterpret_cast<ShmemDict *>(resolveOffset(dictOffset, heapPtr));

    NILPtr->colorBlack();
    NILPtr->setLeft(NILPtr);
    NILPtr->setRight(NILPtr);

    dictPtr->size = 0;
    dictPtr->type = Dict;

    dictPtr->setRoot(NILPtr);
    dictPtr->setNIL(NILPtr);

    return dictOffset;
}

void ShmemDict::deconstruct(size_t offset, ShmemHeap *heapPtr)
{
    // Do post-order traversal and remove all nodes
    ShmemDict *ptr = reinterpret_cast<ShmemDict *>(resolveOffset(offset, heapPtr));
    deconstructHelper(ptr->root(), heapPtr);
    ShmemDictNode::deconstruct(reinterpret_cast<Byte *>(ptr->NIL()) - heapPtr->heapHead(), heapPtr);
    heapPtr->shfree(reinterpret_cast<Byte *>(ptr));
}

void ShmemDict::insert(KeyType key, ShmemObj *data, ShmemHeap *heapPtr)
{
    int hashKey = hashIntOrString(key);

    ShmemDictNode *parent = nullptr;
    ShmemDictNode *current = root();

    while (current != NIL())
    {
        parent = current;
        if (hashKey < current->hashedKey())
            current = current->left();
        else if (hashKey > current->hashedKey())
            current = current->right();
        else
        { // repeated key, replace the old data
            ShmemObj::deconstruct(reinterpret_cast<Byte *>(current->data()) - heapPtr->heapHead(), heapPtr);
            current->setData(data);
            return;
        }
    }

    // We find a place for the new key, create a new node
    size_t newNodeOffset = ShmemDictNode::construct(hashKey, heapPtr);
    ShmemDictNode *newNode = static_cast<ShmemDictNode *>(resolveOffset(newNodeOffset, heapPtr));
    newNode->setData(data);
    newNode->setLeft(NIL());
    newNode->setRight(NIL());
    newNode->setParent(parent);

    if (parent == nullptr)
        setRoot(newNode);
    else if (hashKey < parent->hashedKey())
        parent->setLeft(newNode);
    else if (hashKey > parent->hashedKey())
        parent->setRight(newNode);
    else
    {
        throw std::runtime_error("hashKey == parent->key, and code should not reach here");
    }

    if (newNode->parent() == nullptr)
    {
        newNode->colorBlack();
        return;
    }
    if (newNode->parent()->parent() == nullptr)
        return;

    fixInsert(newNode);
}

ShmemObj *ShmemDict::get(KeyType key)
{
    ShmemDictNode *result = search(key);
    if (result == nullptr)
        throw IndexError("Key not found");
    return result->data();
}

ShmemDictNode *ShmemDict::search(KeyType key)
{
    ShmemDictNode *result = searchHelper(root(), hashIntOrString(key));
    if (result == NIL())
    {
        return nullptr;
    }
    return result;
}

std::string ShmemDict::toString(ShmemDict *dict, int indent)
{
    std::string result = dict->toStringHelper(dict->root(), indent);
    if (result.size() < 40)
    {
        return "{\n" + result + "\n" + std::string(indent, ' ') + "}";
    }
    else
    {
        return "{" + result + "}";
    }
}

// Traversal helpers
std::string ShmemDict::toStringHelper(ShmemDictNode *node, int indent)
{
    if (node != this->NIL())
    {
        std::string left = toStringHelper(node->left());
        if (left != "")
            left += "\n";
        std::string current;
        current.append(std::string(indent, ' ')).append(node->keyToString()).append(": ").append(std::to_string(node->data()->type));
        std::string right = toStringHelper(node->right());
        if (right != "")
            right = "\n" + right;
        left.append(current).append(right);
        return left;
    }
    return "";
}

void ShmemDict::deconstructHelper(ShmemDictNode *node, ShmemHeap *heapPtr)
{
    if (node->hashedKey() != hashedNILKey)
    {
        deconstructHelper(node->left(), heapPtr);  // Destroy the left subtree
        deconstructHelper(node->right(), heapPtr); // Destroy the right subtree
        ShmemDictNode::deconstruct(reinterpret_cast<const Byte *>(node) - heapPtr->heapHead(), heapPtr);
    }
}

ShmemDictNode *ShmemDict::searchHelper(ShmemDictNode *node, int key)
{
    if (node == this->NIL() || node->hashedKey() == key)
        return node;
    if (key < node->hashedKey())
        return searchHelper(node->left(), key);
    else
        return searchHelper(node->right(), key);
}