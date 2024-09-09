#include "ShmemAccessor.h"

ShmemObjInitializer SList(const pybind11::object &iniList)
{
    // pybind11::object type = pybind11::reinterpret_borrow<pybind11::object>(iniList.get_type());
    // std::string typeName = type.attr("__name__").cast<std::string>();
    // std::cout << typeName << std::endl;
    return ShmemObjInitializer(List, iniList);
}

ShmemObjInitializer SDict(const pybind11::object &iniDict)
{
    return ShmemObjInitializer(Dict, iniDict);
}

// ShmemAccessor constructors
ShmemAccessor::ShmemAccessor(ShmemHeap *heapPtr) : heapPtr(heapPtr), path({}) {}

ShmemAccessor::ShmemAccessor(ShmemHeap *heapPtr, std::vector<KeyType> path) : heapPtr(heapPtr), path(path) {}

inline ShmemObj *ShmemAccessor::entrance() const
{
    return reinterpret_cast<ShmemObj *>(this->heapPtr->entrance());
}

void ShmemAccessor::setEntrance(size_t offset)
{
    this->heapPtr->entranceOffset() = offset;
}

void ShmemAccessor::setEntrance(ShmemObj *obj)
{
    this->heapPtr->entranceOffset() = reinterpret_cast<Byte *>(obj) - this->heapPtr->heapHead();
}

void ShmemAccessor::resolvePath(ShmemObj *&prevObj, ShmemObj *&obj, int &resolvedDepth) const
{
    ShmemObj *current = this->entrance();
    ShmemObj *prev = nullptr;
    int resolveDepth = 0;
    for (resolveDepth = 0; resolveDepth < path.size(); resolveDepth++)
    {
        KeyType pathElement = path[resolveDepth];
        try
        {
            if (current == nullptr)
            {
                break;
            }
            else if (isPrimitive(current->type))
            { // This mean there's an additional index on a primitive
                break;
            }
            else if (current->type == Dict)
            {
                ShmemDict *currentDict = static_cast<ShmemDict *>(current);

                prev = current;
                current = currentDict->get(pathElement);
            }
            else if (current->type == List)
            {
                ShmemList *currentList = static_cast<ShmemList *>(current);
                if (std::holds_alternative<std::string>(pathElement))
                {
                    throw std::runtime_error("Cannot index <string> on List");
                }

                prev = current;
                current = currentList->getObj(std::get<int>(pathElement));
            }
        }
        catch (IndexError &e)
        {
            break;
        }
    }
    prevObj = prev;
    obj = current;
    resolvedDepth = resolveDepth;
    return;
}

// Type (Special interface)
int ShmemAccessor::typeId() const
{
    ShmemObj *obj, *prev;
    int resolvedDepth;
    resolvePath(prev, obj, resolvedDepth);

    if (resolvedDepth != path.size())
    {
        if (obj == nullptr)
        {
            throw std::runtime_error("Path resolution failed on nullptr");
        }
        if (resolvedDepth == path.size() - 1 && isPrimitive(obj->type) && std::holds_alternative<int>(path[resolvedDepth]))
        { // Return the typeId of primitive element
            return obj->type;
        }
        throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
    }
    if (obj == nullptr)
    { // TODO: should I return typedId 0?
        throw std::runtime_error("nullptr does not have typeId");
    }
    return obj->type;
}

std::string ShmemAccessor::typeStr() const
{
    return typeNames.at(this->typeId());
}

std::string ShmemAccessor::pathString() const
{
    return pathToString(path.data(), path.size());
}

// __len__ implementation
size_t ShmemAccessor::len() const
{
    ShmemObj *obj, *prev;
    int resolvedDepth;
    resolvePath(prev, obj, resolvedDepth);

    if (resolvedDepth != path.size())
    {
        if (obj == nullptr)
        {
            throw std::runtime_error("Path resolution failed on nullptr");
        }
        if (resolvedDepth == path.size() - 1 && isPrimitive(obj->type) && std::holds_alternative<int>(path[resolvedDepth]))
        {
            throw std::runtime_error("Cannot call len() on primitive element");
        }
        std::string lastPath = std::holds_alternative<int>(path[resolvedDepth]) ? std::to_string(std::get<int>(path[resolvedDepth])) : std::get<std::string>(path[resolvedDepth]);
        throw std::runtime_error("Cannot resolve " + lastPath + "on object" + obj->toString());
    }

    if (obj == nullptr)
    {
        throw std::runtime_error("Cannot get len of nullptr");
    }

    if (isPrimitive(obj->type))
    {
        return static_cast<ShmemPrimitive_ *>(obj)->len();
    }
    else if (obj->type == List)
    {
        return static_cast<ShmemList *>(obj)->len();
    }
    else if (obj->type == Dict)
    {
        return static_cast<ShmemDict *>(obj)->len();
    }
    else
    {
        throw std::runtime_error("Cannot get len of " + typeNames.at(obj->type));
    }
}

// __delitem__
void ShmemAccessor::del(KeyType index)
{
    ShmemObj *obj, *prev;
    int resolvedDepth;
    resolvePath(prev, obj, resolvedDepth);

    if (resolvedDepth != path.size())
    {
        if (obj == nullptr)
        {
            throw std::runtime_error("Path resolution failed on nullptr");
        }
        if (resolvedDepth == path.size() - 1 && isPrimitive(obj->type) && std::holds_alternative<int>(path[resolvedDepth]))
        {
            throw std::runtime_error("Cannot call del(index) on primitive element, please call it on the primitive array");
        }
        throw std::runtime_error("Cannot resolve " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
    }
    if (obj == nullptr)
    {
        throw std::runtime_error("Cannot del on nullptr");
    }
    if (isPrimitive(obj->type))
    {
        if (std::holds_alternative<std::string>(index))
        {
            throw std::runtime_error("Cannot use string as index on Primitive Object");
        }
        static_cast<ShmemPrimitive_ *>(obj)->del(std::get<int>(index));
        // throw std::runtime_error("Cannot delete from " + typeNames.at(obj->type) + " Primitive Object, as it's immutable");
    }
    else if (obj->type == List)
    {
        if (std::holds_alternative<std::string>(index))
        {
            throw std::runtime_error("Cannot use string as index on List Object");
        }
        static_cast<ShmemList *>(obj)->del(std::get<int>(index), this->heapPtr);
    }
    else if (obj->type == Dict)
    {
        static_cast<ShmemDict *>(obj)->del(index, this->heapPtr);
    }
    else
    {
        throw std::runtime_error("Cannot delete from " + typeNames.at(obj->type) + " Primitive Object, as it's immutable");
    }
}

// __str__ implementation
std::string ShmemAccessor::toString(int maxElements) const
{
    ShmemObj *obj, *prev;
    int resolvedDepth;
    resolvePath(prev, obj, resolvedDepth);

    bool partiallyResolved = resolvedDepth != path.size();

    int primitiveIndex;
    bool usePrimitiveIndex = false;

    if (partiallyResolved)
    {
        if (obj == nullptr)
        {
            throw std::runtime_error("Path resolution failed on nullptr");
        }
        else if (resolvedDepth == path.size() - 1 && isPrimitive(obj->type) && std::holds_alternative<int>(path[resolvedDepth]))
        {
            primitiveIndex = std::get<int>(path[resolvedDepth]);
            usePrimitiveIndex = true;
        }
        else
        {
            throw std::runtime_error("Cannot index <remaining index> on object" + obj->toString());
        }
    }

    if (obj == nullptr)
    {
        return "nullptr";
    }

    if (usePrimitiveIndex)
    {
        return static_cast<ShmemPrimitive_ *>(obj)->elementToString(primitiveIndex);
    }
    else
        return obj->toString(0, maxElements);
}

std::ostream &operator<<(std::ostream &os, const ShmemAccessor &acc)
{
    os << acc.toString();
    return os;
}

// Utilities
std::string ShmemAccessor::pathToString(const KeyType *path, int size)
{
    std::string pathStr;
    for (int i = 0; i < size; i++)
    {
        KeyType pathElement = path[i];
        pathStr += std::holds_alternative<int>(pathElement) ? std::to_string(std::get<int>(pathElement)) : std::get<std::string>(pathElement);
        if (i != size - 1)
            pathStr += ".";
    }
    return pathStr;
}

std::string ShmemAccessor::pathToString() const
{
    return pathToString(path.data(), path.size());
}
