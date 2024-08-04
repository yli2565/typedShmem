#include <gtest/gtest.h>
#include <cstring>

#include "ShmemBase.h"

// Test Fixture for ShmemBase
class ShmemBaseTest : public ::testing::Test
{
protected:
    // Setup code (called before each test)
    void SetUp() override
    {
        // Initialize necessary objects/resources
        shmObject = new ShmemBase("test_shm", 1024);
        // Set spdlog sink to a file
    }

    // Teardown code (called after each test)
    void TearDown() override
    {
        // Cleanup objects/resources
        delete shmObject;
    }

    ShmemBase *shmObject;
};

// Test constructor with name and capacity
TEST_F(ShmemBaseTest, ConstructorWithNameAndCapacity)
{
    EXPECT_EQ(shmObject->getName(), "test_shm");
    EXPECT_EQ(shmObject->getCapacity(), 1024);
    EXPECT_FALSE(shmObject->isConnected());
    EXPECT_FALSE(shmObject->ownsSharedMemory());
    EXPECT_EQ(shmObject->getUsedSize(), 0);
    EXPECT_EQ(shmObject->getVersion(), -1);
}

// Test constructor with capacity only
TEST_F(ShmemBaseTest, ConstructorWithCapacity)
{
    ShmemBase unnamedShm(2048);
    EXPECT_EQ(unnamedShm.getName(), "");
    EXPECT_EQ(unnamedShm.getCapacity(), 2048);
}

// Test creating shared memory
TEST_F(ShmemBaseTest, Create)
{
    shmObject->create();
    EXPECT_TRUE(shmObject->isConnected());
    EXPECT_TRUE(shmObject->ownsSharedMemory());
    EXPECT_EQ(shmObject->getUsedSize(), 0);
    EXPECT_EQ(shmObject->getVersion(), 0);
}

// Test basic resizing
TEST_F(ShmemBaseTest, Resize)
{
    shmObject->create();
    size_t newCapacity = 2048;
    shmObject->resize(newCapacity);
    EXPECT_EQ(shmObject->getCapacity(), newCapacity);
}

// Test if resized shared memory keep the original data
TEST_F(ShmemBaseTest, ResizeKeepOriginalData)
{
    Byte data[4] = {1, 2, 3, 4};
    shmObject->create();

    shmObject->setBytes(900, data, 4);
    shmObject->printMemoryView();

    // First resize
    shmObject->resize(2048);
    Byte readData[4];
    shmObject->getBytes(900, readData, 4);
    EXPECT_EQ(memcmp(data, readData, 4), 0);
    EXPECT_EQ(shmObject->getUsedSize(), 904);
    shmObject->printMemoryView();

    shmObject->setBytes(1500, data, 4);
    shmObject->printMemoryView();

    // Second resize
    shmObject->resize(4096);
    shmObject->getBytes(1500, readData, 4);
    EXPECT_EQ(memcmp(data, readData, 4), 0);
    EXPECT_EQ(shmObject->getUsedSize(), 1504);
    shmObject->printMemoryView();
}

// Test reconnect after resize
TEST_F(ShmemBaseTest, ResizeAndReconnect)
{
    Byte data[4] = {1, 2, 3, 4};

    shmObject->create();
    ShmemBase anotherShm("test_shm", 1024);
    anotherShm.connect();

    shmObject->setBytes(900, data, 4);

    // First resize
    shmObject->resize(2048);

    unsigned char readData = anotherShm.get<unsigned char>(903);
    EXPECT_EQ(readData, 4);
}

TEST_F(ShmemBaseTest, Connect)
{
    shmObject->create();
    ShmemBase anotherShm("test_shm", 1024);
    anotherShm.connect();
    EXPECT_TRUE(anotherShm.isConnected());
    EXPECT_FALSE(anotherShm.ownsSharedMemory());
}

// Test setting and getting bytes
TEST_F(ShmemBaseTest, SetAndGetBytes)
{
    shmObject->create();
    Byte data[4] = {1, 2, 3, 4};
    shmObject->setBytes(0, data, 4);
    Byte readData[4];
    shmObject->getBytes(0, readData, 4);
    EXPECT_EQ(memcmp(data, readData, 4), 0);

    int combined = shmObject->get<unsigned int>(0);
    // Since it's little-endian, we need to swap bytes
    EXPECT_EQ(combined, 0x04030201);

    // Consider the array start at 896 in the shared memory, and the value is assigned to index 1
    // which is at byte index 900
    shmObject->set<unsigned int>(1, 0x04030201, 896);
    shmObject->getBytes(900, readData, 4);
    EXPECT_EQ(memcmp(data, readData, 4), 0);

    shmObject->printMemoryView();
}

// Test clearing shared memory
TEST_F(ShmemBaseTest, Clear)
{
    shmObject->create();
    Byte data[4] = {1, 2, 3, 4};
    shmObject->setBytes(0, data, 4);
    shmObject->clear();
    EXPECT_EQ(shmObject->getUsedSize(), 0);
}

// Test closing shared memory
TEST_F(ShmemBaseTest, Close)
{
    shmObject->create();
    shmObject->close();
    EXPECT_FALSE(shmObject->isConnected());
    EXPECT_EQ(shmObject->getVersion(), -1);
}

// Test printing memory view
TEST_F(ShmemBaseTest, PrintMemoryView)
{
    shmObject->create();
    Byte data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    shmObject->setBytes(0, data, 4);
    shmObject->printMemoryView(4);
}
