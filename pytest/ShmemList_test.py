import pytest
from TypedShmem import ShmemHeap, ShmemAccessor, SList


@pytest.fixture
def shmemListTest():
    """Fixture to initialize and cleanup ShmemHeap and ShmemAccessor for each test."""
    shmHeap = ShmemHeap("test_shm_list", 80, 1024)
    acc = ShmemAccessor(shmHeap)
    shmHeap.setLogLevel(4)  # Set log level to info
    shmHeap.create()  # Create the shared memory heap
    yield shmHeap, acc
    shmHeap.close()  # Clean up resources


def testBasicAssignmentAndMemoryUsage(shmemListTest):
    shmHeap, acc = shmemListTest
    v = [[1] * 10 for _ in range(10)]
    acc.set(v)
    eachVecInt10Size = 8 + 4 * 10
    assert shmHeap.briefLayout() == [
        24,
        8 * 10,
        eachVecInt10Size,
        eachVecInt10Size,
        eachVecInt10Size,
        eachVecInt10Size,
        eachVecInt10Size,
        eachVecInt10Size,
        eachVecInt10Size,
        eachVecInt10Size,
        eachVecInt10Size,
        eachVecInt10Size,
        4096 - 13 * 8 - 24 - 8 * 10 - eachVecInt10Size * 10,
    ]

    v = [[i for i in range(1, j + 1)] for j in range(1, 11)]
    acc.set(v)
    assert shmHeap.briefLayout() == [
        24,
        8 * 10,
        24,
        24,
        24,
        24,
        32,
        32,
        40,
        40,
        48,
        48,
        3552,
    ]


def testTypeIdAndLen(shmemListTest):
    _, acc = shmemListTest
    acc.set(SList([1, 11, 111, 1111, 11111]))
    assert acc.len() == 5
    assert acc.typeStr() == "list"

    acc[4].set([[2], [22], [222], [2222], [22222]])
    acc.add(["t", "tt", "ttt", "tttt", "ttttt"])
    acc[4][4].set(["s", "ss", "sss", "ssss", "sssss"])
    acc[4][4][4].set([[9], [99], [999], [9999.0], [99999]])

    assert acc.len() == 6
    assert acc.typeStr() == "list"
    assert acc[4].typeStr() == "list"
    assert acc[5].typeStr() == "list"
    assert acc[5][1].typeStr() == "char"
    assert acc[4][3].typeStr() == "int"
    assert acc[4][4].typeStr() == "list"
    assert acc[4][4][3].typeStr() == "char"
    assert acc[4][4][4].typeStr() == "list"
    assert acc[4][4][4][3].typeStr() == "float"
    assert acc[4][4][4][4].typeStr() == "int"

    print(acc)


def testCreateEmptyList(shmemListTest):
    _, acc = shmemListTest
    acc.set(SList())
    assert acc.len() == 0
    assert acc.typeStr() == "list"
    assert acc.toString() == "(L:0)[\n]"
    print(acc)


def testSetAndGetAndAddElement(shmemListTest):
    _, acc = shmemListTest
    acc.set(SList([1, 11, 111, 1111, 11111]))
    result1 = acc.fetch()
    assert result1 == [1, 11, 111, 1111, 11111]

    acc.set([1, 11, 111, 1111, 11111])
    acc[0].set(bytes("s", "ascii"))
    result2 = acc.fetch()
    assert result2 == [115, 11, 111, 1111, 11111]  # 115 is 's' in ascii

    acc.set([[1], [11], [111], [1111], [11111]])
    acc.add([[2], [2, 2], [2], [2], [2]])
    # with pytest.raises(Exception):
    #     acc.fetch()

    acc.set(None)
    acc.set([[1], [11], [111], [1111], [11111]])
    acc[4].set([[2], [22], [222], [2222], [22222]])
    acc.add(["t", "tt", "ttt", "tttt", "ttttt"])
    acc[4][4].set(["s", "ss", "sss", "ssss", "sssss"])
    acc[4][4][4].set([[9], [99], [999], [9999], [99999]])

    result4 = acc[4][4][4].fetch()
    assert result4 == [9, 99, 999, 9999, 99999]

    # with pytest.raises(Exception):
    #     acc[4][4].fetch()

    acc[4][4][0].set([[4], [44], [444], [4444], [44444]])
    acc[4][4][1].set([[5], [55], [555], [5555], [55555]])
    acc[4][4][2].set([[6], [66], [666], [6666], [66666]])
    acc[4][4][3].set(["A", "BB", "CCC", "DDDD", "EEEEE"])

    result5 = acc[4][4].fetch()
    print(acc[4][4][3][0].typeStr())
    assert result5 == [
        [4, 44, 444, 4444, 44444],
        [5, 55, 555, 5555, 55555],
        [6, 66, 666, 6666, 66666],
        ["A", "BB", "CCC", "DDDD", "EEEEE"],
        [9, 99, 999, 9999, 99999],
    ]

    # with pytest.raises(Exception):
    #     acc[4].fetch()

    acc[4][0].set([[45], [67], [89]])
    acc[4][1].set([[46], [68], [90]])
    acc[4][2].set([[47], [69], [91]])
    acc[4][3].set([[48], [70], [92]])

    acc[4][4].set(SList([9, 99, 999, 9999, 99999]))
    
    assert acc[4].fetch() == [
        [45, 67, 89],
        [46, 68, 90],
        [47, 69, 91],
        [48, 70, 92],
        [9, 99, 999, 9999, 99999],
    ]

    acc.set(None)


def testAdd(shmemListTest):
    _, acc = shmemListTest
    acc.set(SList())
    acc.add([1])
    acc.add([11])
    acc.add([111])
    acc.add([1111])
    acc.add([11111])
    acc[4].set(SList())
    acc[4].add([2])
    acc[4].add([22])
    acc[4].add([222])
    acc[4].add([2222])
    acc[4].add([22222])
    acc.add("tt")
    acc.add("ttt")
    acc.add("tttt")
    acc.add("ttttt")
    acc[4][3].set("ssss")
    acc[4][4].set("sssss")

    with pytest.raises(RuntimeError):
        acc[4][5].fetch()

    assert acc.len() == 9
    assert acc[4].len() == 5


def testDel(shmemListTest):
    _, acc = shmemListTest
    acc.set([[1], [11], [111], [1111], [11111]])
    acc[4].set(SList([2, 22, 222, 2222, 22222]))
    acc.add(["t", "tt", "ttt", "ttft"])
    acc[4][4].set(["s", "ss", "sss", "ssss", "sssss"])

    del acc[4]
    assert acc[4].fetch() == ["t", "tt", "ttt", "ttft"]

    with pytest.raises(RuntimeError):
        acc[4][4].fetch()

    assert acc[4][3].fetch() == "ttft"

    del acc[4][3][2]
    assert acc[4][3].fetch() == "ttt"
    assert acc[4][3].len() == 4


# def testContains(shmemListTest):
#     _, acc = shmemListTest
#     acc.set([[1], [11], [111], [1111], [11111]])
#     acc[4].set([[2], [22], [222], [2222], [22222]])
#     acc.add(["t", "tt", "ttt", "tttt", "ttttt"])
#     acc[4][4].set(["s", "ss", "sss", "ssss", "sssss"])
#     assert acc[4][4].contains("ssss")

#     acc[4][4][0].set([[4], [44], [444], [4444], [44444]])
#     acc[4][4][1].set([[5], [55], [555], [5555], [55555]])
#     acc[4][4][2].set([[6], [66], [666], [6666], [66666]])
#     acc[4][4][3].set(["A", "BB", "CCC", "DDDD", "EEEEE"])
#     acc[4][4][4].set([[9], [99], [999], [9999], [99999]])
#     assert not acc[4][4].contains("ssss")

#     assert acc[4][4].contains(["A", "BB", "CCC", "DDDD", "EEEEE"])
#     assert acc[4][4].contains([[6], [66], [666], [6666], [66666]])


def testConvertToPythonObject(shmemListTest):
    _, acc = shmemListTest
    acc.set([1.0] * 10)
    assert str(acc.fetch()) == "[1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]"
    acc.set([1] * 10)
    assert str(acc.fetch()) == "[1, 1, 1, 1, 1, 1, 1, 1, 1, 1]"
    acc.set([True] * 10)
    result = acc.fetch()
    assert (
        str(result)
        == "[True, True, True, True, True, True, True, True, True, True]"
    )

    acc.set(["s"])
    assert str(acc.fetch()) == "['s']"

    acc.set(SList([1, 11, 111, 1111, 11111]))
    acc[0].set(SList([2, 22, 222, 2222, 22222]))
    acc[0][0].set([True] * 10)

    assert (
        str(acc.fetch())
        == "[[[True, True, True, True, True, True, True, True, True, True], 22, 222, 2222, 22222], 11, 111, 1111, 11111]"
    )
    print(acc)


if __name__ == "__main__":
    pytest.main(["-v", "pytest/ShmemList_test.py"])
    # pytest.main(["-v", "pytest/ShmemList_test.py::testSetAndGetAndAddElement"])


    # shmHeap = ShmemHeap("test_shm_list", 80, 1024)
    # acc = ShmemAccessor(shmHeap)
    # shmHeap.setLogLevel(4)  # Set log level to info
    # shmHeap.create()  # Create the shared memory heap

    # acc.set(SList([1, 11, 111, 1111, 11111]))
    # result1 = acc.fetch()
    # assert result1 == [1, 11, 111, 1111, 11111]
    # acc.set([1.0] * 10)
    # assert str(acc.fetch()) == "[1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]"
    # acc.set([1] * 10)
    # assert str(acc.fetch()) == "[1, 1, 1, 1, 1, 1, 1, 1, 1, 1]"
    # acc.set([True] * 10)
    # result = acc.fetch()
    # assert (
    #     str(result)
    #     == "[True, True, True, True, True, True, True, True, True, True]"
    # )

    # acc.set(["s"])
    # assert str(acc.fetch()) == "['s']"

    # acc.set([[1], [11], [111], [1111], [11111]])
    # acc[0].set([[2], [22], [222], [2222], [22222]])
    # acc[0][0].set([True] * 10)
    
    # assert (
    #     str(acc.fetch())
    #     == "[[[True, True, True, True, True, True, True, True, True, True], 22, 222, 2222, 22222], 11, 111, 1111, 11111]"
    # )
    # print(acc)

