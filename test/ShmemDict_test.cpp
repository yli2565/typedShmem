#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstring>

#include "ShmemList.h"
#include "ShmemAccessor.h"

using namespace std;
class ShmemDictTest : public ::testing::Test
{
protected:
    // Setup code (called before each test)
    ShmemDictTest() : shmHeap("test_shm_dict", 80, 1024), acc(&shmHeap){};

    void SetUp() override
    {
        shmHeap.getLogger()->set_level(spdlog::level::info);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        shmHeap.getLogger()->sinks().clear();
        shmHeap.getLogger()->sinks().push_back(console_sink);
        // Initialize necessary objects/resources
        shmHeap.create();
        Py_Initialize();

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
TEST_F(ShmemDictTest, BasicAssignmentAndMemoryUsage)
{
    std::map<std::string, int> m1({{"9", 2}});

    acc = m1;

    // 24     , 56 , 24     , 24,    , 56      , 24      , 3832
    // DictObj, NIL, NIL_key, data(2), DictNode, key("9"), free_block
    EXPECT_EQ(shmHeap.briefLayout(), vector<size_t>({24, 56, 24, 24, 56, 24, 3832}));

    std::map<std::string, int> m2({{std::string(100, 'A'), 2}});
    acc = m2;

    // 24     , 56 , 24     , 24,    , 56      , 112, 3744
    // DictObj, NIL, NIL_key, data(2), DictNode, key, free_block
    EXPECT_EQ(shmHeap.briefLayout(), vector<size_t>({24, 56, 24, 24, 56, 112, 3744}));

    acc = m1;
    acc["new"] = 5;

    // 24     , 56 , 24     , 24,    , 56      , 24,     , 24     , 56      , 24        , 3704
    // DictObj, NIL, NIL_key, data(2), DictNode, key("9"), data(5), DictNode, key("new"), free_block
    EXPECT_EQ(shmHeap.briefLayout(), vector<size_t>({24, 56, 24, 24, 56, 24, 24, 56, 24, 3704}));

    acc["new"] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    // 24     , 56 , 24     , 24,    , 56      , 24,     , 24(freed), 56      , 24        , 72            , 3704
    // DictObj, NIL, NIL_key, data(2), DictNode, key("9"), data(5)  , DictNode, key("new"), new data array, free_block
    EXPECT_EQ(shmHeap.briefLayout(), vector<size_t>({24, 56, 24, 24, 56, 24, 24, 56, 24, 72, 3624}));

    EXPECT_ANY_THROW(acc.del(9));
    acc.del("9");
    acc.del("new");
    EXPECT_EQ(acc.toString(), "(D:0){\n}");

    // More test required

    acc[2].set(m1);
    acc[2].del("9");

    std::cout << acc << std::endl;
}

TEST_F(ShmemDictTest, QuickAssign)
{
    map<int, int> m1 = {{1, 11}, {2, 22}, {3, 33}, {4, 44}};
    acc = {{1, 11}, {2, 22}, {3, 33}, {4, 44}};
    EXPECT_EQ(acc, m1);

    map<string, int> m2 = {{"A", 1}, {"BB", 11}, {"CCC", 111}, {"DDDD", 1111}, {"EEEEE", 11111}};
    acc = {{"A", 1}, {"BB", 11}, {"CCC", 111}, {"DDDD", 1111}, {"EEEEE", 11111}};
    EXPECT_EQ(acc, m2);

    map<int, float> m3 = {{1, 11.0}, {2, 22.0}, {3, 33.0}, {4, 44.0}};
    acc = {{1, 11.0}, {2, 22.0}, {3, 33.0}, {4, 44.0}};
    EXPECT_EQ(acc, m3);

    map<string, float> m4 = {{"A", 1.0}, {"BB", 11.0}, {"CCC", 111.0}, {"DDDD", 1111.0}, {"EEEEE", 11111.0}};
    acc = {{"A", 1.0}, {"BB", 11.0}, {"CCC", 111.0}, {"DDDD", 1111.0}, {"EEEEE", 11111.0}};
    EXPECT_EQ(acc, m4);

    map<string, string> m5 = {{"A", "1"}, {"BB", "11"}, {"CCC", "111"}, {"DDDD", "1111"}, {"EEEEE", "11111"}};
    acc = {{"A", "1"}, {"BB", "11"}, {"CCC", "111"}, {"DDDD", "1111"}, {"EEEEE", "11111"}};
    EXPECT_EQ(acc, m5);

    map<string, int> m6 = {{"A", 1}, {"BB", 11}, {"CCC", 111}, {"DDDD", 1111}, {"EEEEE", 11111}};
    acc = {{"A", 1}, {"BB", 11}, {"CCC", 111}, {"DDDD", 1111}, {"EEEEE", 11111}};
    EXPECT_EQ(acc, m6);
}

TEST_F(ShmemDictTest, ConvertToPythonObject)
{
    acc = {{"A", 1}, {"BB", 11}, {"CCC", 111}, {"DDDD", 1111}, {"EEEEE", 11111}};
    acc["A"]=vector<vector<int>>({{2,3,4},{5,6,7},{8,9,10},{11,12,13},{14,15,16}});
    acc=nullptr;
    pybind11::object obj = acc.operator pybind11::object();
    std::cout << obj << std::endl;
}