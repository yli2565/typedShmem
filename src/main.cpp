#include <iostream>
#include <pybind11/pybind11.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "ShmemObj.h"
#include "ShmemAccessor.h"
using namespace std;
namespace py = pybind11;

int main(int argc, char **argv)
{
    Py_Initialize();

    ShmemHeap shmHeap("test_shm_heap", 80, 1024);
    ShmemAccessor acc(&shmHeap);

    shmHeap.create();

    acc.set(vector<int>({1, 2, 3, 4, 5, 6, 7}));
    acc[2].set(py::object(py::int_(9)));

    cout << acc << endl;
    return 0;
}
