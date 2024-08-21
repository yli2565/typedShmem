#include <iostream>
#include "ShmemObj.h"
#include "ShmemAccessor.h"

int main(int argc, char **argv)
{
    // spdlog::set_default_logger(spdlog::basic_logger_mt("test", "test.log"));
    // spdlog::set_level(spdlog::level::debug);
    // printf("Running main() from %s\n", __FILE__);

    // auto SH = ShmemHeap("test", 0x1000, 0x1000);
    // SH.create();
    // auto ACC = ShmemAccessor(&SH, {});
    // ACC = 12;
    // ACC = "string";

    // float t = ACC;
    // SH.printShmHeap();

    // ACC = std::vector<int>(10, 1);
    // if (ACC == std::vector<int>(10, 1))
    // {
    //     std::cout << "std::vector<int> is equal to std::vector<int>(10, 1)" << std::endl;
    // }
    std::cout << sizeof(ShmemList) << std::endl;
    return 0;
}
