#include "ShmemDict.h"
// Utlities
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
    nodeX->setRight(nodeY->left());
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
    nodeY->setLeft(nodeX);
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
    else
    {
        nodeX->parent()->setLeft(nodeY);
    }
    nodeY->setRight(nodeX);
    nodeX->setParent(nodeY);
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

void ShmemDict::transplant(ShmemDictNode *nodeU, ShmemDictNode *nodeV)
{
    if (nodeU->parent() == nullptr)
    {
        setRoot(nodeV);
    }
    else if (nodeU == nodeU->parent()->left())
    {
        nodeU->parent()->setLeft(nodeV);
    }
    else
    {
        nodeU->parent()->setRight(nodeV);
    }
    if (nodeV != NIL())
    {
        nodeV->setParent(nodeU->parent());
    }
}

ShmemDictNode *ShmemDict::minimum(ShmemDictNode *node) const
{
    while (node->left() != NIL())
    {
        node = node->left();
    }
    return node;
}

ShmemDictNode *ShmemDict::maximum(ShmemDictNode *node) const
{
    while (node->right() != NIL())
    {
        node = node->right();
    }
    return node;
}
ShmemDictNode *ShmemDict::findSuccessor(const ShmemDictNode *node) const
{
    // If the right subtree is not null, the successor is the minimum key in the right subtree
    if (node->right() != NIL())
    {
        return minimum(node->right());
    }

    // If there is no right subtree, the successor is an ancestor
    ShmemDictNode *p = node->parent();
    while (p != nullptr && node == p->right())
    {
        node = p;
        p = p->parent();
    }
    return p;
}

void ShmemDict::fixDelete(ShmemDictNode *nodeX)
{
    while (nodeX != root() && !nodeX->isRed())
    {
        if (nodeX == nodeX->parent()->left())
        {
            ShmemDictNode *nodeW = nodeX->parent()->right();

            if (nodeW->isRed())
            {
                nodeW->colorBlack();
                nodeX->parent()->colorRed();
                leftRotate(nodeX->parent());
                nodeW = nodeX->parent()->right();
            }

            if (!nodeW->left()->isRed() && !nodeW->right()->isRed())
            {
                nodeW->colorRed();
                nodeX = nodeX->parent();
            }
            else
            {
                if (!nodeW->right()->isRed())
                {
                    nodeW->left()->colorBlack();
                    nodeW->colorRed();
                    rightRotate(nodeW);
                    nodeW = nodeX->parent()->right();
                }

                nodeW->setColor(nodeX->parent()->getColor());
                nodeX->parent()->colorBlack();
                nodeW->right()->colorBlack();
                leftRotate(nodeX->parent());
                nodeX = root();
            }
        }
        else
        {
            ShmemDictNode *nodeW = nodeX->parent()->left();

            if (nodeW->isRed())
            {
                nodeW->colorBlack();
                nodeX->parent()->colorRed();
                rightRotate(nodeX->parent());
                nodeW = nodeX->parent()->left();
            }

            if (!nodeW->right()->isRed() && !nodeW->left()->isRed())
            {
                nodeW->colorRed();
                nodeX = nodeX->parent();
            }
            else
            {
                if (!nodeW->left()->isRed())
                {
                    nodeW->right()->colorBlack();
                    nodeW->colorRed();
                    leftRotate(nodeW);
                    nodeW = nodeX->parent()->left();
                }

                nodeW->setColor(nodeX->parent()->getColor());
                nodeX->parent()->colorBlack();
                nodeW->left()->colorBlack();
                rightRotate(nodeX->parent());
                nodeX = root();
            }
        }
    }
    nodeX->colorBlack();
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
        { // repeated key, replace the old data, don't increase the size
            ShmemObj::deconstruct(reinterpret_cast<Byte *>(current->data()) - heapPtr->heapHead(), heapPtr);
            current->setData(data);
            return;
        }
    }

    // We find a place for the new key, create a new node
    size_t newNodeOffset = ShmemDictNode::construct(key, heapPtr);
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

    // Increase the size
    this->size++;

    if (newNode->parent() == nullptr)
    {
        newNode->colorBlack();
        return;
    }
    if (newNode->parent()->parent() == nullptr)
        return;

    fixInsert(newNode);
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

// Traversal helpers

void ShmemDict::toStringHelper(const ShmemDictNode *node, int indent, std::ostringstream &resultStream, int currentElement, int maxElements) const
{
    if (node != this->NIL())
    {
        // Traverse left subtree
        toStringHelper(node->left(), indent, resultStream, currentElement, maxElements);

        // Append current node
        if (currentElement >= maxElements)
        {
            resultStream << std::string(indent, ' ') << "..." << "\n";
            return;
        }

        resultStream << std::string(indent, ' ')
                     << node->keyToString() << ": "
                     << node->data()->toString(indent ) << "\n";

        currentElement++;

        // Traverse right subtree
        toStringHelper(node->right(), indent, resultStream, currentElement, maxElements);
    }
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

void ShmemDict::keysHelper(const ShmemDictNode *node, std::vector<KeyType> &result, bool &allInt, bool &allString) const
{
    if (node == this->NIL())
        return;

    keysHelper(node->left(), result, allInt, allString);

    if (node->keyType() == Int)
        allString = false;
    else
        allInt = false;

    result.push_back(node->keyVal());

    keysHelper(node->right(), result, allInt, allString);
}

size_t ShmemDict::construct(ShmemHeap *heapPtr)
{
    size_t dictOffset = heapPtr->shmalloc(sizeof(ShmemDict));
    size_t NILOffset = ShmemDictNode::construct(NILKey, heapPtr);

    ShmemDictNode *NILPtr = reinterpret_cast<ShmemDictNode *>(resolveOffset(NILOffset, heapPtr));
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

// __len__
size_t ShmemDict::len() const
{
    return this->size;
}

// __getitem__
ShmemObj *ShmemDict::get(KeyType key) const
{
    const ShmemDictNode *result = search(key);
    if (result == nullptr)
        throw IndexError("Key not found");
    return result->data();
}

// __setitem__ implemented in .tcc, alias to insert()

// __delitem__
void ShmemDict::del(KeyType key, ShmemHeap *heapPtr)
{
    ShmemDictNode *nodeToDelete = search(key);
    if (nodeToDelete == nullptr || nodeToDelete == NIL())
    {
        throw IndexError("Key not found");
    }

    ShmemDictNode *nodeY = nodeToDelete; // Node to be deleted or moved
    ShmemDictNode *nodeX;                // Node that will replace nodeY
    bool originalColor = nodeY->isRed(); // Save the original color

    if (nodeToDelete->left() == NIL())
    {
        nodeX = nodeToDelete->right();
        transplant(nodeToDelete, nodeToDelete->right());
    }
    else if (nodeToDelete->right() == NIL())
    {
        nodeX = nodeToDelete->left();
        transplant(nodeToDelete, nodeToDelete->left());
    }
    else
    {
        nodeY = minimum(nodeToDelete->right()); // Find the minimum node in the right subtree
        originalColor = nodeY->isRed();
        nodeX = nodeY->right();

        if (nodeY->parent() == nodeToDelete)
        {
            nodeX->setParent(nodeY);
        }
        else
        {
            transplant(nodeY, nodeY->right());
            nodeY->setRight(nodeToDelete->right());
            nodeY->right()->setParent(nodeY);
        }

        transplant(nodeToDelete, nodeY);
        nodeY->setLeft(nodeToDelete->left());
        nodeY->left()->setParent(nodeY);
        nodeY->setColor(nodeToDelete->getColor());
    }

    // Free memory used by the nodeToDelete
    ShmemDictNode::deconstruct(reinterpret_cast<Byte *>(nodeToDelete) - heapPtr->heapHead(), heapPtr);

    // Decrease the size
    this->size--;

    if (!originalColor)
    {
        fixDelete(nodeX);
    }
}

// __contains__
bool ShmemDict::contains(KeyType key) const
{
    const ShmemDictNode *result = search(key);
    if (result == nullptr)
        return false;
    return true;
}

// __str__
std::string ShmemDict::toString(int indent, int maxElements) const
{
    std::ostringstream resultStream;

    maxElements = maxElements > 0 ? maxElements : this->size;

    resultStream << "(D:" << std::to_string(this->size) << ")" << "{\n";

    toStringHelper(this->root(), indent + 1, resultStream, 0, maxElements);

    resultStream << std::string(indent, ' ') << "}";

    return resultStream.str();
}

// __keys__
std::vector<KeyType> ShmemDict::keys(bool *allInt_, bool *allString_) const
{
    bool allInt = true;
    bool allString = true;
    std::vector<KeyType> result;
    result.reserve(this->size);
    keysHelper(this->root(), result, allInt, allString);
    if (allInt_)
        *allInt_ = allInt;
    if (allString_)
        *allString_ = allString;

    return result;
}

// Iterator related
KeyType ShmemDict::beginIdx() const
{
    const ShmemDictNode *min = minimum(this->root());
    return min->keyVal(); // if NIL, it is the end() iterator, and the whole dict is empty
}
KeyType ShmemDict::endIdx() const
{
    return this->NIL()->keyVal();
}

KeyType ShmemDict::nextIdx(KeyType index) const
{
    const ShmemDictNode *node = search(index);
    if (node == nullptr)
        throw IndexError("Cannot get next index of a non-existent key");
    const ShmemDictNode *successor = findSuccessor(node);
    if (successor == nullptr)
        throw StopIteration("End of dict");
    return successor->keyVal();
}