#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstring>

#include "ShmemPrimitive.h"
#include "ShmemAccessor.h"

class ShmemPrimitiveTest : public ::testing::Test
{
protected:
    // Setup code (called before each test)
    ShmemPrimitiveTest() : shmHeap("test_shm_heap", 80, 1024), acc(&shmHeap){};

    void SetUp() override
    {
        shmHeap.getLogger()->set_level(spdlog::level::info);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        shmHeap.getLogger()->sinks().clear();
        shmHeap.getLogger()->sinks().push_back(console_sink);
        // Initialize necessary objects/resources
        shmHeap.create();

        // Set spdlog sink to a file
    }

    // Teardown code (called after each test)
    void TearDown() override
    {
        // Cleanup objects/resources
        // spdlog::drop_all();
    }

    ShmemHeap shmHeap;
    ShmemAccessor acc;
};

// Test constructor with name and capacity
TEST_F(ShmemPrimitiveTest, BasicAssignmentAndMemoryUsage)
{
    size_t expectedMemSize = 0;

    acc = 1;
    expectedMemSize = 24; // min payload is 24
    EXPECT_EQ(shmHeap.briefLayout(), std::vector<size_t>({expectedMemSize, 4096 - expectedMemSize - unitSize * 2}));
    EXPECT_EQ(acc.toString(20), "[P:int:1] 1");

    acc = std::vector<float>(10, 1);
    expectedMemSize = 8 + 40; // ShmemPrimitive header 8 bytes + 4*10 + 0 padding
    EXPECT_EQ(shmHeap.briefLayout(), std::vector<size_t>({expectedMemSize, 4096 - expectedMemSize - unitSize * 2}));

    acc = std::vector<int>(10, 1);
    expectedMemSize = 8 + 40; // ShmemPrimitive header 8 bytes + 4*10 + 0 padding
    EXPECT_EQ(shmHeap.briefLayout(), std::vector<size_t>({expectedMemSize, 4096 - expectedMemSize - unitSize * 2}));
    EXPECT_EQ(acc.toString(20), "[P:int:10] 1 1 1 1 1 1 1 1 1 1");

    acc = std::vector<size_t>(10, 1);
    expectedMemSize = 8 + 8 * 10; // ShmemPrimitive header 8 bytes + 8*10 + 0 padding
    EXPECT_EQ(shmHeap.briefLayout(), std::vector<size_t>({expectedMemSize, 4096 - expectedMemSize - unitSize * 2}));
    EXPECT_EQ(acc.toString(20), "[P:unsigned long:10] 1 1 1 1 1 1 1 1 1 1");
    EXPECT_EQ(acc.toString(4), "[P:unsigned long:10] 1 1 1 1 ...");

    // shmHeap.printShmHeap();
    // std::cout << acc.toString(10) << std::endl;
}

TEST_F(ShmemPrimitiveTest, SetAndGetElement)
{
    acc = std::vector<float>(10, 1);
    acc[2] = 2.f;
    std::cout << acc.toString(10) << std::endl;
    std::cout << acc[2] << std::endl;
}