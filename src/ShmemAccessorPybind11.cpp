#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "ShmemAccessor.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "ShmemAccessor.h"

namespace py = pybind11;

// Wrapper class for ShmemAccessor that handles specific types
class ShmemAccessorWrapper : ShmemAccessor
{
public:
    ShmemAccessorWrapper(ShmemHeap *heap) : ShmemAccessor(heap) {}
    ShmemAccessorWrapper(ShmemHeap *heap, std::vector<KeyType> keys) : ShmemAccessor(heap, keys) {}

    // __getitem__
    ShmemAccessorWrapper operator[](const py::args &args) const
    {
        std::vector<KeyType> accessPath(this->path);
        for (auto arg : args)
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
                throw std::runtime_error("Invalid type in access path");
            }
        }
        return ShmemAccessorWrapper(this->heapPtr, accessPath);
    }

    void __setitem__(const KeyType &key, const py::object &value) const
    {
        ShmemAccessorWrapper target = this->ShmemAccessor::operator[](key);
        target.set(value);
    }

    void __delitem__(const KeyType &key)
    {
        this->del(key);
    }

    bool __contains__(const py::object &key) const
    {
        return this->contains(key);
    }
    py::object __str__() const
    {
        return py::str(this->toString());
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
            if (isPrimitive(obj->type))
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
                        throw std::runtime_error("Cannot index string: " + std::get<std::string>(path[resolvedDepth]) + " on primitive array");
                    }
                }
                else
                {
                    throw std::runtime_error("Cannot index " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on primitive object");
                }
            }
            else
            {
                throw std::runtime_error("Cannot index " + pathToString(path.data() + resolvedDepth, path.size() - resolvedDepth) + " on object " + obj->toString());
            }
        }

        // convert to target type
        if (isPrimitive(obj->type))
        {
            if (usePrimitiveIndex)
                return reinterpret_cast<ShmemPrimitive_ *>(obj)->elementToPyObject(primitiveIndex);
            else
                return reinterpret_cast<ShmemPrimitive_ *>(obj)->operator pybind11::object();
        }
        else if (obj->type == List)
        {
            return reinterpret_cast<ShmemList *>(obj)->operator pybind11::list();
        }
        else if (obj->type == Dict)
        {
            return reinterpret_cast<ShmemDict *>(obj)->operator pybind11::dict();
        }
        else
        {
            throw ConversionError("Cannot convert to this type (C++ doesn't support dynamic type)");
        }
    }
};

PYBIND11_MODULE(TypedShmem, m)
{
    py::class_<ShmemAccessorWrapper>(m, "ShmemAccessor")
        .def(py::init<ShmemHeap *>())
        .def(py::init<ShmemHeap *, std::vector<KeyType>>())
        .def("__getitem__", &ShmemAccessorWrapper::operator[], py::is_operator())
        .def("__setitem__", &ShmemAccessor::set<int>)
        .def("__delitem__", &ShmemAccessor::del)
        .def("__contains__", &ShmemAccessor::contains<int>)
        .def("__len__", &ShmemAccessor::len)
        .def("__str__", &ShmemAccessor::toString)
        .def("typeId", &ShmemAccessor::typeId)
        .def("typeStr", &ShmemAccessor::typeStr)
        .def("get", &ShmemAccessor::get<int>)
        .def("set", &ShmemAccessor::set<int>)
        .def("add", py::overload_cast<const int &>(&ShmemAccessor::add<int>))
        .def("begin", &ShmemAccessor::begin)
        .def("end", &ShmemAccessor::end)
        .def("operator++", &ShmemAccessor::operator++)
        .def("operator==", py::overload_cast<const ShmemAccessor &>(&ShmemAccessor::operator==))
        .def("operator!=", py::overload_cast<const ShmemAccessor &>(&ShmemAccessor::operator!=))
        .def("__repr__", [](const ShmemAccessor &a)
             { return "<ShmemAccessor>"; })
        .def("__iter__", [](ShmemAccessor &a)
             { return py::make_iterator(a.begin(), a.end()); }, py::keep_alive<0, 1>());
}
