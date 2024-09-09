import sys
import os
import tempfile
import subprocess
import shutil

import pytest
from TypedShmem import ShmemHeap


@pytest.fixture
def setup():
    shmHeap = ShmemHeap("test_shm_heap", 80, 1024)
    yield shmHeap
    shmHeap.unlink()

def testConstructor(setup):
    shmHeap = setup

    assert shmHeap.getName() == "test_shm_heap"
    assert shmHeap.getCapacity() == 4096 + 80
    assert not shmHeap.isConnected()
    assert not shmHeap.ownsSharedMemory()
    assert shmHeap.getVersion() == -1

    another = ShmemHeap("another_shm_heap", 1, 4097)
    assert another.getName() == "another_shm_heap"
    assert another.getCapacity() == 2 * 4096 + 4 * 8


def testCreate(setup):
    shmHeap = setup

    shmHeap.create()
    assert shmHeap.isConnected()
    assert shmHeap.ownsSharedMemory()
    assert shmHeap.staticCapacity() == 80
    assert shmHeap.heapCapacity() == 4096


def testConnect(setup):
    shmHeap = setup

    shmHeap.setHCap(4097)
    shmHeap.setSCap(90)
    shmHeap.create()

    another = ShmemHeap("test_shm_heap", 1, 1000000)
    another.connect()
    assert another.staticCapacity() == 96
    assert another.heapCapacity() == 4096 * 2


def testResizeCapacityCorrectness(setup):
    shmHeap = setup

    shmHeap.create()
    shmHeap.resize(4097)

    another = ShmemHeap("test_shm_heap", 1, 1000000)
    another.connect()
    assert another.staticCapacity() == 80
    assert another.heapCapacity() == 4096 * 2

    shmHeap.resize(90, 4096 * 2 + 1)
    assert another.staticCapacity() == 96
    assert another.heapCapacity() == 4096 * 3


def testResizeContentCorrectness(setup):
    shmHeap = setup

    shmHeap.create()
    ptr1 = shmHeap.shmalloc(0x100)

    shmHeap.resize(4097)
    another = ShmemHeap("test_shm_heap", 1, 1000000)
    another.connect()

    ptr2 = another.shmalloc(7928 - 8 - 200 - 8)
    ptr3 = another.shmalloc(200)
    shmHeap.shfree(ptr2)
    assert another.briefLayoutStr() == "256A, 7712E, 200A"

    shmHeap.resize(90, 4096 * 2 + 1)
    assert another.briefLayoutStr() == "256A, 7712E, 200A, 4088E"

    shmHeap.shfree(ptr3)
    ptr4 = another.shmalloc(12016 - 32)
    assert another.briefLayoutStr() == "256A, 11984A, 24E"

    another.resize(4096 * 3 + 1)
    assert shmHeap.briefLayoutStr() == "256A, 11984A, 4120E"
    assert another.staticCapacity() == 96
    assert another.heapCapacity() == 4096 * 4


def testFullMallocAndFree(setup):
    shmHeap = setup

    shmHeap.setHCap(1)
    shmHeap.create()
    shm1 = ShmemHeap("test_shm_heap", 1, 1000000)
    shm2 = ShmemHeap("test_shm_heap", 4, 7)
    shm3 = ShmemHeap("test_shm_heap", 9, 999)
    shm1.connect()
    shm2.connect()
    shm3.connect()

    expectedLayout = ""
    for i in range(4096 // 32):
        if i % 4 == 0:
            shmHeap.shmalloc(1)
        elif i % 4 == 1:
            shm1.shmalloc(1)
        elif i % 4 == 2:
            shm2.shmalloc(1)
        elif i % 4 == 3:
            shm3.shmalloc(1)
        expectedLayout += "24A"
        if i < 4096 // 32 - 1:
            expectedLayout += ", "

    assert shmHeap.briefLayoutStr() == expectedLayout

    for i in range(1, 4096 // 32 - 1):
        if i % 4 == 0:
            assert shmHeap.shfree(i * 32 + 8) == 0
        elif i % 4 == 1:
            assert shm1.shfree(i * 32 + 8) == 0
        elif i % 4 == 2:
            assert shm2.shfree(i * 32 + 8) == 0
        elif i % 4 == 3:
            assert shm3.shfree(i * 32 + 8) == 0

    assert shmHeap.briefLayoutStr() == "24A, 4024E, 24A"

    shmHeap.shfree(8)
    assert shmHeap.briefLayoutStr() == "4056E, 24A"

    shm1.shfree(4096 - 32 + 8)
    assert shm1.briefLayoutStr() == "4088E"


def testBasicRealloc(setup):
    shmHeap = setup

    shmHeap.create()
    ptr1 = shmHeap.shmalloc(0x100)
    assert shmHeap.briefLayoutStr() == "256A, 3824E"

    ptr2 = shmHeap.shrealloc(ptr1, 0x1FA)
    assert shmHeap.briefLayoutStr() == "512A, 3568E"

if __name__ == "__main__":
    pytest.main(["-v", "pytest/ShmemHeap_test.py"])