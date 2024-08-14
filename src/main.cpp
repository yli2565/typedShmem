#include <iostream>
#include "ShmemObj.h"
int main(int argc, char **argv)
{
    // spdlog::set_default_logger(spdlog::basic_logger_mt("test", "test.log"));
    spdlog::set_level(spdlog::level::debug);
    printf("Running main() from %s\n", __FILE__);

    auto SH = ShmemHeap("test", 0x1000, 0x1000);
    auto ACC = ShmemAccessor(&SH, {});
    ACC = "123123123";

    std::cout << ACC.toString() << std::endl;
    return 0;
}
