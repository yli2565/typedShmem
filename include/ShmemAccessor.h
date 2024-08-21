#ifndef SHMEM_ACCESSOR_H
#define SHMEM_ACCESSOR_H

#include <iostream>
#include "ShmemObj.h"

class ShmemAccessor
{
private:
    static std::string pathToString(const KeyType *path, int size);
    std::string pathToString() const;

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

    /**
     * @brief Get the entrance ptr of the data structure. Check connection before using
     * @return this->shmPtr + staticCapacity + entranceOffset
     */
    ShmemObj *entrance() const;

    /**
     * @brief Set the Entrance object
     *
     * @param obj
     * @warning the obj must belongs to this->heapPtr
     */
    void setEntrance(ShmemObj *obj);

    /**
     * @brief Set the entrance object by offset (Recommended, safer)
     *
     * @param offset the offset of the entrance object from the heap head
     */
    void setEntrance(size_t offset);

    // Utility functions

    void resolvePath(ShmemObj *&prevObj, ShmemObj *&obj, int &resolvedDepth) const;

public:
    ShmemHeap *heapPtr;
    std::vector<KeyType> path;

    ShmemAccessor(ShmemHeap *heapPtr);
    ShmemAccessor(ShmemHeap *heapPtr, std::vector<KeyType> path);

    ~ShmemAccessor() = default;

    template <typename... KeyTypes>
    ShmemAccessor operator[](KeyTypes... accessPath) const
    {
        static_assert(((std::is_same<int, KeyTypes>::value || std::is_same<std::string, KeyTypes>::value || std::is_same<KeyType, KeyTypes>::value) && ...), "All arguments must be of type KeyType");
        std::vector<KeyType> newPath(this->path);
        newPath.insert(newPath.end(), {accessPath...});
        return ShmemAccessor(this->heapPtr, newPath);
    }

    // Type (Special interface)
    int typeId() const;
    std::string typeStr() const;

    // __len__
    size_t len() const;

    // __getitem__
    template <typename T>
    T get() const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        int primitiveIndex;
        bool usePrimitiveIndex = false;

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
                }
                else
                {
                    throw std::runtime_error("Cannot index string: " + std::get<std::string>(path[resolvedDepth]) + " on primitive array");
                }
            }
            else
            {
                throw std::runtime_error("Cannot index " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on primitive object");
            }
        }

        // convert to target type
        if constexpr (isPrimitive<T>())
        {
            // Based on the required return type, phrase the primitive object
            if constexpr (isVector<T>::value)
            {
                using vecType = typename unwrapVectorType<T>::type;
                return reinterpret_cast<ShmemPrimitive<vecType> *>(obj)->operator T();
            }
            else
            {
                if (usePrimitiveIndex)
                    return reinterpret_cast<ShmemPrimitive_ *>(obj)->operator[]<T>(primitiveIndex);
                else
                    return reinterpret_cast<ShmemPrimitive_ *>(obj)->operator T();
            }
        }
        else if constexpr (isString<T>())
        {
            return reinterpret_cast<ShmemPrimitive<char> *>(obj)->operator T();
        }
        else if constexpr (isVector<T>::value)
        {
            return reinterpret_cast<ShmemList *>(obj)->operator T();
        }
        else if constexpr (isMap<T>::value)
        {
            return reinterpret_cast<ShmemDict *>(obj)->operator T();
        }
        else
        {
            throw std::runtime_error("Cannot convert to this type (C++ doesn't support dynamic type)");
        }
    }

    // __setitem__
    template <typename T>
    void set(const T &val)
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
            // Only one path element is unresolved
            if (resolvedDepth == path.size() - 1)
            {
                // Two cases:
                // 1. The object is a primitive, and the last path is an index on the primitive array
                //    Replace case
                // 2. The object is not a dict, the last path element is a new key
                //    Create case
                // Potential: index on a List that is < capacity > size, this is not implemented yet
                if (isPrimitive(obj->type))
                {
                    if (std::holds_alternative<int>(path[resolvedDepth]))
                    {
                        primitiveIndex = std::get<int>(path[resolvedDepth]);
                        usePrimitiveIndex = true;
                    }
                    else
                    {
                        throw std::runtime_error("Cannot use string as index on primitive array");
                    }
                }
                else if (obj->type == Dict)
                {
                    insertNewKey = true;
                }
                else
                {
                    throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on List object " + ShmemObj::toString(obj));
                }
            }
            else
            {
                throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + ShmemObj::toString(obj));
            }
        }

        // Assign the value
        if (partiallyResolved)
        { // The obj ptr is the work target
            if (usePrimitiveIndex)
                static_cast<ShmemPrimitive_ *>(obj)->set(val, primitiveIndex);
            else if (insertNewKey)
            {
                static_cast<ShmemDict *>(obj)->set(val, path[resolvedDepth], this->heapPtr);
            }
            else
                throw std::runtime_error("Code should not reach here");
        }
        else
        { // The prev ptr is the work target, as the obj ptr is the one get replaced
            if (prev == nullptr)
            {
                if (obj != nullptr)
                {
                    ShmemObj::deconstruct(reinterpret_cast<Byte *>(obj) - this->heapPtr->heapHead(), this->heapPtr);
                }
                // The obj is the root object in the heap, update the entrance point
                this->setEntrance(ShmemObj::construct(val, this->heapPtr));
            }
            else
            {
                int prevType = prev->type;
                if (isPrimitive(prevType))
                {
                    // It is not possible for a primitive to have a child obj
                    throw std::runtime_error("Code should not reach here");
                }
                else if (prevType == List)
                {
                    static_cast<ShmemList *>(prev)->set(val, std::get<int>(path.back()), this->heapPtr);
                }
                else if (prevType == Dict)
                {
                    static_cast<ShmemDict *>(prev)->set(val, path[resolvedDepth], this->heapPtr);
                }
                else
                {
                    throw std::runtime_error("Does not support this type yet");
                }
            }
        }
    }

    // template <typename T>
    // void set(const std::initializer_list<T> &val)
    // {
    //     ShmemObj *obj, *prev;
    //     int resolvedDepth;
    //     resolvePath(prev, obj, resolvedDepth);

    //     bool partiallyResolved = resolvedDepth != path.size();

    //     int primitiveIndex;
    //     bool usePrimitiveIndex = false;
    //     bool insertNewKey = false;

    //     // Deal with unresolved path
    //     if (partiallyResolved)
    //     {
    //         // Only one path element is unresolved
    //         if (resolvedDepth == path.size() - 1)
    //         {
    //             // Two cases:
    //             // 1. The object is a primitive, and the last path is an index on the primitive array
    //             //    Replace case
    //             // 2. The object is not a dict, the last path element is a new key
    //             //    Create case
    //             // Potential: index on a List that is < capacity > size, this is not implemented yet
    //             if (isPrimitive(obj->type))
    //             {
    //                 if (std::holds_alternative<int>(path[resolvedDepth]))
    //                 {
    //                     primitiveIndex = std::get<int>(path[resolvedDepth]);
    //                     usePrimitiveIndex = true;
    //                 }
    //                 else
    //                 {
    //                     throw std::runtime_error("Cannot use string as index on primitive array");
    //                 }
    //             }
    //             else if (obj->type == Dict)
    //             {
    //                 insertNewKey = true;
    //             }
    //             else
    //             {
    //                 throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on List object " + ShmemObj::toString(obj));
    //             }
    //         }
    //         else
    //         {
    //             throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + ShmemObj::toString(obj));
    //         }
    //     }

    //     // Assign the value
    //     if (partiallyResolved)
    //     { // The obj ptr is the work target
    //         if (usePrimitiveIndex)
    //             static_cast<ShmemPrimitive_ *>(obj)->set(val, primitiveIndex);
    //         else if (insertNewKey)
    //         {
    //             static_cast<ShmemDict *>(obj)->set(val, path[resolvedDepth], this->heapPtr);
    //         }
    //         else
    //             throw std::runtime_error("Code should not reach here");
    //     }
    //     else
    //     { // The prev ptr is the work target, as the obj ptr is the one get replaced
    //         if (prev == nullptr)
    //         {
    //             if (obj != nullptr)
    //             {
    //                 ShmemObj::deconstruct(reinterpret_cast<Byte *>(obj) - this->heapPtr->heapHead(), this->heapPtr);
    //             }
    //             // The obj is the root object in the heap, update the entrance point
    //             this->setEntrance(ShmemObj::construct(val, this->heapPtr));
    //         }
    //         else
    //         {
    //             int prevType = prev->type;
    //             if (isPrimitive(prevType))
    //             {
    //                 // It is not possible for a primitive to have a child obj to set
    //                 throw std::runtime_error("Code should not reach here");
    //             }
    //             else if (prevType == List)
    //             {
    //                 static_cast<ShmemList *>(prev)->set(val, std::get<int>(path.back()), this->heapPtr);
    //             }
    //             else if (prevType == Dict)
    //             {
    //                 static_cast<ShmemDict *>(prev)->set(val, path[resolvedDepth], this->heapPtr);
    //             }
    //             else
    //             {
    //                 throw std::runtime_error("Does not support this type yet");
    //             }
    //         }
    //     }
    // }

    // __delitem__
    void del(KeyType index); // For List/Dict

    // __contains__
    template <typename T>
    bool contains(const T &value) const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            return false;
        }

        if (isPrimitive(obj->type))
        {
            return static_cast<ShmemPrimitive_ *>(obj)->contains(value);
        }
        else if (obj->type == List)
        {
            return static_cast<ShmemList *>(obj)->contains(value);
        }
        else if (obj->type == Dict)
        {
            return static_cast<ShmemDict *>(obj)->contains(value);
        }
        else
        {
            throw std::runtime_error("Unknown type");
        }
    }

    // __index__ (for list / primitive array)
    template <typename T>
    int index(const T &value) const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            return -1;
        }

        if (isPrimitive(obj->type))
        {
            return static_cast<ShmemPrimitive_ *>(obj)->index(value);
        }
        else if (obj->type == List)
        {
            return static_cast<ShmemList *>(obj)->index(value);
        }
        else if (obj->type == Dict)
        {
            throw std::runtime_error("index() is not supported on Dict, please use key() instead");
        }
        else
        {
            throw std::runtime_error("Unknown type");
        }
    }

    // __key__ (for dict)
    template <typename T>
    KeyType key(const T &value) const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            return -1;
        }

        if (isPrimitive(obj->type))
        {
            // TODO: reconsider if I should apply key() in List and Primitive
            return static_cast<ShmemPrimitive_ *>(obj)->index(value);
        }
        else if (obj->type == List)
        {
            return static_cast<ShmemList *>(obj)->index(value);
        }
        else if (obj->type == Dict)
        {
            return static_cast<ShmemDict *>(obj)->key(value);
        }
        else
        {
            throw std::runtime_error("Unknown type");
        }
    }

    // __str__
    std::string toString(int maxElements = -1) const;

    // Iterator related
    template <typename T>
    T operator*() const
    {
        return this->get<T>();
    }

    // __iter__
    template <typename T>
    T begin() const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            throw std::runtime_error("Path is not iterable: " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + ShmemObj::toString(obj));
        }

        return this->operator[](obj->beginIdx());
    }

    // __next__
    ShmemAccessor &operator++()
    {
        KeyType lastPath = this->path.back();
        path.pop_back();

        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            throw std::runtime_error("Path is not iterable: " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + ShmemObj::toString(obj));
        }

        KeyType newIdx = obj->nextIdx(lastPath);
        this->path.push_back(newIdx);
        return *this;
    }

    // Iterator compare
    bool operator==(const ShmemAccessor &acc) const
    {
        if (path.size() != acc.path.size())
            return false;

        for (int i = 0; i < path.size(); i++)
        {
            if (path[i] != acc.path[i])
                return false;
        }
        return true;
    }

    bool operator!=(const ShmemAccessor &acc) const
    {
        return !this->operator==(acc);
    }

    // Converters
    template <typename T>
    operator T() const
    {
        return this->get<T>();
    }

    // Arithmetics
    template <typename T>
    ShmemAccessor &operator=(const T &val)
    {
        this->set(val);
        return *this;
    }

    // template <typename T>
    // bool operator=(const std::initializer_list<T> &val) const
    // {
    //     this->set(val);
    //     return *this;
    // }

    template <typename T>
    bool operator==(const T &val) const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        int primitiveIndex;
        bool usePrimitiveIndex = false;

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
                }
                else
                {
                    throw std::runtime_error("Cannot index string: " + std::get<std::string>(path[resolvedDepth]) + " on primitive array");
                }
            }
            else
            {
                throw std::runtime_error("Cannot index " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on primitive object");
            }
        }

        if (usePrimitiveIndex)
        { // The result is a primitive
            if constexpr (isPrimitive<T>() && !isVector<T>::value)
                return reinterpret_cast<ShmemPrimitive_ *>(obj)->operator[]<T>(primitiveIndex) == val;
            else
                throw std::runtime_error("Cannot compare primitive types to a non-primitive value");
        }
        else
            return obj->operator==(val);
    }

    template <typename T>
    bool operator!=(const T &val) const
    {
        return !this->operator==(val);
    }
};

// Overload the << operator for ShmemAccessor
std::ostream &operator<<(std::ostream &os, const ShmemAccessor &acc);

#endif // SHMEM_ACCESSOR_H
