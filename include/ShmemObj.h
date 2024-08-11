#include "ShmemHeap.h"
#include "typeEncodings.h"
#include <variant>
#include <vector>
#include <string>

using KeyType = std::variant<int, std::string>;

const static KeyType NILKey = "NILKey:js82nfd-"; // Something random
const static int hashedNILKey = hashIntOrString(NILKey);

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

class ShmemAccessor
{
private:
    struct ShmemObj
    {
        int type;
        int size;

        size_t capacity() const
        {
            return *(reinterpret_cast<const size_t *>(this) - 1) & ~0x111;
        }
        static ShmemObj *resolveOffset(size_t offset, ShmemHeap *heapPtr)
        {
            return reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + offset);
        }
    };

    template <typename T>
    struct ShmemPrimitive : public ShmemObj
    {
        static size_t construct(size_t size, ShmemHeap *heapPtr)
        {
            size_t offset = heapPtr->shmalloc(sizeof(ShmemPrimitive) + size * sizeof(T));
            ShmemObj *ptr = resolveOffset(offset, heapPtr);
            ptr->type = TypeEncoding<T>::value;
            ptr->size = size;
            return offset;
        }
        
        static size_t construct(std::vector<T> vec, ShmemHeap *heapPtr)
        {
            size_t size = vec.size();
            size_t offset = heapPtr->shmalloc(sizeof(ShmemPrimitive) + size * sizeof(T));
            ShmemPrimitive<T> *ptr = static_cast<ShmemPrimitive<T> *>(resolveOffset(offset, heapPtr));
            ptr->type = TypeEncoding<T>::value;
            ptr->size = size;
            memcpy(ptr->getPtr(), vec.data(), size * sizeof(T));
            return offset;
        }

        static void deconstruct(size_t offset, ShmemHeap *heapPtr)
        {
            heapPtr->shfree(reinterpret_cast<Byte *>(resolveOffset(offset, heapPtr)));
        }
        size_t resize(size_t newSize, ShmemHeap *heapPtr)
        {
            int oldType = this->type;
            if (TypeEncoding<T>::value != oldType)
            {
                // info :converting from one type to another
                printf("Type mismatch\n");
            }
            size_t newOffset = heapPtr->shrealloc(this->offset, sizeof(ShmemPrimitive) + newSize * sizeof(T));
            ShmemObj *ptr = resolveOffset(newOffset, heapPtr);
            ptr->type = TypeEncoding<T>::value;
            ptr->size = size;
            return newOffset;
        }

        void checkType() const
        {
            if (isPrimitive<T>() && TypeEncoding<T>::value != this->type)
            {
                throw std::runtime_error("Type mismatch");
            }
        }
        T operator[](int index) const
        {
            checkType();
            if (index < 0)
            { // negative index is relative to the end
                index += this->size;
            }
            if (index >= this->size || index < 0)
            {
                throw std::runtime_error("Index out of bounds");
            }
            return this->getPtr()[index];
        }
        int find(T value) const
        {
            checkType();
            if (this->size == 0)
            {
                return -1;
            }
            T *ptr = this->getPtr();
            for (int i = 0; i < this->size; i++)
            {
                if (ptr[i] == value)
                {
                    return i;
                }
            }
            return -1;
        }
        void set(int index, T value)
        {
            checkType();
            if (index < 0)
            { // negative index is relative to the end
                index += this->size;
            }
            if (index >= this->size || index < 0)
            {
                throw std::runtime_error("Index out of bounds");
            }
            this->getPtr()[index] = value;
        }

        // Accessor
        operator T() const
        {
            T *ptr = this->getPtr();
            if (this->size != 1)
            {
                printf("Size is not 1, but you are only accessing the first element\n");
            }
            return *ptr;
        }

        operator std::vector<T>() const
        {
            T *ptr = this->getPtr();
            std::vector<T> vec(ptr, ptr + this->size);
            return vec;
        }

        void copyTo(T *target, int size = -1) const
        {
            T *ptr = this->getPtr();
            if (size == -1)
            {
                size = this->size;
            }
            if (size > this->size)
            {
                throw std::runtime_error("Size is larger than the object");
            }
            std::copy(ptr, ptr + size, target);
        }

        // Helper functions
        const T *getPtr() const
        {
            checkType();
            return static_cast<const T *>(reinterpret_cast<const Byte *>(this) + sizeof(ShmemObj));
        }
        T *getPtr()
        {
            checkType();
            return static_cast<T *>(reinterpret_cast<Byte *>(this) + sizeof(ShmemObj));
        }
    };

    struct ShmemDictNode : public ShmemObj
    {
        ptrdiff_t leftOffset, rightOffset, parentOffset;
        ptrdiff_t dataOffset;
        int key;
        int color;

        static size_t construct(KeyType key, ShmemHeap *heapPtr)
        {
            int hashKey = hashIntOrString(key);
            size_t offset = heapPtr->shmalloc(sizeof(ShmemDictNode));
            ShmemDictNode *ptr = static_cast<ShmemDictNode *>(resolveOffset(offset, heapPtr));
            ptr->type = DictNode;
            ptr->size = -1; // DictNode don't need to record size
            ptr->colorRed();
            ptr->setLeft(nullptr);
            ptr->setRight(nullptr);
            ptr->setParent(nullptr);
            ptr->setData(nullptr);
            ptr->key = hashKey;
            return offset;
        }
        static void deconstruct(size_t offset, ShmemHeap *heapPtr)
        {
            ShmemDictNode *ptr = static_cast<ShmemDictNode *>(resolveOffset(offset, heapPtr));
            heapPtr->shfree(reinterpret_cast<Byte *>(ptr->data()));
            heapPtr->shfree(reinterpret_cast<Byte *>(ptr));
        }
        // To save memory, I encode the red bit in the left pointer
        bool isRed() const
        {
            return color == 0;
        }

        bool isBlack() const
        {
            return color == 1;
        }

        bool colorRed()
        {
            this->color = 0;
        }
        bool colorBlack()
        {
            this->color = 1;
        }

        ShmemDictNode *left() const
        {
            if (leftOffset == NPtr)
                return nullptr;
            else
                return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + leftOffset);
        }

        void setLeft(ShmemDictNode *node)
        {
            if (node == nullptr)
                leftOffset = NPtr; // Set offset to an impossible value to indicate nullptr
            else
                leftOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
        }
        ShmemDictNode *right() const
        {
            if (rightOffset == NPtr)
                return nullptr;
            else
                return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + rightOffset);
        }

        void setRight(ShmemDictNode *node)
        {
            if (node == nullptr)
                rightOffset = NPtr; // Set offset to an impossible value to indicate nullptr
            else
                rightOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
        }

        ShmemDictNode *parent() const
        {
            if (parentOffset == NPtr)
                return nullptr;
            else
                return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + parentOffset);
        }

        void setParent(ShmemDictNode *node)
        {
            if (node == nullptr)
                parentOffset = NPtr; // Set offset to an impossible value to indicate nullptr
            else
                parentOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
        }

        ShmemObj *data() const
        {
            if (dataOffset == NPtr)
                return nullptr;
            else
                return reinterpret_cast<ShmemObj *>(reinterpret_cast<uintptr_t>(this) + dataOffset);
        }

        void setData(ShmemObj *obj)
        {
            if (obj == nullptr)
                dataOffset = NPtr; // Set offset to an impossible value to indicate nullptr
            else
                dataOffset = reinterpret_cast<uintptr_t>(obj) - reinterpret_cast<uintptr_t>(this);
        }
    };

    struct ShmemDict : public ShmemObj
    {
        ptrdiff_t rootOffset;
        ptrdiff_t NILOffset;

        ShmemDictNode *root() const
        {
            return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + rootOffset);
        }

        void setRoot(ShmemDictNode *node)
        {
            rootOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
        }

        ShmemDictNode *NIL() const
        {
            return reinterpret_cast<ShmemDictNode *>(reinterpret_cast<uintptr_t>(this) + NILOffset);
        }

        void setNIL(ShmemDictNode *node)
        {
            NILOffset = reinterpret_cast<uintptr_t>(node) - reinterpret_cast<uintptr_t>(this);
        }

        ShmemObj *get(KeyType key)
        {
            if (this->type != Dict)
            {
                throw std::runtime_error("Cannot get on non-dict ShmemObj");
            }
        }

        void leftRotate(ShmemDictNode *nodeX)
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

        void rightRotate(ShmemDictNode *nodeX)
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
            }
        }

        void fixInsert(ShmemDictNode *nodeK)
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

        void inorderHelper(ShmemDictNode *node)
        {
            if (node != this->NIL())
            {
                inorderHelper(node->left());
                std::cout << node->key << ":" << node->data()->type << " ";
                inorderHelper(node->right());
            }
        }
        static void deconstructHelper(ShmemDictNode *node, ShmemHeap *heapPtr)
        {
            if (node->key != hashedNILKey)
            {
                deconstructHelper(node->left(), heapPtr);  // Destroy the left subtree
                deconstructHelper(node->right(), heapPtr); // Destroy the right subtree
                heapPtr->shfree(reinterpret_cast<Byte *>(node->data()));
                heapPtr->shfree(reinterpret_cast<Byte *>(node));
            }
        }

        ShmemDictNode *searchHelper(ShmemDictNode *node, int key)
        {
            if (node == this->NIL() || node->key == key)
                return node;
            if (key, node->key)
                return searchHelper(node->left(), key);
            else
                return searchHelper(node->right(), key);
        }

    public:
        static size_t construct(ShmemHeap *heapPtr)
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
        }

        static void deconstruct(size_t offset, ShmemHeap *heapPtr)
        {
            // Do post-order traversal and remove all nodes
            // TODO: lock the object by setting busy bit
            ShmemDict *ptr = reinterpret_cast<ShmemDict *>(resolveOffset(offset, heapPtr));
            deconstructHelper(ptr->root(), heapPtr);
            heapPtr->shfree(reinterpret_cast<Byte *>(ptr->NIL()));
            heapPtr->shfree(reinterpret_cast<Byte *>(ptr));
        }

        void insert(KeyType key, ShmemObj *data, ShmemHeap *heapPtr)
        { // TODO: lock the object by setting busy bit
            int hashKey = hashIntOrString(key);

            ShmemDictNode *parent = nullptr;
            ShmemDictNode *current = root();

            while (current != NIL())
            {
                parent = current;
                if (hashKey < current->key)
                    current = current->left();
                else if (hashKey > current->key)
                    current = current->right();
                else
                { // repeated ke, replace the old data
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
            else if (hashKey < parent->key)
                parent->setLeft(newNode);
            else if (hashKey > parent->key)
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

        ShmemObj *get(KeyType key)
        {
            return search(key)->data();
        }

        // Inorder traversal
        void inorder()
        {
            inorderHelper(root());
        }

        // Search for a node
        ShmemDictNode *search(KeyType key)
        {
            ShmemDictNode *result = searchHelper(root(), hashIntOrString(key));
            if (result == NIL())
            {
                return nullptr;
            }
        }
    };
    struct ShmemList : public ShmemObj
    {
        const ShmemObj *at(int index) const
        {
            if (this->type != List)
            {
                throw std::runtime_error("Cannot at on non-list ShmemObj");
            }
            if (index < 0)
            { // negative index is relative to the end
                index += size;
            }
            if (index >= size || index < 0)
            {
                throw std::runtime_error("Index out of bounds");
            }
            return reinterpret_cast<const ShmemObj *>(reinterpret_cast<const Byte *>(this) + sizeof(ShmemObj) + index * sizeof(ptrdiff_t));
        }

        ShmemObj *at(int index)
        {
            return const_cast<ShmemObj *>(const_cast<const ShmemList *>(this)->at(index));
        }

        void set(int index, ShmemObj *value)
        {
        }
    };

public:
    /**
     * @brief A public proxy to access a ShmemObj. It force immediate conversion to the target type
     * and prevent saving the ShmemObj ptr. As ShmemObj is a temporary ptr into the memory
     */
    struct ShmemObjProxy
    {
        ShmemObj *ptr;
        ShmemObjProxy(ShmemObj *ptr) : ptr(ptr) {}
        ShmemObjProxy(const ShmemObjProxy &) = delete;
        ShmemObjProxy(ShmemObjProxy &&) = delete;
        ShmemObjProxy &operator=(const ShmemObjProxy &) = delete;

        template <typename T>
        operator T() const
        {
            if (TypeEncoding<T>::value != this->ptr->type)
            {
                throw std::runtime_error("Type mismatch");
            }
            if (isPrimitive(this->ptr->type))
            {
                return static_cast<ShmemPrimitive<T> *>(this->ptr)->operator T();
            }
            else
            {
                throw std::runtime_error("Current cannot cast non-primitive ShmemObj");
            };
        }

        template <typename T>
        T operator[](int index) const
        {
            return static_cast<ShmemPrimitive<T> *>(this->ptr)->operator[](index);
        }

        ShmemObjProxy at(int index) const
        {
            int type = this->ptr->type;
            if (type != List)
            {
                throw std::runtime_error("Cannot call at on non-list ShmemObj");
            }
            return ShmemObjProxy(static_cast<ShmemList *>(this->ptr)->at(index));
        }

        ShmemObjProxy get(KeyType key) const
        {
            int type = this->ptr->type;
            if (type != Dict)
            {
                throw std::runtime_error("Cannot call get on non-dict ShmemObj");
            }
            return ShmemObjProxy(static_cast<ShmemDict *>(this->ptr)->get(key));
        }
    };
    ShmemHeap *heapPtr;
    std::vector<KeyType> path;

    ShmemAccessor(ShmemHeap *heapPtr) : heapPtr(heapPtr), path({}) {}
    ShmemAccessor(ShmemHeap *heapPtr, std::vector<KeyType> path) : heapPtr(heapPtr), path(path) {}

    ShmemObjProxy resolve()
    {
        ShmemObj *current = reinterpret_cast<ShmemObj *>(this->heapPtr->heapHead());
        for (int i = 0; i < path.size(); i++)
        {
            KeyType pathElement = path[i];
            if (isPrimitive(current->type))
            { // base case
                if (i != path.size() - 1 || std::holds_alternative<std::string>(pathElement))
                {
                    throw std::runtime_error("Cannot index <remaining index> on primitive object");
                }
                return ShmemObjProxy(current);
            }
            else if (current->type == Dict)
            {
                ShmemDict *currentDict = static_cast<ShmemDict *>(current);
                current = currentDict->get(pathElement);
            }
            else if (current->type == List)
            {
                ShmemList *currentList = static_cast<ShmemList *>(current);
                if (std::holds_alternative<std::string>(pathElement))
                {
                    throw std::runtime_error("Cannot index <string> on List");
                }
                current = currentList->at(std::get<int>(pathElement));
            }
        }
        // TODO: warning providing a dict / list path
        return ShmemObjProxy(current);
    }

    template <typename... KeyTypes>
    ShmemAccessor operator()(KeyTypes... accessPath) const
    {
        static_assert((std::is_same<KeyType, KeyTypes>::value && ...), "All arguments must be of type KeyType");
        // TODO: change to reference a head in static space
        this->path.insert(this->path.end(), {accessPath...});
    }

    ShmemAccessor operator[](KeyType index) const
    {
        std::vector<KeyType> newPath(this->path);
        newPath.push_back(index);
        return ShmemAccessor(this->heapPtr, newPath);
    }

    template <typename T>
    operator T() const
    {
        if (!isPrimitive<T>())
        {
            throw std::runtime_error("Currently can only convert to primitive or vector of primitive");
        }

        ShmemObj *current = reinterpret_cast<ShmemObj *>(this->heapPtr->heapHead());
        for (int i = 0; i < path.size(); i++)
        {
            KeyType pathElement = path[i];
            if (isPrimitive(current->type))
            { // base case
                if (current->type != TypeEncoding<T>::value)
                {
                    throw std::runtime_error("Type mismatch");
                }
                if (i == path.size() - 1)
                {
                    if (std::holds_alternative<int>(pathElement))
                    {
                        if (isVector<T>::value)
                        {
                            throw std::runtime_error("Ask for a vector but result is a primitive");
                        }
                        return static_cast<ShmemPrimitive<T> *>(current)->operator[](std::get<int>(pathElement));
                    }
                    else
                    {
                        throw std::runtime_error("Cannot index <string> on primitive array");
                    }
                }
                else
                {
                    throw std::runtime_error("Cannot index <remaining index> on primitive object");
                }
            }
            else if (current->type == Dict)
            {
                ShmemDict *currentDict = static_cast<ShmemDict *>(current);
                current = currentDict->get(pathElement);
            }
            else if (current->type == List)
            {
                ShmemList *currentList = static_cast<ShmemList *>(current);
                if (std::holds_alternative<std::string>(pathElement))
                {
                    throw std::runtime_error("Cannot index <string> on List");
                }
                current = currentList->at(std::get<int>(pathElement));
            }
        }

        if (isPrimitive(current->type))
        {
            if (isVector<T>::value)
            {
                using vecType = unwrapVectorType<T>::type;
                return static_cast<ShmemPrimitive<vecType> *>(current)->operator T();
            }
            else
                return static_cast<ShmemPrimitive<T> *>(current)->operator T();
        }
        else
        {
            throw std::runtime_error("Cannot convert to this type (C++ doesn't support dynamic type)");
        }
    }
};