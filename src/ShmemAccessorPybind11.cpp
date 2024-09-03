#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "ShmemAccessor.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "ShmemAccessor.h"

namespace py = pybind11;

// Wrapper class for ShmemAccessor that handles specific types
class ShmemAccessorWrapper : public ShmemAccessor
{
public:
    ShmemAccessorWrapper(ShmemHeap *heap) : ShmemAccessor(heap) {}
    ShmemAccessorWrapper(ShmemHeap *heap, std::vector<KeyType> keys) : ShmemAccessor(heap, keys) {}

    // __getitem__
    ShmemAccessorWrapper __getitem__(const py::object &keys) const
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
    ShmemAccessorWrapper operator[](const py::args &keys) const
    {
        return this->__getitem__(keys);
    }

    void __setitem__(const py::object &indexOrKey, const py::object &value) const
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

    void __delitem__(const py::object &indexOrKey)
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

    bool __contains__(const py::object &value) const
    {
        return this->contains(value);
    }

    int __len__() const
    {
        return this->ShmemAccessor::len();
    }

    py::object __str__() const
    {
        return py::str(this->toString());
    }

    ShmemAccessorWrapper __iter__() const
    {
        return this->begin();
    }

    py::object __next__()
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

    py::bool_ __eq__(const py::object &other) const
    {
        // Check if the other object is of the same C++ type
        if (py::isinstance<ShmemAccessorWrapper>(other))
        {
            // Cast to the same type and compare using custom logic
            const ShmemAccessorWrapper &otherObj = other.cast<const ShmemAccessorWrapper &>();
            return this->operator==(otherObj); // Example comparison
        }
        else
        {
            return this->fetch().equal(other);
        }
    }

    // Additional methods

    void insert(const py::object &value, const py::object &key)
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

    void add(const py::object &value)
    {
        this->ShmemAccessor::add(value);
    }

    py::object fetch() const
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
            if (usePrimitiveIndex)
                throw IndexError("Cannot index element " + std::to_string(primitiveIndex) + " on nullptr");
            else
                return pybind11::none();
        }

        // convert to target type
        if (isPrimitive(obj->type))
        {
            if (usePrimitiveIndex)
                return reinterpret_cast<ShmemPrimitive_ *>(obj)->elementToPyObject(primitiveIndex);
            else
                return reinterpret_cast<ShmemPrimitive_ *>(obj)->operator pybind11::object();
        }
        else if (obj->type == List || obj->type == Dict)
        {
            return obj->operator pybind11::object();
        }
        else
        {
            throw std::runtime_error("Unknown type");
        }
    }
};

PYBIND11_MODULE(TypedShmem, m)
{
    py::class_<ShmemAccessorWrapper>(m, "ShmemAccessor")
        .def(py::init<ShmemHeap *>())
        .def(py::init<ShmemHeap *, std::vector<KeyType>>())
        .def("__getitem__", &ShmemAccessorWrapper::__getitem__)
        .def("__setitem__", &ShmemAccessorWrapper::__setitem__)
        .def("__delitem__", &ShmemAccessorWrapper::__delitem__)
        .def("__contains__", &ShmemAccessorWrapper::__contains__)
        .def("__len__", &ShmemAccessorWrapper::__len__)
        .def("__str__", &ShmemAccessorWrapper::__str__)
        .def("typeId", &ShmemAccessorWrapper::typeId)
        .def("typeStr", &ShmemAccessorWrapper::typeStr)
        .def("get", &ShmemAccessorWrapper::get<py::object>)
        .def("set", &ShmemAccessorWrapper::set<py::object>)
        .def("add", &ShmemAccessorWrapper::add)
        .def("insert", &ShmemAccessorWrapper::insert)

        // .def("operator++", &ShmemAccessorWrapper::operator++)
        // .def("operator!=", py::overload_cast<const ShmemAccessorWrapper &>(&ShmemAccessorWrapper::operator!=))
        .def("__repr__", [](const ShmemAccessorWrapper &a)
             { return "<ShmemAccessor>"; })

        // Iterator related
        .def("next", &ShmemAccessorWrapper::__next__)
        .def("__iter__", &ShmemAccessorWrapper::__iter__) // Return self in __iter__
        .def("__next__", &ShmemAccessorWrapper::__next__) // Bind the next method to __next__

        // Arithmetic
        .def("__eq__", &ShmemAccessorWrapper::__eq__);
}
