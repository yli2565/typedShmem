#ifndef SHMEM_ACCESSOR_H
#define SHMEM_ACCESSOR_H

#include <iostream>
#include "ShmemObj.h"

ShmemObjInitializer SList();

ShmemObjInitializer SDict();

class ShmemAccessor
{
protected:
    static std::string pathToString(const KeyType *path, int size);
    std::string pathToString() const;

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
        static_assert(((std::is_same_v<KeyTypes, int> || std::is_same_v<KeyTypes, std::variant<int, std::string>> || isString<KeyTypes>()) && ...), "All arguments must be of type KeyType");
        std::vector<KeyType> newPath(this->path);
        newPath.insert(newPath.end(), {accessPath...});
        return ShmemAccessor(this->heapPtr, newPath);
    }

    // Type (Special interface)
    int typeId() const;
    std::string typeStr() const;
    std::string pathString() const;

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
            if (obj != nullptr && isPrimitive(obj->type))
            {
                if (resolvedDepth == path.size() - 1)
                {
                    if (std::holds_alternative<int>(path[resolvedDepth]))
                    {
                        primitiveIndex = std::get<int>(path[resolvedDepth]);
                        usePrimitiveIndex = true;
                    }
                    else
                    {
                        throw IndexError("Cannot index string: " + std::get<std::string>(path[resolvedDepth]) + " on primitive array");
                    }
                }
                else
                {
                    throw IndexError("Cannot index " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on primitive object");
                }
            }
            else
            {
                throw IndexError("Cannot index " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
            }
        }

        if (obj == nullptr)
        {
            if constexpr (std::is_base_of_v<pybind11::object, T>)
            {
                if (!usePrimitiveIndex)
                {
                    return pybind11::none();
                }
            }
            throw IndexError("Cannot index " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + prev->toString());
        }

        // convert to target type
        if (isPrimitive(obj->type))
        {
            if (usePrimitiveIndex)
                return reinterpret_cast<ShmemPrimitive_ *>(obj)->operator[]<T>(primitiveIndex);
            else
                return reinterpret_cast<ShmemPrimitive_ *>(obj)->operator T();
        }
        else if (obj->type == List)
        {
            return reinterpret_cast<ShmemList *>(obj)->operator T();
        }
        else if (obj->type == Dict)
        {
            return reinterpret_cast<ShmemDict *>(obj)->operator T();
        }
        else
        {
            throw std::runtime_error("Unknown type");
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
                if (obj == nullptr)
                {
                    // We want to add something to nullptr, which is impossible
                    throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on null object");
                }
                else if (isPrimitive(obj->type))
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
                    throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on List object " + obj->toString());
                }
            }
            else
            {
                throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
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
                    static_cast<ShmemDict *>(prev)->set(val, path.back(), this->heapPtr);
                }
                else
                {
                    throw std::runtime_error("Does not support this type yet");
                }
            }
        }
    }

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
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, std::variant<int, std::string>> || isString<T>())
            {
                return static_cast<ShmemDict *>(obj)->contains(value);
            }
            else if constexpr (std::is_base_of_v<pybind11::object, T>)
            {
                if (pybind11::isinstance<pybind11::str>(value))
                    return static_cast<ShmemDict *>(obj)->contains(pybind11::cast<std::string>(value));
                else if (pybind11::isinstance<pybind11::int_>(value))
                {
                    return static_cast<ShmemDict *>(obj)->contains(pybind11::cast<int>(value));
                }
                else
                {
                    return false;
                }
            }
            return false;
        }
        else
        {
            throw std::runtime_error("Unknown obj type");
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

    // Type specific methods
    // Dict
    template <typename T>
    void add(const T &value, const KeyType &key)
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            throw std::runtime_error("Path is not iterable: " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
        }

        if (obj->type == Dict)
            static_cast<ShmemDict *>(obj)->set(value, key, this->heapPtr);
        else
            throw std::runtime_error("Cannot add a key-value pair to a non-dict object");
    }

    template <typename T>
    void add(const T &value) const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            throw std::runtime_error("Path is not iterable: " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
        }

        if (obj->type == List)
            static_cast<ShmemList *>(obj)->append(value, this->heapPtr);
        else
            throw std::runtime_error("Cannot add a single value to a non-list object");
    }

    // Quick add
    void add(const std::initializer_list<float> &value) const
    {
        this->add(std::vector<float>(value));
    }

    void add(const std::initializer_list<const char *> &value) const
    {
        std::vector<std::string> stringVec;
        for (const char *str : value)
        {
            stringVec.push_back(std::string(str));
        }

        this->add(stringVec);
    }

    void add(const std::initializer_list<std::pair<const int, float>> &value) const
    {
        this->add(std::map<int, float>(value));
    }

    void add(const std::initializer_list<std::pair<const std::string, float>> &value) const
    {
        this->add(std::map<std::string, float>(value));
    }

    void add(const std::initializer_list<std::pair<const int, const char *>> &value) const
    {
        std::map<int, std::string> stringMap;
        for (const auto &pair : value)
        {
            stringMap.insert({pair.first, std::string(pair.second)});
        }
        this->add(stringMap);
    }

    void add(const std::initializer_list<std::pair<const std::string, const char *>> &value) const
    {
        std::map<std::string, std::string> stringMap;
        for (const auto &pair : value)
        {
            stringMap.insert({pair.first, std::string(pair.second)});
        }
        this->add(stringMap);
    }

    // Iterator related
    // template <typename T>
    // T operator*() const
    // {
    //     return this->get<T>();
    // }
    ShmemAccessor operator*() const
    {
        return *this;
    }

    // __iter__
    ShmemAccessor begin() const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            throw std::runtime_error("Path is not iterable: " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
        }

        return this->operator[](obj->beginIdx());
    }

    ShmemAccessor end() const
    {
        ShmemObj *obj, *prev;
        int resolvedDepth;
        resolvePath(prev, obj, resolvedDepth);

        if (resolvedDepth != path.size())
        {
            throw std::runtime_error("Path is not iterable: " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
        }

        return this->operator[](obj->endIdx());
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
            throw StopIteration("Path is not iterable: " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
        }

        KeyType newIdx = obj->nextIdx(lastPath);
        this->path.push_back(newIdx);
        return *this;
    }

    ShmemAccessor operator++(int)
    {
        ShmemAccessor temp = *this;
        ++(*this);
        return temp;
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

    // Quick assignments

    // Assign vector
    ShmemAccessor &operator=(const std::initializer_list<float> &val)
    {
        this->set(std::vector<float>(val));
        return *this;
    }
    ShmemAccessor &operator=(const std::initializer_list<const char *> &val)
    {
        std::vector<std::string> stringVec;
        for (const char *str : val)
        {
            stringVec.push_back(std::string(str));
        }

        this->set(stringVec);
        return *this;
    }

    // Assign map
    ShmemAccessor &operator=(const std::initializer_list<std::pair<const std::string, float>> &val)
    {
        std::map<std::string, float> mapVal;
        for (const auto &pair : val)
        {
            mapVal[pair.first] = pair.second;
        }

        this->set(mapVal);
        return *this;
    }
    ShmemAccessor &operator=(const std::initializer_list<std::pair<const int, float>> &val)
    {
        std::map<int, float> mapVal;
        for (const auto &pair : val)
        {
            mapVal[pair.first] = pair.second;
        }

        this->set(mapVal);
        return *this;
    }
    ShmemAccessor &operator=(const std::initializer_list<std::pair<const std::string, const char *>> &val)
    {
        std::map<std::string, std::string> mapVal;
        for (const auto &pair : val)
        {
            mapVal[pair.first] = std::string(pair.second);
        }

        this->set(mapVal);
        return *this;
    }
    ShmemAccessor &operator=(const std::initializer_list<std::pair<const int, const char *>> &val)
    {
        std::map<int, std::string> mapVal;
        for (const auto &pair : val)
        {
            mapVal[pair.first] = std::string(pair.second);
        }

        this->set(mapVal);
        return *this;
    }

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
                if (isPrimitive(obj->type) && std::holds_alternative<int>(path[resolvedDepth]))
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
