import pytest
from TypedShmem import ShmemHeap, ShmemAccessor


@pytest.fixture
def shmemPrimitiveTest():
    """Fixture to initialize and cleanup ShmemHeap and ShmemAccessor for each test."""
    shmHeap = ShmemHeap("test_shm_heap", 80, 1024)
    acc = ShmemAccessor(shmHeap)
    shmHeap.setLogLevel(0)  # Set log level to info or appropriate level
    shmHeap.create()  # Create the shared memory heap
    yield shmHeap, acc
    shmHeap.close()  # Clean up resources


def testBasicAssignmentAndMemoryUsage(shmemPrimitiveTest):
    shmHeap, acc = shmemPrimitiveTest
    unitSize = 8  # Assuming unitSize is 8 bytes; adjust based on actual value

    acc.set(1)
    expectedMemSize = 24  # min payload is 24
    assert shmHeap.briefLayout() == [
        expectedMemSize,
        4096 - expectedMemSize - unitSize * 2,
    ]
    assert acc.toString() == "(P:int:1)[1]"

    acc.set(None)
    assert shmHeap.briefLayout() == [4096 - unitSize]
    assert acc.toString() == "nullptr"

    acc.set([1.0] * 10)
    expectedMemSize = 8 + 40  # ShmemPrimitive header 8 bytes + 4*10 + 0 padding
    assert shmHeap.briefLayout() == [
        expectedMemSize,
        4096 - expectedMemSize - unitSize * 2,
    ]

    acc.set([1] * 10)
    expectedMemSize = 8 + 40  # ShmemPrimitive header 8 bytes + 4*10 + 0 padding
    assert shmHeap.briefLayout() == [
        expectedMemSize,
        4096 - expectedMemSize - unitSize * 2,
    ]
    assert acc.toString() == "(P:int:10)[1, 1, 1, 1, 1, 1, 1, 1, 1, 1]"

    # acc.set([1] * 10)
    # expectedMemSize = 8 + 8 * 10  # ShmemPrimitive header 8 bytes + 8*10 + 0 padding
    # assert shmHeap.briefLayout() == [expectedMemSize, 4096 - expectedMemSize - unitSize * 2]
    # assert acc.toString() == "(P:unsigned long:10)[1, 1, 1, 1, 1, 1, 1, 1, 1, 1]"
    # assert acc.toString(4) == "(P:unsigned long:10)[1, 1, 1, 1, ...]"


def testTypeIdAndLen(shmemPrimitiveTest):
    _, acc = shmemPrimitiveTest

    # acc.set(7.0)
    # assert acc.len() == 1
    # assert acc.typeStr() == "double"

    acc.set([1.0] * 10)
    assert acc.len() == 10
    assert acc.typeStr() == "float"

    acc.set("string")
    assert acc.len() == 7
    assert acc.typeStr() == "char"


def testSetAndGetElement(shmemPrimitiveTest):
    _, acc = shmemPrimitiveTest

    acc.set(7.0)
    assert acc.fetch() == 7
    # assert acc.get() == [7]

    acc.set(None)
    assert acc.fetch() is None
    with pytest.raises(Exception):
        acc[0] = 1

    acc.set([1.0] * 10)

    with pytest.raises(Exception):
        acc.get()

    acc[0].set(ord(b"s".decode("ascii")))
    assert acc[0] == ord(b"s".decode("ascii"))
    acc[1].set(11)
    assert acc[1] == 11
    acc[2].set(12)
    assert acc[2] == 12
    assert acc.fetch() == [ord(b"s".decode("ascii")), 11, 12, 1, 1, 1, 1, 1, 1, 1]

    with pytest.raises(Exception):
        acc[10] = ord(b"s".decode("ascii"))

    with pytest.raises(Exception):
        acc[10] = [1] * 10

    acc.set("string")
    assert acc.fetch() == "string"
    assert acc.len() == 7
    assert acc.typeStr() == "char"


# def testContains(shmemPrimitiveTest):
#     _, acc = shmemPrimitiveTest

#     acc.set([1.0] * 10)
#     acc[0] = ord(b"s".decode("ascii"))
#     acc[9] = ord(b"a".decode("ascii"))

#     assert acc.contains(ord(b"s".decode("ascii")))
#     assert acc.contains(ord(b"a".decode("ascii")))
#     assert not acc.contains(ord(b"b".decode("ascii")))


def testConvertToPythonObject(shmemPrimitiveTest):
    _, acc = shmemPrimitiveTest

    acc.set(1)
    assert acc.fetch() == 1

    acc.set(3.14)
    assert acc.fetch() == pytest.approx(3.14)

    acc.set(12345678.9123456)
    assert acc.fetch() == pytest.approx(12345678.9123456)

    acc.set([1.0] * 10)
    assert str(acc.fetch()) == "[1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]"

    acc.set([1] * 10)
    assert str(acc.fetch()) == "[1, 1, 1, 1, 1, 1, 1, 1, 1, 1]"

    acc.set([True] * 10)
    assert (
        str(acc.fetch())
        == "[True, True, True, True, True, True, True, True, True, True]"
    )

    # with pytest.raises(Exception):
    #     acc.fetch()

    acc.set([115] * 10)
    # assert acc.fetch() == "ssssssssss"
    # assert str(acc.fetch()) == "[115, 115, 115, 115, 115, 115, 115, 115, 115, 115]"


def testAssignWithPythonObject(shmemPrimitiveTest):
    _, acc = shmemPrimitiveTest

    acc.set(list(range(10)))
    assert str(acc.fetch()) == "[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]"

    acc.set([0, 1, 2, 3, 4, 100, 6, 7, 8, 9])
    assert str(acc.fetch()) == "[0, 1, 2, 3, 4, 100, 6, 7, 8, 9]"

    acc.set(True)
    assert acc.fetch() is True

    acc.set(3)
    assert acc.fetch() == 3

    acc.set(3.14)
    assert acc.fetch() == pytest.approx(3.14)

    acc.set("sample")
    assert acc.fetch() == "sample"

    # with pytest.raises(Exception):
    #     acc.fetch()

    acc.set(b"sample")
    # with pytest.raises(Exception):
    #     acc.fetch()

    assert acc.fetch() == [
        ord(b"s".decode("ascii")),
        ord(b"a".decode("ascii")),
        ord(b"m".decode("ascii")),
        ord(b"p".decode("ascii")),
        ord(b"l".decode("ascii")),
        ord(b"e".decode("ascii")),
    ]
    print(acc.fetch())


if __name__ == "__main__":
    pytest.main(["-v", "pytest/ShmemPrimitive_test.py"])
