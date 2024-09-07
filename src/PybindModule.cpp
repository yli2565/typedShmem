#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <pybind11/embed.h>
#include <pybind11/functional.h>

#include "ShmemObj.h"
#include "ShmemAccessorPybindWrapper.h"
PYBIND11_MODULE(TypedShmem, m)
{
    m.doc() = "TypedShmem is interthread communication module for C++/Python.";

    py::class_<ShmemHeap>(m, "ShmemHeap")
        .def(py::init<const std::string &, size_t, size_t>(),
             py::arg("name"), py::arg("staticSpaceSize") = DSCap, py::arg("heapSize") = DHCap)
        .def("create", &ShmemHeap::create)
        .def("connect", &ShmemHeap::connect)
        .def("close", &ShmemHeap::close)
        .def("unlink", &ShmemHeap::unlink)
        .def("resize", py::overload_cast<size_t>(&ShmemHeap::resize))
        .def("resize", py::overload_cast<size_t, size_t>(&ShmemHeap::resize))
        .def("shmalloc", &ShmemHeap::shmalloc)
        .def("shrealloc", &ShmemHeap::shrealloc)
        .def("shfree", static_cast<int (ShmemHeap::*)(size_t)>(&ShmemHeap::shfree), py::arg("offset"))
        .def("getHCap", &ShmemHeap::getHCap)
        .def("getSCap", &ShmemHeap::getSCap)
        .def("staticCapacity", &ShmemHeap::staticCapacity, py::return_value_policy::reference)
        .def("heapCapacity", &ShmemHeap::heapCapacity, py::return_value_policy::reference)
        .def("freeBlockListOffset", &ShmemHeap::freeBlockListOffset, py::return_value_policy::reference)
        .def("entranceOffset", &ShmemHeap::entranceOffset, py::return_value_policy::reference)
        .def("staticSpaceHead", &ShmemHeap::staticSpaceHead)
        .def("entrance", &ShmemHeap::entrance)
        .def("heapHead", &ShmemHeap::heapHead)
        .def("heapTail", &ShmemHeap::heapTail)
        .def("freeBlockList", &ShmemHeap::freeBlockList, py::return_value_policy::reference)
        .def("setHCap", &ShmemHeap::setHCap)
        .def("setSCap", &ShmemHeap::setSCap)
        // spdlog is not usable in python, so we don't expose the instance, instead, we set some common attribute functions
        // .def("getLogger", &ShmemHeap::getLogger)
        .def("setLogLevel", [](ShmemHeap *heap, int level)
             { heap->getLogger()->set_level(spdlog::level::level_enum(level)); })
        .def("setLogPattern", [](ShmemHeap *heap, const std::string &pattern)
             { heap->getLogger()->set_pattern(pattern); })

        .def("printShmHeap", &ShmemHeap::printShmHeap)
        .def("briefLayout", &ShmemHeap::briefLayout)
        .def("briefLayoutStr", &ShmemHeap::briefLayoutStr);

    // Initializers
    py::class_<ShmemObjInitializer>(m, "ShmemObjInitializer")
        .def(py::init<int>(), py::arg("typeId"));

    m.def("SDict", &SDict);
    m.def("SList", &SList);

    m.def("setShmemUtilLogLevel", [](int level)
          { ShmemUtils::getLogger()->set_level(spdlog::level::level_enum(level)); });

    py::class_<ShmemAccessorWrapper>(m, "ShmemAccessor")
        .def(py::init<ShmemHeap &>(), py::arg("heap"))
        .def(py::init<ShmemHeap &, py::list>(), py::arg("heap"), py::arg("keys"))

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
        .def("__repr__", [](const ShmemAccessorWrapper &a)
             { return "<ShmemAccessor>"; })
        // Iterator related
        .def("next", &ShmemAccessorWrapper::__next__)
        .def("__iter__", &ShmemAccessorWrapper::__iter__) // Return self in __iter__
        .def("__next__", &ShmemAccessorWrapper::__next__) // Bind the next method to __next__
        // Arithmetic
        .def("__eq__", &ShmemAccessorWrapper::__eq__);
}