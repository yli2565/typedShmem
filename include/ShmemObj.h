#include "ShmemHeap.h"
#include "typeEncodings.h"
#include <variant>
#include <vector>
#include <string>

using KeyType = std::variant<int, std::string>;

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

const static KeyType NILKey = "NILKey:js82nfd-"; // Something random
const static int hashedNILKey = hashIntOrString(NILKey);

class ShmemAccessor
{
private:
    struct ShmemObj
    {
        int type;
        int size;

        ShmemObj() = delete;
        size_t capacity() const
        {
            return this->getHeader()->size();
        }
        bool isBusy() const
        {
            return this->getHeader()->B();
        }
        void setBusy(bool b = true)
        {
            this->getHeader()->setB(b);
        }
        int wait(int timeout = -1) const
        {
            return this->getHeader()->wait(timeout);
        }

        inline ShmemHeap::BlockHeader *getHeader() const
        {
            return reinterpret_cast<ShmemHeap::BlockHeader *>(reinterpret_cast<uintptr_t>(this) - sizeof(ShmemHeap::BlockHeader));
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
            size_t offset = construct(size, heapPtr);
            ShmemPrimitive<T> *ptr = static_cast<ShmemPrimitive<T> *>(resolveOffset(offset, heapPtr));
            memcpy(ptr->getPtr(), vec.data(), size * sizeof(T));
            return offset;
        }

        static size_t construct(std::string str, ShmemHeap *heapPtr)
        {
            if (TypeEncoding<T>::value != Char)
            {
                throw std::runtime_error("Cannot use string to construct a non-char primitive object");
            }
            size_t size = str.size() + 1; // +1 for the \0
            size_t offset = construct(size, heapPtr);
            ShmemPrimitive<T> *ptr = static_cast<ShmemPrimitive<T> *>(resolveOffset(offset, heapPtr));
            memcpy(ptr->getPtr(), str.data(), size * sizeof(T));
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
            const T *ptr = this->getPtr();
            if (this->size != 1)
            {
                printf("Size is not 1, but you are only accessing the first element\n");
            }
            return *ptr;
        }

        operator std::vector<T>() const
        {
            const T *ptr = this->getPtr();
            std::vector<T> vec(ptr, ptr + this->size);
            return vec;
        }

        operator std::string() const
        {
            if (this->type != Char)
            {
                throw std::runtime_error("Cannot convert non-char object to string");
            }
            const T *ptr = this->getPtr();
            std::string str(ptr, ptr + this->size);
            return str;
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
            return reinterpret_cast<const T *>(reinterpret_cast<const Byte *>(this) + sizeof(ShmemObj));
        }
        T *getPtr()
        {
            checkType();
            return reinterpret_cast<T *>(reinterpret_cast<Byte *>(this) + sizeof(ShmemObj));
        }

        std::string toString() const
        {
            std::string result = "[P:" + typeNames.at(this->type) + ":" + std::to_string(this->size) + "]";

            for (int i = 0; i < std::max(this->size, 3); i++)
            {
                result += " " + std::to_string(this->getPtr()[i]);
            }
            if (this->size > 3)
            {
                result += " ...";
            }
            return result;
        }
    };

    struct ShmemDictNode : public ShmemObj
    {
        ptrdiff_t leftOffset, rightOffset, parentOffset;
        ptrdiff_t keyOffset;
        ptrdiff_t dataOffset;
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
            if (std::holds_alternative<std::string>(key))
                ptr->keyOffset = ShmemPrimitive<char>::construct(std::get<std::string>(key), heapPtr) - offset;
            else
                ptr->keyOffset = ShmemPrimitive<int>::construct(std::get<int>(key), heapPtr) - offset;
            return offset;
        }

        static void deconstruct(ShmemDictNode *node, ShmemHeap *heapPtr)
        {
            heapPtr->shfree(const_cast<Byte *>(reinterpret_cast<const Byte *>(node->key())));
            heapPtr->shfree(reinterpret_cast<Byte *>(node->data()));
            heapPtr->shfree(reinterpret_cast<Byte *>(node));
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

        void colorRed()
        {
            this->color = 0;
        }
        void colorBlack()
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

        const ShmemObj *key() const
        {
            return reinterpret_cast<ShmemObj *>(reinterpret_cast<uintptr_t>(this) + keyOffset);
        }

        inline KeyType keyVal() const
        {
            int keyType = key()->type;
            if (keyType == String)
                return reinterpret_cast<const ShmemPrimitive<char> *>(key())->operator std::string();
            else if (keyType == Int)
                return reinterpret_cast<const ShmemPrimitive<int> *>(key())->operator int();
            else
                throw std::runtime_error("Unknown key type");
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

        int hashedKey() const
        {
            int keyType = key()->type;
            if (keyType == String)
                return std::hash<std::string>{}(reinterpret_cast<const ShmemPrimitive<char> *>(key())->operator std::string());
            else if (keyType == Int)
                return std::hash<int>{}(reinterpret_cast<const ShmemPrimitive<int> *>(key())->operator int());
            else
                throw std::runtime_error("Unknown key type");
        }

        std::string keyToString() const
        {
            // (color == 0 ? "R" : "B")
            int keyType = key()->type;
            if (keyType == String)
                return reinterpret_cast<const ShmemPrimitive<char> *>(key())->operator std::string();
            else if (keyType == Int)
                return std::to_string(reinterpret_cast<const ShmemPrimitive<int> *>(key())->operator int());
            else
                throw std::runtime_error("Unknown key type");
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

        std::string inorderHelper(ShmemDictNode *node, int indent = 0)
        {
            if (node != this->NIL())
            {
                std::string left = inorderHelper(node->left());
                if (left != "")
                    left += "\n";
                std::string current = std::string(indent, ' ') + node->keyToString() + ": " + std::to_string(node->data()->type) + "";
                std::string right = inorderHelper(node->right());
                if (right != "")
                    right = "\n" + right;
                return left + current + right;
            }
            return "";
        }

        // void inorderHelper(ShmemDictNode *node, int indent = 0)
        // {
        //     if (node != this->NIL())
        //     {
        //         inorderHelper(node->left());
        //         std::cout << node->keyToString() << ":" << toString(data()) << " ";
        //         inorderHelper(node->right());
        //     }
        // }

        static void deconstructHelper(ShmemDictNode *node, ShmemHeap *heapPtr)
        {
            if (node->hashedKey() != hashedNILKey)
            {
                deconstructHelper(node->left(), heapPtr);  // Destroy the left subtree
                deconstructHelper(node->right(), heapPtr); // Destroy the right subtree
                ShmemDictNode::deconstruct(node, heapPtr);
            }
        }

        ShmemDictNode *searchHelper(ShmemDictNode *node, int key)
        {
            if (node == this->NIL() || node->hashedKey() == key)
                return node;
            if (key, node->hashedKey())
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

            return dictOffset;
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
                if (hashKey < current->hashedKey())
                    current = current->left();
                else if (hashKey > current->hashedKey())
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

        ShmemObj *get(KeyType key)
        {
            return search(key)->data();
        }

        // Inorder traversal
        std::string inorder(int indent = 0)
        {
            return "{\n" + inorderHelper(root(), indent + 1) + "\n" + std::string(indent, ' ') + "}";
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
        static size_t construct(size_t size, ShmemHeap *heapPtr)
        {
            size_t offset = heapPtr->shmalloc(sizeof(ShmemList) + size * sizeof(ptrdiff_t));
            ShmemList *ptr = static_cast<ShmemList *>(resolveOffset(offset, heapPtr));
            ptr->type = List;
            ptr->size = size;
            return offset;
        }
        ptrdiff_t *relativeOffsetPtr()
        {

            return reinterpret_cast<ptrdiff_t *>(reinterpret_cast<Byte *>(this) + sizeof(ShmemObj));
        }

        ShmemObj *at(int index)
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
            return reinterpret_cast<ShmemObj *>(reinterpret_cast<Byte *>(this) + relativeOffsetPtr()[index]);
        }
    };

    std::string toString(ShmemObj *obj, int indent = 0) const
    {
        std::string result;
        std::string indentStr = std::string(indent, ' ');

        ShmemList *list = static_cast<ShmemList *>(obj);
        ShmemDictNode *node = static_cast<ShmemDictNode *>(obj);
        ShmemDict *dict = static_cast<ShmemDict *>(obj);

        switch (obj->type)
        {
        case Bool:
            result = static_cast<ShmemPrimitive<bool> *>(obj)->toString();
            break;
        case Char:
            result = static_cast<ShmemPrimitive<char> *>(obj)->toString();
            break;
        case UChar:
            result = static_cast<ShmemPrimitive<unsigned char> *>(obj)->toString();
            break;
        case Short:
            result = static_cast<ShmemPrimitive<short> *>(obj)->toString();
            break;
        case UShort:
            result = static_cast<ShmemPrimitive<unsigned short> *>(obj)->toString();
            break;
        case Int:
            result = static_cast<ShmemPrimitive<int> *>(obj)->toString();
            break;
        case UInt:
            result = static_cast<ShmemPrimitive<unsigned int> *>(obj)->toString();
            break;
        case Long:
            result = static_cast<ShmemPrimitive<long> *>(obj)->toString();
            break;
        case ULong:
            result = static_cast<ShmemPrimitive<unsigned long> *>(obj)->toString();
            break;
        case LongLong:
            result = static_cast<ShmemPrimitive<long long> *>(obj)->toString();
            break;
        case ULongLong:
            result = static_cast<ShmemPrimitive<unsigned long long> *>(obj)->toString();
            break;
        case Float:
            result = static_cast<ShmemPrimitive<float> *>(obj)->toString();
            break;
        case Double:
            result = static_cast<ShmemPrimitive<double> *>(obj)->toString();
            break;
        case String:
            result = static_cast<ShmemPrimitive<char> *>(obj)->toString();
            break;
        case List:
            result += "[\n";
            for (int i = 0; i < list->size; i++)
            {
                result += toString(list->at(i), indent + 2) + "\n";
            }
            result += indentStr + "]";
            break;
        case DictNode:
            result = node->keyToString() + ": " + toString(node->data());
            break;
        case Dict:
            result = dict->inorder(indent);
            break;
        default:
            throw std::runtime_error("Cannot convert ShmemObj to string");
            break;
        }

        return result;
    }

public:
    /**
     * @brief A public proxy to access a ShmemObj. It force immediate conversion to the target type
     * and prevent saving the ShmemObj ptr. As ShmemObj is a temporary ptr into the memory
     */
    ShmemHeap *heapPtr;
    std::vector<KeyType> path;

    ShmemAccessor(ShmemHeap *heapPtr) : heapPtr(heapPtr), path({}) {}
    ShmemAccessor(ShmemHeap *heapPtr, std::vector<KeyType> path) : heapPtr(heapPtr), path(path) {}

    template <typename... KeyTypes>
    ShmemAccessor operator()(KeyTypes... accessPath) const
    {
        static_assert((std::is_same<KeyType, KeyTypes>::value && ...), "All arguments must be of type KeyType");
        // TODO: change to reference a head in static space
        std::vector<KeyType> newPath(this->path);
        newPath.insert(newPath.end(), {accessPath...});
        return ShmemAccessor(this->heapPtr, newPath);
    }

    ShmemAccessor operator[](KeyType index) const
    {
        std::vector<KeyType> newPath(this->path);
        newPath.push_back(index);
        return ShmemAccessor(this->heapPtr, newPath);
    }

    template <typename T>
    size_t construct(const T &value)
    {
        if (isPrimitive<T>())
        { // This case is either a single primitive or a vector of primitive
            return ShmemPrimitive<T>::construct(value, this->heapPtr);
        }
        else if (isString<T>())
        {
            return ShmemPrimitive<char>::construct(value, this->heapPtr);
        }
        else if (isVector<T>::value)
        { // A vector of non-primitive
            ShmemList *newList = ShmemList::construct(value.size(), this->heapPtr);
            for (int i = 0; i < static_cast<int>(value.size()); i++)
            {
                ShmemObj *newObj = this->construct<unwrapVectorType<T>::type>(value[i]);
                newList->relativeOffsetPtr()[i] = reinterpret_cast<Byte *>(newObj) - reinterpret_cast<Byte *>(newList);
            }
            return reinterpret_cast<Byte *>(newList) - this->heapPtr->heapHead();
        }
        else if (isMap<T>::value)
        { // Any map
            if (std::is_same<T, int>::value || std::is_same<T, std::string>::value)
            { // Only accept int and string as keys
                ShmemDict *newDict = ShmemDict::construct(this->heapPtr);
                for (auto &[key, value] : value)
                {
                    ShmemObj *newObj = this->construct<unwrapMapType<T>::type>(value);
                    newDict->insert(key, newObj, this->heapPtr);
                }
                return reinterpret_cast<Byte *>(newDict) - this->heapPtr->heapHead();
            }
            else
            {
                throw std::runtime_error("Only accept int and string as keys");
            }
        }
        else
        {
            throw std::runtime_error("This type cannot be stored into Shmem, try to parse it to dict or vector first");
        }
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
                using vecType = typename unwrapVectorType<T>::type;
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