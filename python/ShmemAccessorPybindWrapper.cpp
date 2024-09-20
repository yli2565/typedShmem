// ShmemAccessorWrapper.cpp
#include "ShmemAccessorPybindWrapper.h"

// Constructor definitions

ShmemAccessorWrapper::ShmemAccessorWrapper(ShmemHeap *heapPtr) : ShmemAccessor(heapPtr) {}
ShmemAccessorWrapper::ShmemAccessorWrapper(ShmemHeap *heapPtr, std::vector<KeyType> path) : ShmemAccessor(heapPtr, path) {}

ShmemAccessorWrapper::ShmemAccessorWrapper(ShmemHeap &heap)
    : ShmemAccessor(&heap) {}

ShmemAccessorWrapper::ShmemAccessorWrapper(ShmemHeap &heap, py::list keys) : ShmemAccessor(&heap)
{
    this->path.reserve(keys.size());
    for (auto key : keys)
    {
        if (py::isinstance<py::int_>(key))
        {
            this->path.push_back(key.cast<int>());
        }
        else if (py::isinstance<py::str>(key))
        {
            this->path.push_back(key.cast<std::string>());
        }
        else
        {
            throw py::type_error("Invalid type in access path");
        }
    }
}

// Method implementations
ShmemAccessorWrapper ShmemAccessorWrapper::__getitem__(const py::args &keys) const
{
    std::vector<KeyType> accessPath(this->path);
    for (auto arg : keys)
    {
        if (py::isinstance<py::int_>(arg))
        {
            accessPath.push_back(arg.cast<int>());
        }
        else if (py::isinstance<py::str>(arg))
        {
            accessPath.push_back(arg.cast<std::string>());
        }
        else
        {
            throw py::type_error("Invalid type in access path");
        }
    }
    return ShmemAccessorWrapper(this->heapPtr, accessPath);
}

ShmemAccessorWrapper ShmemAccessorWrapper::operator[](const py::args &keys) const
{
    return this->__getitem__(keys);
}

void ShmemAccessorWrapper::__setitem__(const py::object &indexOrKey, const py::object &value) const
{
    if (py::isinstance<py::str>(indexOrKey))
    {
        ShmemAccessorWrapper target = this->ShmemAccessor::operator[](py::cast<std::string>(indexOrKey));
        target.set(value);
    }
    else if (py::isinstance<py::int_>(indexOrKey))
    {
        ShmemAccessorWrapper target = this->ShmemAccessor::operator[](py::cast<int>(indexOrKey));
        target.set(value);
    }
    else
    {
        throw py::type_error("index or key must be either string or int.");
    }
}

void ShmemAccessorWrapper::__delitem__(const py::object &indexOrKey)
{
    if (py::isinstance<py::str>(indexOrKey))
    {
        this->del(py::cast<std::string>(indexOrKey));
    }
    else if (py::isinstance<py::int_>(indexOrKey))
    {
        this->del(py::cast<int>(indexOrKey));
    }
    else
    {
        throw py::type_error("index or key must be either string or int.");
    }
}

bool ShmemAccessorWrapper::__contains__(const py::object &value) const
{
    return this->contains(value);
}

int ShmemAccessorWrapper::__len__() const
{
    return this->ShmemAccessor::len();
}

py::object ShmemAccessorWrapper::__str__() const
{
    return py::str(this->toString());
}

ShmemAccessorWrapper ShmemAccessorWrapper::__iter__() const
{
    return this->begin();
}

py::object ShmemAccessorWrapper::__next__()
{
    py::object result = this->fetch();
    try
    {
        this->operator++();
    }
    catch (StopIteration &)
    {
        throw py::stop_iteration();
    }
    return result;
}

py::bool_ ShmemAccessorWrapper::__eq__(const py::object &other) const
{
    if (py::isinstance<ShmemAccessorWrapper>(other))
    {
        const ShmemAccessorWrapper &otherObj = other.cast<const ShmemAccessorWrapper &>();
        return this->operator==(otherObj);
    }
    else
    {
        return this->fetch().equal(other);
    }
}

void ShmemAccessorWrapper::insert(const py::object &key, const py::object &value)
{
    if (py::isinstance<py::str>(key))
    {
        this->ShmemAccessor::add(value, py::cast<std::string>(key));
    }
    else if (py::isinstance<py::int_>(key))
    {
        this->ShmemAccessor::add(value, py::cast<int>(key));
    }
    else
    {
        throw py::type_error("Key must be either string or int.");
    }
}

void ShmemAccessorWrapper::add(const py::object &value)
{
    this->ShmemAccessor::add(value);
}

py::object ShmemAccessorWrapper::fetch() const
{
    ShmemObj *obj, *prev;
    int resolvedDepth;
    resolvePath(prev, obj, resolvedDepth);

    int primitiveIndex;
    bool usePrimitiveIndex = false;

    if (static_cast<size_t>(resolvedDepth) != path.size())
    {
        if (obj != nullptr && isPrimitive(obj->type))
        {
            if (static_cast<size_t>(resolvedDepth) == path.size() - 1)
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
                throw IndexError("Cannot index " + pathToString(path.data() + resolvedDepth, static_cast<int>(path.size()) - resolvedDepth) + " on primitive object");
            }
        }
        else
        {
            throw IndexError("Cannot index " + pathToString(path.data() + resolvedDepth, static_cast<int>(path.size()) - resolvedDepth) + " on object " + obj->toString());
        }
    }

    if (obj == nullptr)
    {
        if (usePrimitiveIndex)
        {
            throw IndexError("Cannot index element " + std::to_string(primitiveIndex) + " on nullptr");
        }
        else
        {
            return pybind11::none();
        }
    }

    if (isPrimitive(obj->type))
    {
        if (usePrimitiveIndex)
        {
            return reinterpret_cast<ShmemPrimitive_ *>(obj)->elementToPyObject(primitiveIndex);
        }
        else
        {
            return reinterpret_cast<ShmemPrimitive_ *>(obj)->operator pybind11::object();
        }
    }
    else if (obj->type == List || obj->type == Dict)
    {
        return obj->operator pybind11::object();
    }
    else
    {
        throw std::runtime_error("ShmemAccessorWrapper.fetch(): Unknown type: " + std::to_string(obj->type));
    }
}
