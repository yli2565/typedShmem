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
    ShmemHeap shmHeap("test_shm_heap", 80, 1024);
    ShmemAccessor acc(&shmHeap);

    shmHeap.getLogger()->set_level(spdlog::level::info);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    shmHeap.getLogger()->sinks().clear();
    shmHeap.getLogger()->sinks().push_back(console_sink);
    // Initialize necessary objects/resources
    shmHeap.create();
    Py_Initialize();

    acc = {{"A", 1}, {"BB", 11}, {"CCC", 111}, {"DDDD", 1111}, {"EEEEE", 11111}};
    acc["A"] = vector<vector<int>>({{2, 3, 4}, {5, 6, 7}, {8, 9, 10}, {11, 12, 13}, {14, 15, 16}});

    std::cout << acc.begin().pathString() << std::endl;
    std::cout << acc.end().pathString() << std::endl;

    for (ShmemAccessor v = acc.begin(); v != acc.end(); v++)
    {
        std::cout << v.pathString() << std::endl;

        std::cout << v.begin().pathString() << std::endl;
        std::cout << v.end().pathString() << std::endl;
        for (ShmemAccessor it = v.begin(); it != v.end(); it++)
        {
            std::cout << it.pathString() << std::endl;
            std::cout << it << " " << std::flush;
        }
        std::cout << std::endl;
    }
    acc = nullptr;
    // pybind11::dict obj = acc.operator pybind11::dict();
    std::cout << acc << std::endl;
    return 0;
}
