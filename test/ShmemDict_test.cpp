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

// TEST_F(ShmemDictTest, TypeIdAndLen)
// {
//     // Single Level
//     acc = vector<vector<float>>({{1}, {11}, {111}, {1111}, {11111}});
//     EXPECT_EQ(acc.len(), 5);
//     EXPECT_EQ(acc.typeId(), List);

//     // Multiple level
//     acc[4] = vector<vector<int>>({{2}, {22}, {222}, {2222}, {22222}});
//     acc.add({"t", "tt", "ttt", "tttt", "ttttt"});
//     acc[4][4] = {"s", "ss", "sss", "ssss", "sssss"};
//     acc[4][4][4] = vector<vector<double>>({{9}, {99}, {999}, {9999}, {99999}});
//     EXPECT_EQ(acc.len(), 6);
//     EXPECT_EQ(acc.typeId(), List);
//     EXPECT_EQ(acc[4].typeId(), List);
//     EXPECT_EQ(acc[5].typeId(), List);
//     EXPECT_EQ(acc[5][1].typeId(), Char);
//     EXPECT_EQ(acc[4][3].typeId(), Int);
//     EXPECT_EQ(acc[4][4].typeId(), List);
//     EXPECT_EQ(acc[4][4][3].typeId(), Char);
//     EXPECT_EQ(acc[4][4][4].typeId(), List);
//     EXPECT_EQ(acc[4][4][4][3].typeId(), Double);
//     EXPECT_EQ(acc[4][4][4][4].typeId(), Double);

//     std::cout << acc << std::endl;
// }

// TEST_F(ShmemDictTest, SetAndGetAndAddElement)
// {
//     // basic get and set
//     acc = vector<vector<float>>({{1}, {11}, {111}, {1111}, {11111}});
//     vector<vector<float>> result1(acc);
//     EXPECT_EQ(result1, vector<vector<float>>({{1}, {11}, {111}, {1111}, {11111}}));

//     // Getting a list of uniformed but conversable types is allowed
//     acc = vector<vector<float>>({{1}, {11}, {111}, {1111}, {11111}});
//     acc[0] = "s";
//     vector<vector<float>> result2(acc);
//     EXPECT_EQ(result2, vector<vector<float>>({{'s', '\0'}, {11}, {111}, {1111}, {11111}}));

//     // Getting a list of uniformed and non-conversable types is not allowed
//     acc = vector<vector<float>>({{1}, {11}, {111}, {1111}, {11111}});
//     acc.add(vector<vector<int>>({{2}, {2, 2}, {2}, {2}, {2}}));
//     vector<vector<float>> result3;
//     EXPECT_ANY_THROW(result3 = acc.get<vector<vector<float>>>());

//     // Deep assignment
//     acc = nullptr;
//     acc = vector<vector<float>>({{1}, {11}, {111}, {1111}, {11111}});
//     acc[4] = vector<vector<int>>({{2}, {22}, {222}, {2222}, {22222}});
//     acc.add({"t", "tt", "ttt", "tttt", "ttttt"});
//     acc[4][4] = {"s", "ss", "sss", "ssss", "sssss"};
//     acc[4][4][4] = vector<vector<double>>({{9}, {99}, {999}, {9999}, {99999}});

//     vector<vector<int>> result4;

//     result4 = acc[4][4][4].get<vector<vector<int>>>();
//     EXPECT_EQ(result4, vector<vector<int>>({{9}, {99}, {999}, {9999}, {99999}}));

//     // acc[4][4][4] is in consistent with acc[4][4][0-3], should throw an exception
//     // acc[4][4][0-3] is (P:char:N)
//     // acc[4][4][4] is (L:N*(P:double:1))
//     EXPECT_ANY_THROW(result4 = acc[4][4].get<vector<vector<int>>>());

//     acc[4][4][0] = vector<vector<double>>({{4}, {44}, {444}, {4444}, {44444}});
//     acc[4][4][1] = vector<vector<double>>({{5}, {55}, {555}, {5555}, {55555}});
//     acc[4][4][2] = vector<vector<double>>({{6}, {66}, {666}, {6666}, {66666}});
//     acc[4][4][3] = vector<std::string>({"A", "BB", "CCC", "DDDD", "EEEEE"});

//     // Now acc[4][4][0-3] is also (L:N*(P:any:N)) which is conversable with List[List[P:int:1]] (vector<vector<vector<int>>>)
//     vector<vector<vector<int>>> result5 = acc[4][4].get<vector<vector<vector<int>>>>();
//     EXPECT_EQ(result5, vector<vector<vector<int>>>({{{4}, {44}, {444}, {4444}, {44444}},
//                                                     {{5}, {55}, {555}, {5555}, {55555}},
//                                                     {{6}, {66}, {666}, {6666}, {66666}},
//                                                     {{'A', '\0'}, {'B', 'B', '\0'}, {'C', 'C', 'C', '\0'}, {'D', 'D', 'D', 'D', '\0'}, {'E', 'E', 'E', 'E', 'E', '\0'}},
//                                                     {{9}, {99}, {999}, {9999}, {99999}}}));

//     // Test the type check is a deep check(recursive)

//     // Assign all acc[4][0-3] to List[P:int:1]
//     acc[4][0] = vector<vector<int>>({{45}, {67}, {89}});
//     acc[4][1] = vector<vector<int>>({{46}, {68}, {90}});
//     acc[4][2] = vector<vector<int>>({{47}, {69}, {91}});
//     acc[4][3] = vector<vector<int>>({{48}, {70}, {92}});
//     // But acc[4][4] is List[List[P:int:1]]

//     // acc[4][0-4] are all list, but as acc[4][4] have a different type, it is not conversable
//     EXPECT_ANY_THROW(result5 = acc[4].get<vector<vector<vector<int>>>>());

//     acc[4][4] = vector<vector<double>>({{9}, {99}, {999}, {9999}, {99999}});
//     EXPECT_NO_THROW(result5 = acc[4].get<vector<vector<vector<int>>>>());

//     acc[4][4][4] = vector<vector<int>>({{9}, {99}, {999}, {9999}, {99999}});
//     EXPECT_ANY_THROW(result5 = acc[4].get<vector<vector<vector<int>>>>());

//     acc = nullptr;

//     // shmHeap.printShmHeap();
//     // std::cout << acc[4] << std::endl;
// }

// TEST_F(ShmemDictTest, Add)
// {
//     acc = vector<vector<float>>();
//     acc.add(vector<float>({1}));
//     acc.add(vector<float>({11}));
//     acc.add(vector<float>({111}));
//     acc.add(vector<float>({1111}));
//     acc.add(vector<float>({11111}));
//     acc[4] = vector<vector<int>>();
//     acc[4].add(vector<int>({2}));
//     acc[4].add(vector<int>({22}));
//     acc[4].add(vector<int>({222}));
//     acc[4].add(vector<int>({2222}));
//     acc[4].add(vector<int>({22222}));
//     acc.add("tt");
//     acc.add("ttt");
//     acc.add("tttt");
//     acc.add("ttttt");
//     acc[4][3] = "ssss";
//     acc[4][4] = "sssss";

//     EXPECT_THROW(acc[4][5].get<std::string>(), std::runtime_error);

//     EXPECT_EQ(acc.len(), 9);
//     EXPECT_EQ(acc[4].len(), 5);

//     // std::cout << acc << std::endl;
// }

// TEST_F(ShmemDictTest, Del)
// {
//     acc = vector<vector<float>>({{1}, {11}, {111}, {1111}, {11111}});
//     acc[4] = vector<vector<int>>({{2}, {22}, {222}, {2222}, {22222}});
//     acc.add({"t", "tt", "ttt", "ttft"});
//     acc[4][4] = {"s", "ss", "sss", "ssss", "sssss"};
//     shmHeap.printShmHeap();

//     acc.del(4);
//     EXPECT_EQ(acc[4], vector<string>({"t", "tt", "ttt", "ttft"}));
//     EXPECT_THROW(acc[4][4].get<std::string>(), std::runtime_error);

//     EXPECT_EQ(acc[4][3], "ttft");

//     // Actually this part should be in primitive test ...
//     // TODO: Move this to primitive test
//     acc[4][3].del(2);
//     EXPECT_EQ(acc[4][3], "ttt");
//     EXPECT_EQ(acc[4][3].len(), 3 + 1);

//     std::cout << acc[4][3] << std::endl;
// }

// TEST_F(ShmemDictTest, Contains)
// {
//     acc = vector<vector<float>>({{1}, {11}, {111}, {1111}, {11111}});
//     acc[4] = vector<vector<int>>({{2}, {22}, {222}, {2222}, {22222}});
//     acc.add({"t", "tt", "ttt", "tttt", "ttttt"});
//     acc[4][4] = {"s", "ss", "sss", "ssss", "sssss"};
//     EXPECT_TRUE(acc[4][4].contains("ssss"));

//     acc[4][4][0] = vector<vector<double>>({{4}, {44}, {444}, {4444}, {44444}});
//     acc[4][4][1] = vector<vector<double>>({{5}, {55}, {555}, {5555}, {55555}});
//     acc[4][4][2] = vector<vector<double>>({{6}, {66}, {666}, {6666}, {66666}});
//     acc[4][4][3] = vector<std::string>({"A", "BB", "CCC", "DDDD", "EEEEE"});
//     acc[4][4][4] = vector<vector<double>>({{9}, {99}, {999}, {9999}, {99999}});
//     EXPECT_FALSE(acc[4][4].contains("ssss"));

//     EXPECT_TRUE(acc[4][4].contains(vector<std::string>({"A", "BB", "CCC", "DDDD", "EEEEE"})));
//     EXPECT_TRUE(acc[4][4].contains(vector<vector<float>>({{6}, {66}, {666}, {6666}, {66666}})));
//     // std::cout << acc[4][4] << std::endl;
// }