#include <gtest/gtest.h>
#include <cstring>

#include "ShmemHeap.h"

// Test Fixture for ShmemBase
class ShmemHeapTest : public ::testing::Test
{
protected:
    // Setup code (called before each test)
    void SetUp() override
    {
        // Initialize necessary objects/resources
        shmHeap = new ShmemHeap("test_shm_heap", 80, 1024);
        // Set spdlog sink to a file
    }

    // Teardown code (called after each test)
    void TearDown() override
    {
        // Cleanup objects/resources
        delete shmHeap;
    }

    ShmemHeap *shmHeap;
};

// Test constructor with name and capacity
TEST_F(ShmemHeapTest, Constructor)
{
    EXPECT_EQ(shmHeap->getName(), "test_shm_heap");
    // EXPECT_EQ(shmHeap->staticCapacity(), 80);
    // EXPECT_EQ(shmHeap->heapCapacity(), 4096);
    EXPECT_EQ(shmHeap->getCapacity(), 4096 + 80);
    EXPECT_FALSE(shmHeap->isConnected());
    EXPECT_FALSE(shmHeap->ownsSharedMemory());
    EXPECT_EQ(shmHeap->getVersion(), -1);

    ShmemHeap another = ShmemHeap("another_shm_heap", 1, 4097);
    EXPECT_EQ(another.getName(), "another_shm_heap");
    EXPECT_EQ(another.getCapacity(), 2 * 4096 + 4 * 8);
}

TEST_F(ShmemHeapTest, Create)
{
    shmHeap->create();
    EXPECT_TRUE(shmHeap->isConnected());
    EXPECT_TRUE(shmHeap->ownsSharedMemory());
    EXPECT_EQ(shmHeap->staticCapacity(), 80);
    EXPECT_EQ(shmHeap->heapCapacity(), 4096);
}

TEST_F(ShmemHeapTest, Connect)
{
    shmHeap->setHCap(4097);
    shmHeap->setSCap(90);
    shmHeap->create();

    ShmemHeap another = ShmemHeap("test_shm_heap", 1, 1000000);
    another.connect();
    // After the connect, the capacity should based on the shared memory
    EXPECT_EQ(another.staticCapacity(), 96);
    EXPECT_EQ(another.heapCapacity(), 4096 * 2);
}

TEST_F(ShmemHeapTest, ResizeCapacityCorrectness)
{
    shmHeap->create();

    shmHeap->resize(4097);

    ShmemHeap another = ShmemHeap("test_shm_heap", 1, 1000000);
    another.connect();

    EXPECT_EQ(another.staticCapacity(), 80);
    EXPECT_EQ(another.heapCapacity(), 4096 * 2);

    shmHeap->resize(90, 4096 * 2 + 1);

    EXPECT_EQ(another.staticCapacity(), 96);
    EXPECT_EQ(another.heapCapacity(), 4096 * 3);
}

TEST_F(ShmemHeapTest, ResizeContentCorrectness)
{
    shmHeap->create();

    size_t ptr1 = shmHeap->shmalloc(0x100);

    shmHeap->resize(4097);

    ShmemHeap another = ShmemHeap("test_shm_heap", 1, 1000000);
    another.connect();

    size_t ptr2 = another.shmalloc(7928 - 8 - 200 - 8);
    size_t ptr3 = another.shmalloc(200);
    shmHeap->shfree(ptr2);
    EXPECT_EQ(another.briefLayoutStr(), "256A, 7712E, 200A");
    // Create an allocated block at the end of the heap
    shmHeap->resize(90, 4096 * 2 + 1);
    EXPECT_EQ(another.briefLayoutStr(), "256A, 7712E, 200A, 4088E");

    shmHeap->shfree(ptr3);
    size_t ptr4 = another.shmalloc(12016 - 32);
    // Create a small free block at the end of the heap

    EXPECT_EQ(another.briefLayoutStr(), "256A, 11984A, 24E");

    another.resize(4096 * 3 + 1);

    // 4120 = 24 + 4096
    EXPECT_EQ(shmHeap->briefLayoutStr(), "256A, 11984A, 4120E");

    EXPECT_EQ(another.staticCapacity(), 96);
    EXPECT_EQ(another.heapCapacity(), 4096 * 4);
}

TEST_F(ShmemHeapTest, FullMallocAndFree)
{
    shmHeap->setHCap(1);
    shmHeap->create();
    // Use multiple other object to modify the mem
    ShmemHeap shm1 = ShmemHeap("test_shm_heap", 1, 1000000);
    ShmemHeap shm2 = ShmemHeap("test_shm_heap", 4, 7);
    ShmemHeap shm3 = ShmemHeap("test_shm_heap", 9, 999);
    shm1.connect();
    shm2.connect();
    shm3.connect();

    std::string expectedLayout = "";
    for (int i = 0; i < 4096 / 32; i++)
    {
        if (i % 4 == 0)
        {
            shmHeap->shmalloc(1);
        }
        if (i % 4 == 1)
        {
            shm1.shmalloc(1);
        }
        if (i % 4 == 2)
        {
            shm2.shmalloc(1);
        }
        if (i % 4 == 3)
        {
            shm3.shmalloc(1);
        }
        expectedLayout += "24A";
        if (i < 4096 / 32 - 1)
            expectedLayout += ", ";
    }

    EXPECT_EQ(shmHeap->briefLayoutStr(), expectedLayout);

    for (int i = 1; i < 4096 / 32 - 1; i++)
    {
        if (i % 4 == 0)
        {
            shmHeap->shfree(i * 32 + 8);
        }
        if (i % 4 == 1)
        {
            shm1.shfree(i * 32 + 8);
        }
        if (i % 4 == 2)
        {
            shm2.shfree(i * 32 + 8);
        }
        if (i % 4 == 3)
        {
            shm3.shfree(i * 32 + 8);
        }
    }

    EXPECT_EQ(shmHeap->briefLayoutStr(), "24A, 4024E, 24A");

    shmHeap->shfree(8);
    EXPECT_EQ(shmHeap->briefLayoutStr(), "4056E, 24A");

    shm1.shfree(4096 - 32 + 8);
    EXPECT_EQ(shm1.briefLayoutStr(), "4088E");
}

TEST_F(ShmemHeapTest, BasicRealloc)
{
    shmHeap->create();

    size_t ptr1 = shmHeap->shmalloc(0x100);
    EXPECT_EQ(shmHeap->briefLayoutStr(), "256A, 3824E");

    size_t ptr2 = shmHeap->shrealloc(ptr1, 0x1FA);
    EXPECT_EQ(shmHeap->briefLayoutStr(), "512A, 3568E");
}