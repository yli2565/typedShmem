#include <iostream>
#include <sstream>
#include <gtest/gtest.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "ShmemHeap.h"
// int main(int argc, char **argv)
// {
//     spdlog::set_default_logger(spdlog::basic_logger_mt("test", "test.log"));
//     spdlog::set_level(spdlog::level::debug);
//     printf("Running main() from %s\n", __FILE__);
//     ::testing::InitGoogleTest(&argc, argv);

//     return RUN_ALL_TESTS();
// }
int main(int argc, char **argv)
{
    spdlog::set_default_logger(spdlog::basic_logger_mt("test", "test.log"));
    spdlog::set_level(spdlog::level::debug);
    printf("Running main() from %s\n", __FILE__);

    auto SH = ShmemHeap("test", 0x1000, 0x1000);
    SH.getLogger()->set_level(spdlog::level::off);
    SH.create();
    SH.shmalloc(0x100);
    SH.printShmHeap();
    return 0;
}
