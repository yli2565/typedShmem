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

class IndexError : public std::runtime_error
{
public:
    explicit IndexError(const std::string &message);
};

int hashIntOrString(KeyType key);

const static KeyType NILKey = "NILKey:js82nfd-";
const static int hashedNILKey = hashIntOrString(NILKey);

class ShmemAccessor
{
private:
    struct ShmemObj
    {
        int type;
        int size;

        ShmemObj() = delete;

        template <typename T>
        static size_t construct(const T &value, ShmemHeap *heapPtr)
        {
            if (isPrimitive<T>())
            { // Base case
                if (isVector<T>::value)
                { // Vector as an array of primitive
                    using vecDataType = typename unwrapVectorType<T>::type;
                    return ShmemPrimitive<vecDataType>::construct(value, heapPtr);
                }
                else if (isString<T>())
                { // String as an array of char
                    return ShmemPrimitive<char>::construct(value, heapPtr);
                }
                else
                { // Single value
                    return ShmemPrimitive<T>::construct(value, heapPtr);
                }
            }
            else
            {
                if (isVector<T>::value)
                {
                    using vecDataType = typename unwrapVectorType<T>::type;
                    size_t listOffset = ShmemList::construct(value.size(), heapPtr);
                    ShmemList *list = reinterpret_cast<ShmemList *>(ShmemObj::resolveOffset(listOffset, heapPtr));
                    for (int i = 0; i < value.size(); i++)
                    {
                        ShmemObj *child = ShmemObj::construct<vecDataType>(value[i], heapPtr);
                        list->add(child);
                    }
                    return listOffset;
                }
                else if (isMap<T>::value)
                {
                    using mapDataType = typename unwrapMapType<T>::type;
                    using mapKeyType = typename unwrapMapType<T>::keyType;
                    size_t dictOffset = ShmemDict::construct(heapPtr);
                    ShmemDict *dict = reinterpret_cast<ShmemDict *>(ShmemObj::resolveOffset(dictOffset, heapPtr));
                    for (auto &[key, val] : value)
                    {
                        ShmemObj *child = ShmemObj::construct<mapDataType>(val, heapPtr);
                        dict->insert(key, child, heapPtr);
                    }
                    return dictOffset;
                }
                else
                {
                    throw std::runtime_error("Cannot construct object of type " + std::string(typeid(T).name()));
                }
            }
        }
        static void deconstruct(size_t offset, ShmemHeap *heapPtr);

        size_t capacity() const;
        bool isBusy() const;
        void setBusy(bool b = true);
        int wait(int timeout = -1) const;

        ShmemHeap::BlockHeader *getHeader() const;
        static ShmemObj *resolveOffset(size_t offset, ShmemHeap *heapPtr);

        static std::string toString(ShmemObj *obj, int indent = 0);
    };

    // For those functions that does not rely on Template <T>
    struct ShmemPrimitive_ : public ShmemObj
    {
        static inline void deconstruct(size_t offset, ShmemHeap *heapPtr)
        {
            heapPtr->shfree(reinterpret_cast<Byte *>(resolveOffset(offset, heapPtr)));
        }

        inline const Byte *getBytePtr() const
        {
            return reinterpret_cast<const Byte *>(reinterpret_cast<const Byte *>(this) + sizeof(ShmemObj));
        }

        inline Byte *getBytePtr()
        {
            return reinterpret_cast<Byte *>(reinterpret_cast<Byte *>(this) + sizeof(ShmemObj));
        }

        static std::string toString(ShmemPrimitive_ *obj, int indent = 0, int maxElements = 4)
        {
#define PRINT_OBJ(type)                                                   \
    for (int i = 0; i < std::max(obj->size, maxElements); i++)            \
    {                                                                     \
        result += " " + std::to_string(reinterpret_cast<type *>(ptr)[i]); \
    }                                                                     \
    break;
            std::string result = "[P:" + typeNames.at(obj->type) + ":" + std::to_string(obj->size) + "]";

            Byte *ptr = obj->getBytePtr();
            switch (obj->type)
            {
            case Bool:
                PRINT_OBJ(bool);
            case Char:
                PRINT_OBJ(char);
            case UChar:
                PRINT_OBJ(unsigned char);
            case Short:
                PRINT_OBJ(short);
            case UShort:
                PRINT_OBJ(unsigned short);
            case Int:
                PRINT_OBJ(int);
            case UInt:
                PRINT_OBJ(unsigned int);
            case Long:
                PRINT_OBJ(long);
            case ULong:
                PRINT_OBJ(unsigned long);
            case LongLong:
                PRINT_OBJ(long long);
            case ULongLong:
                PRINT_OBJ(unsigned long long);
            case Float:
                PRINT_OBJ(float);
            case Double:
                PRINT_OBJ(double);
            default:
                throw std::runtime_error("Unknown type");
                break;
            }
#undef PRINT_OBJ
            return result;
        }
    };

    template <typename T>
    struct ShmemPrimitive : public ShmemPrimitive_
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
            return reinterpret_cast<const T *>(getBytePtr());
        }
        T *getPtr()
        {
            checkType();
            return reinterpret_cast<T *>(getBytePtr());
        }

        std::string toString(int maxElements = 4) const
        {
            std::string result = "[P:" + typeNames.at(this->type) + ":" + std::to_string(this->size) + "]";
            const T *ptr = this->getPtr();
            for (int i = 0; i < std::max(this->size, maxElements); i++)
            {
                result += " " + std::to_string(ptr[i]);
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

        static size_t construct(KeyType key, ShmemHeap *heapPtr);
        // static void deconstruct(ShmemDictNode *node, ShmemHeap *heapPtr);
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

    struct ShmemDict : public ShmemObj
    {
        ptrdiff_t rootOffset;
        ptrdiff_t NILOffset;

        ShmemDictNode *root() const;
        void setRoot(ShmemDictNode *node);
        ShmemDictNode *NIL() const;
        void setNIL(ShmemDictNode *node);

        void leftRotate(ShmemDictNode *nodeX);
        void rightRotate(ShmemDictNode *nodeX);
        void fixInsert(ShmemDictNode *nodeK);
        std::string inorderHelper(ShmemDictNode *node, int indent = 0);

        static void deconstructHelper(ShmemDictNode *node, ShmemHeap *heapPtr);
        ShmemDictNode *searchHelper(ShmemDictNode *node, int key);

    public:
        static size_t construct(ShmemHeap *heapPtr);
        static void deconstruct(size_t offset, ShmemHeap *heapPtr);

        void insert(KeyType key, ShmemObj *data, ShmemHeap *heapPtr);

        ShmemObj *get(KeyType key);

        std::string inorder(int indent = 0);

        ShmemDictNode *search(KeyType key);

        static std::string toString(ShmemDict *dict, int indent = 0);
    };

    struct ShmemList : public ShmemObj
    {
        size_t listCapacity() const;
        static size_t construct(size_t capacity, ShmemHeap *heapPtr);
        static void deconstruct(size_t offset, ShmemHeap *heapPtr);

        ptrdiff_t *relativeOffsetPtr();
        int resolveIndex(int index);

        ShmemObj *at(int index);
        void add(ShmemObj *newObj);
        ShmemObj *replace(int index, ShmemObj *newObj);
        ShmemObj *pop(int index);

        static std::string toString(ShmemList *list, int indent = 0, int maxElements = 4);
    };

    // std::string toString(ShmemObj *obj, int indent = 0) const;
protected:
    /**
     * @brief Get offset of the entrance point to the data structure on the heap
     *
     * @return size_t reference to the fourth 8 bytes of the heap
     */
    size_t &entranceOffset();

    /**
     * @brief Get offset of the entrance point to the data structure on the heap
     *
     * @return size_t value of the fourth 8 bytes of the heap
     */
    size_t entranceOffset() const;

    // Utility functions
    void resolvePath(ShmemObj *&prevObj, ShmemObj *&obj, int &resolvedDepth) const;

public:
    ShmemHeap *heapPtr;
    std::vector<KeyType> path;

    ShmemAccessor(ShmemHeap *heapPtr);
    ShmemAccessor(ShmemHeap *heapPtr, std::vector<KeyType> path);

    /**
     * @brief Get the entrance ptr of the data structure. Check connection before using
     * @return this->shmPtr + staticCapacity + entranceOffset
     */
    ShmemObj *entrance() const;

    void setEntrance(ShmemObj *obj);

    template <typename... KeyTypes>
    ShmemAccessor operator[](KeyTypes... accessPath) const
    {
        static_assert((std::is_same<KeyType, KeyTypes>::value && ...), "All arguments must be of type KeyType");
        std::vector<KeyType> newPath(this->path);
        newPath.insert(newPath.end(), {accessPath...});
        return ShmemAccessor(this->heapPtr, newPath);
    }

    // ShmemAccessor operator[](KeyType index) const;

    template <typename T>
    operator T() const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        int primitiveIndex;
        bool usePrimitiveIndex = false;

        if (obj->type != TypeEncoding<T>::value)
            throw std::runtime_error("Type mismatch");

        // Deal with unresolved path
        if (resolvedDepth != path.size())
        {
            // There are some path not resolved
            // The only case allowed is that the last element is a primitive, and the last path is an index on the primitive array
            if (resolvedDepth == path.size() - 1)
            {
                if (std::holds_alternative<int>(path[resolvedDepth]))
                {
                    primitiveIndex = std::get<int>(path[resolvedDepth]);
                    usePrimitiveIndex = true;
                    return;
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

        // convert to target type
        if (isPrimitive<T>())
        {
            // Based on the required return type, phrase the primitive object
            if (isVector<T>::value)
            {
                using vecType = typename unwrapVectorType<T>::type;
                return static_cast<ShmemPrimitive<vecType> *>(obj)->operator T();
            }
            else
            {
                if (usePrimitiveIndex)
                    return static_cast<ShmemPrimitive<T> *>(obj)->operator[](primitiveIndex);
                else
                    return static_cast<ShmemPrimitive<T> *>(obj)->operator T();
            }
        }
        else if (isString<T>())
        {
            return static_cast<ShmemPrimitive<char> *>(obj)->operator T();
        }
        else
        {
            throw std::runtime_error("Cannot convert to this type (C++ doesn't support dynamic type)");
        }
    }

    template <typename T>
    ShmemAccessor &operator=(T val)
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        bool partiallyResolved = resolvedDepth != path.size();

        int primitiveIndex;
        bool usePrimitiveIndex = false;
        bool insertNewKey = false;

        // Deal with unresolved path
        if (partiallyResolved)
        {
            // Only one path element is not resolved
            if (resolvedDepth == path.size() - 1)
            {
                // Two cases:
                // 1. The object is a primitive, and the last path is an index on the primitive array
                // 2. The object is not a dict, the last path element is a new key
                if (isPrimitive(obj->type))
                {
                    if (std::holds_alternative<int>(path[resolvedDepth]))
                    {
                        primitiveIndex = std::get<int>(path[resolvedDepth]);
                        usePrimitiveIndex = true;
                        return;
                    }
                    else
                    {
                        throw std::runtime_error("Cannot index <string> on primitive array");
                    }
                }
                else if (obj->type == Dict)
                {
                    insertNewKey = true;
                }
                else
                {
                    throw std::runtime_error("The index XX is invalid on object of type List");
                }
            }
            else
            {
                throw std::runtime_error("Cannot index <remaining index> on primitive object");
            }
        }

        // Assign the value
        if (partiallyResolved)
        {
            if (usePrimitiveIndex)
                static_cast<ShmemPrimitive<T> *>(obj)->set(primitiveIndex, val);
            else if (insertNewKey)
            {
                ShmemObj *newObj = ShmemObj::construct<T>(val, this->heapPtr);
                static_cast<ShmemDict *>(obj)->insert(path[resolvedDepth], newObj, this->heapPtr);
            }
            else
                throw std::runtime_error("Code should not reach here");
        }
        else
        {
            if (obj == nullptr)
            {
                if (path.size() == 0 && prev == nullptr)
                { // It means the whole heap is not initialized
                    ShmemObj *newObj = ShmemObj::construct<T>(val, this->heapPtr);
                    this->setEntrance(newObj);
                }
                else if (prev->type == List)
                {
                    ShmemObj *newObj = ShmemObj::construct<T>(val, this->heapPtr);
                    ShmemObj *originalObj = static_cast<ShmemList *>(prev)->replace(std::get<int>(this->path.back()), newObj);
                    if (originalObj != nullptr)
                    {
                        throw std::runtime_error("This should never be the case");
                    }
                }
                else
                {
                    throw std::runtime_error("This case should not happen");
                }
            }
            else
            {
                // Replace the old element
                ShmemObj::deconstruct(reinterpret_cast<Byte *>(obj) - this->heapPtr->heapHead(), this->heapPtr);
                ShmemObj *newObj = ShmemObj::construct<T>(val, this->heapPtr);
                int prevType = prev->type;
                if (isPrimitive(prevType))
                {
                }
                else if (prevType == List)
                {
                    static_cast<ShmemList *>(prev)->set(0, newObj);
                }
                else if (prevType == Dict)
                {
                    static_cast<ShmemDict *>(prev)->insert(path[resolvedDepth], newObj, this->heapPtr);
                }
            }
        }

        return *this;
    }
};

#endif