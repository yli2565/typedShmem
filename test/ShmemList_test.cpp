#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstring>

#include "ShmemList.h"
#include "ShmemAccessor.h"

class ShmemListTest : public ::testing::Test
{
protected:
    // Setup code (called before each test)
    ShmemListTest() : shmHeap("test_shm_list", 80, 1024), acc(&shmHeap){};

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

// Core tests

TEST_F(ShmemListTest, BasicAssignmentAndMemoryUsage)
{
    std::vector<std::vector<int>> v(10, std::vector<int>(10, 1));
    acc = v;
    size_t eachVecInt10Size = 8 + 4 * 10;
    EXPECT_EQ(shmHeap.briefLayout(), std::vector<size_t>({24, 8 * 10, eachVecInt10Size, eachVecInt10Size, eachVecInt10Size, eachVecInt10Size, eachVecInt10Size, eachVecInt10Size, eachVecInt10Size, eachVecInt10Size, eachVecInt10Size, eachVecInt10Size, 4096 - 13 * 8 - 24 - 8 * 10 - eachVecInt10Size * 10}));

    v = {{1}, {1, 2}, {1, 2, 3}, {1, 2, 3, 4}, {1, 2, 3, 4, 5}, {1, 2, 3, 4, 5, 6}, {1, 2, 3, 4, 5, 6, 7}, {1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
    acc = v;
    EXPECT_EQ(shmHeap.briefLayout(), std::vector<size_t>({24, 8 * 10, 24, 24, 24, 24, 32, 32, 40, 40, 48, 48, 3552}));

    acc[2]={{1, 2, 3, 4, 5, 6, 7, 8, 9, 10},{11}};
    std::cout << acc << std::endl;

}

// TEST_F(ShmemListTest, TypeIdAndLen)
// {
//     // Single Primitive
//     acc = double(7);
//     EXPECT_EQ(acc.len(), 1);
//     EXPECT_EQ(acc.typeId(), Double);

//     // Vector
//     acc = std::vector<float>(10, 1);
//     EXPECT_EQ(acc.len(), 10);
//     EXPECT_EQ(acc.typeId(), Float);

//     // String
//     acc = "string";
//     EXPECT_EQ(acc.len(), 6 + 1);
//     EXPECT_EQ(acc.typeId(), Char);
// }

// TEST_F(ShmemListTest, SetAndGetElement)
// {
//     // Single Primitive
//     acc = double(7);
//     EXPECT_EQ(acc, 7);
//     EXPECT_EQ(acc, std::vector<int>({7}));

//     // Vector
//     acc = std::vector<float>(10, 1);

//     int a;
//     // Assign a multiple element array to a single primitive is not allowed
//     EXPECT_ANY_THROW(a = acc);

//     acc[0] = 's';
//     EXPECT_EQ(acc[0], 's');
//     acc[1] = double(11);

//     EXPECT_EQ(acc[1], 11);
//     acc[2] = 12;
//     EXPECT_EQ(acc[2], 12);
//     EXPECT_EQ(acc, std::vector<int>({'s', 11, 12, 1, 1, 1, 1, 1, 1, 1}));

//     EXPECT_ANY_THROW(acc[10] = "s");

//     EXPECT_ANY_THROW(acc[10] = std::vector<int>(10, 1));

//     // String
//     acc = "string";
//     shmHeap.printShmHeap();
//     EXPECT_EQ(acc, "string");
//     EXPECT_EQ(acc.len(), 6 + 1);
//     EXPECT_EQ(acc.typeId(), Char);
// }

// TEST_F(ShmemListTest, Contains)
// {
//     acc = std::vector<float>(10, 1);
//     acc[0] = 's';
//     acc[9] = 'a';

//     EXPECT_EQ(acc.contains('s'), true);
//     EXPECT_EQ(acc.contains('a'), true);
//     EXPECT_EQ(acc.contains('b'), false);
// }