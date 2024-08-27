// #include <pybind11/pybind11.h>
// #include <pybind11/stl.h>
// #include "ShmemAccessor.h"

// namespace py = pybind11;

// PYBIND11_MODULE(shmem_accessor, m) {
//     py::class_<ShmemAccessor>(m, "ShmemAccessor")
//         .def(py::init<ShmemHeap *>())
//         .def(py::init<ShmemHeap *, std::vector<KeyType>>())
//         .def("__getitem__", &ShmemAccessor::operator[], py::is_operator())
//         .def("__setitem__", &ShmemAccessor::set<int>)
//         .def("__delitem__", &ShmemAccessor::del)
//         .def("__contains__", &ShmemAccessor::contains<int>)
//         .def("__len__", &ShmemAccessor::len)
//         .def("__str__", &ShmemAccessor::toString)
//         .def("type_id", &ShmemAccessor::typeId)
//         .def("type_str", &ShmemAccessor::typeStr)
//         .def("get", &ShmemAccessor::get<int>)
//         .def("set", &ShmemAccessor::set<int>)
//         .def("add", py::overload_cast<const int&>(&ShmemAccessor::add<int>))
//         .def("begin", &ShmemAccessor::begin)
//         .def("end", &ShmemAccessor::end)
//         .def("operator++", &ShmemAccessor::operator++)
//         .def("operator==", py::overload_cast<const ShmemAccessor&>(&ShmemAccessor::operator==))
//         .def("operator!=", py::overload_cast<const ShmemAccessor&>(&ShmemAccessor::operator!=))
//         .def("__repr__", [](const ShmemAccessor &a) { return "<ShmemAccessor>"; })
//         .def("__iter__", [](ShmemAccessor &a) { return py::make_iterator(a.begin(), a.end()); }, py::keep_alive<0, 1>()); 
// }
