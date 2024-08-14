#include "ShmemObj.h"

ShmemAccessor::ShmemDictNode *ShmemAccessor::ShmemDict::root() const
{
    return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + rootOffset);
}

void ShmemAccessor::ShmemDict::setRoot(ShmemDictNode *node)
{
    rootOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
}

ShmemAccessor::ShmemDictNode *ShmemAccessor::ShmemDict::NIL() const
{
    return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + NILOffset);
}

void ShmemAccessor::ShmemDict::setNIL(ShmemDictNode *node)
{
    NILOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
}

void ShmemAccessor::ShmemDict::leftRotate(ShmemDictNode *nodeX)
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

void ShmemAccessor::ShmemDict::rightRotate(ShmemDictNode *nodeX)
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

void ShmemAccessor::ShmemDict::fixInsert(ShmemDictNode *nodeK)
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

std::string ShmemAccessor::ShmemDict::toStringHelper(ShmemDictNode *node, int indent)
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

void ShmemAccessor::ShmemDict::deconstructHelper(ShmemDictNode *node, ShmemHeap *heapPtr)
{
    if (node->hashedKey() != hashedNILKey)
    {
        deconstructHelper(node->left(), heapPtr);  // Destroy the left subtree
        deconstructHelper(node->right(), heapPtr); // Destroy the right subtree
        ShmemDictNode::deconstruct(reinterpret_cast<const Byte *>(node) - heapPtr->heapHead(), heapPtr);
    }
}

ShmemAccessor::ShmemDictNode *ShmemAccessor::ShmemDict::searchHelper(ShmemDictNode *node, int key)
{
    if (node == this->NIL() || node->hashedKey() == key)
        return node;
    if (key < node->hashedKey())
        return searchHelper(node->left(), key);
    else
        return searchHelper(node->right(), key);
}

size_t ShmemAccessor::ShmemDict::construct(ShmemHeap *heapPtr)
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

void ShmemAccessor::ShmemDict::deconstruct(size_t offset, ShmemHeap *heapPtr)
{
    // Do post-order traversal and remove all nodes
    ShmemDict *ptr = reinterpret_cast<ShmemDict *>(resolveOffset(offset, heapPtr));
    deconstructHelper(ptr->root(), heapPtr);
    ShmemAccessor::ShmemDictNode::deconstruct(reinterpret_cast<Byte *>(ptr->NIL()) - heapPtr->heapHead(), heapPtr);
    heapPtr->shfree(reinterpret_cast<Byte *>(ptr));
}

void ShmemAccessor::ShmemDict::insert(KeyType key, ShmemObj *data, ShmemHeap *heapPtr)
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

ShmemAccessor::ShmemObj *ShmemAccessor::ShmemDict::get(KeyType key)
{
    ShmemDictNode *result = search(key);
    if (result == nullptr)
        throw IndexError("Key not found");
    return result->data();
}

// std::string ShmemAccessor::ShmemDict::inorder(int indent)
// {
//     std::string result;
//     result.reserve(50);
//     result.append("\n").append(inorderHelper(root(), indent)).append("\n").append(std::string(indent, ' ')).append("}");
//     return result;
// }

ShmemAccessor::ShmemDictNode *ShmemAccessor::ShmemDict::search(KeyType key)
{
    ShmemDictNode *result = searchHelper(root(), hashIntOrString(key));
    if (result == NIL())
    {
        return nullptr;
    }
    return result;
}

std::string ShmemAccessor::ShmemDict::toString(ShmemAccessor::ShmemDict *dict, int indent)
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
