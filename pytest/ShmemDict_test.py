import pytest
from TypedShmem import ShmemHeap, ShmemAccessor, SDict


@pytest.fixture
def shmemDictTest():
    """Fixture to initialize and cleanup ShmemHeap and ShmemAccessor for each test."""
    shmHeap = ShmemHeap("test_shm_dict", 80, 1024)
    acc = ShmemAccessor(shmHeap)
    shmHeap.setLogLevel(0)
    shmHeap.create()
    yield shmHeap, acc
    shmHeap.close()


def testCreateEmptyDict(shmemDictTest):
    shmHeap, acc = shmemDictTest
    acc.set(SDict())
    assert acc.len() == 0
    assert acc.typeStr() == "dict"
    assert acc.toString() == "(D:0){\n}"
    print(acc)


def testBasicAssignmentAndMemoryUsage(shmemDictTest):
    shmHeap, acc = shmemDictTest
    m1 = {"9": 2}
    acc.set(m1)

    # Expected layout after assignment
    assert shmHeap.briefLayout() == [24, 56, 24, 24, 56, 24, 3832]

    m2 = {str(100 * "A"): 2}
    acc.set(m2)
    assert shmHeap.briefLayout() == [24, 56, 24, 24, 56, 112, 3744]

    acc.set(m1)
    acc["new"].set(5)
    assert shmHeap.briefLayout() == [24, 56, 24, 24, 56, 24, 24, 56, 24, 3704]

    acc["new"].set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
    assert shmHeap.briefLayout() == [24, 56, 24, 24, 56, 24, 24, 56, 24, 72, 3624]

    with pytest.raises(Exception):
        del acc[9]

    del acc["9"]
    del acc["new"]

    assert acc.toString() == "(D:0){\n}"

    acc[2].set(m1)
    del acc[2]["9"]
    print(acc)


def testQuickAssign(shmemDictTest):
    _, acc = shmemDictTest

    m1 = {1: 11, 2: 22, 3: 33, 4: 44}
    acc.set(m1)
    assert acc == m1

    m2 = {"A": 1, "BB": 11, "CCC": 111, "DDDD": 1111, "EEEEE": 11111}
    acc.set(m2)
    assert acc == m2

    m3 = {1: 11.0, 2: 22.0, 3: 33.0, 4: 44.0}
    acc.set(m3)
    assert acc == m3

    m4 = {"A": 1.0, "BB": 11.0, "CCC": 111.0, "DDDD": 1111.0, "EEEEE": 11111.0}
    acc.set(m4)
    assert acc == m4

    m5 = {"A": "1", "BB": "11", "CCC": "111", "DDDD": "1111", "EEEEE": "11111"}
    acc.set(m5)
    assert acc == m5

    m6 = {"A": 1, "BB": 11, "CCC": 111, "DDDD": 1111, "EEEEE": 11111}
    acc.set(m6)
    assert acc == m6


def testConvertToPythonObject(shmemDictTest):
    _, acc = shmemDictTest
    acc.set({"A": 1, "BB": 11, "CCC": 111, "DDDD": 1111, "EEEEE": 11111})
    acc["A"].set([[2, 3, 4], [5, 6, 7], [8, 9, 10], [11, 12, 13], [14, 15, 16]])
    obj = acc.fetch()

    acc.set(None)

    obj = acc.fetch()
    print(obj)


if __name__ == "__main__":
    pytest.main(["-v", "pytest/ShmemDict_test.py"])
