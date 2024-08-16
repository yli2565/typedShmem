#ifndef SHMEM_ACCESSOR_H
#define SHMEM_ACCESSOR_H

#include <iostream>
#include "ShmemObj.h"

class ShmemAccessor
{
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

    template <typename T>
    operator T() const
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
                    throw std::runtime_error("Cannot index <string> on primitive array");
                }
            }
            else
            {
                throw std::runtime_error("Cannot index <remaining index> on primitive object");
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
                    return reinterpret_cast<ShmemPrimitive<T> *>(obj)->operator[](primitiveIndex);
                else
                    return reinterpret_cast<ShmemPrimitive<T> *>(obj)->operator T();
            }
        }
        else if constexpr (isString<T>())
        {
            return reinterpret_cast<ShmemPrimitive<char> *>(obj)->operator T();
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
        { // The obj ptr is the work target
            if (usePrimitiveIndex)
                static_cast<ShmemPrimitive<T> *>(obj)->set(primitiveIndex, val);
            else if (insertNewKey)
            {
                static_cast<ShmemDict *>(obj)->insert(path[resolvedDepth], val, this->heapPtr);
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
                    static_cast<ShmemList *>(prev)->assign(std::get<int>(path.back()), val, this->heapPtr);
                }
                else if (prevType == Dict)
                {
                    static_cast<ShmemDict *>(prev)->insert(path[resolvedDepth], val, this->heapPtr);
                }
                else
                {
                    throw std::runtime_error("Does not support this type yet");
                }
            }
        }

        return *this;
    }

    std::string toString(int maxElements = 4) const;
};

// Overload the << operator for ShmemAccessor
std::ostream &operator<<(std::ostream &os, const ShmemAccessor &acc);

#endif // SHMEM_ACCESSOR_H
