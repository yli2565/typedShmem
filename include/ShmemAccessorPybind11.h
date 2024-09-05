#ifndef SHMEM_ACCESSOR_PYBIND11_H
#define SHMEM_ACCESSOR_PYBIND11_H

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "ShmemAccessor.h" 

namespace py = pybind11;

class ShmemAccessorWrapper : public ShmemAccessor
{
public:
    ShmemAccessorWrapper(ShmemHeap *heap);
    ShmemAccessorWrapper(ShmemHeap *heap, std::vector<KeyType> keys);
    ShmemAccessorWrapper(ShmemHeap &heap);
    ShmemAccessorWrapper(ShmemHeap &heap, py::list keys);

    ShmemAccessorWrapper __getitem__(const py::object &keys) const;
    ShmemAccessorWrapper operator[](const py::args &keys) const;
    void __setitem__(const py::object &indexOrKey, const py::object &value) const;
    void __delitem__(const py::object &indexOrKey);
    bool __contains__(const py::object &value) const;
    int __len__() const;
    py::object __str__() const;
    ShmemAccessorWrapper __iter__() const;
    py::object __next__();
    py::bool_ __eq__(const py::object &other) const;

    void insert(const py::object &value, const py::object &key);
    void add(const py::object &value);
    py::object fetch() const;
};

#endif