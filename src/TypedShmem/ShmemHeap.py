from .TypedShmem import ShmemHeap as ShmemHeap_pybind11

DHCap = 0x1000
DSCap = 0x8


class ShmemHeap(ShmemHeap_pybind11):
    """
    A Python wrapper for the ShmemHeap class from pybind11 with added documentation 
    based on the original C++ class.
    """

    def __init__(self, name, staticSpaceSize=DSCap, heapSize=DHCap):
        """
        Constructor for ShmemHeap.

        :param name: Name of the shared memory heap.
        :param staticSpaceSize: Size of the static space (default: 8 bytes).
        :param heapSize: Size of the heap (default: 4096 bytes, one page).
        """
        super().__init__(name, staticSpaceSize, heapSize)

    def create(self):
        """
        Create a basic shared memory and set up the heap on it.
        
        Note: The static space and heap capacity can be set with setSCap() and 
        setHCap() before calling create(). After that, setting HCap and SCap 
        will have no effect.
        """
        super().create()

    def connect(self):
        """
        Connect to the shared memory heap.
        """
        super().connect()

    def close(self):
        """
        Close the shared memory and free the heap.
        """
        super().close()

    def unlink(self):
        """
        Unlink the shared memory heap.
        """
        super().unlink()

    def resize(self, staticSpaceSize=-1, heapSize=-1):
        """
        Resize both static and heap space.

        :param staticSpaceSize: New size for static space, padded to unit size. 
                                -1 means not changed.
        :param heapSize: New size for the heap, padded to page size. -1 means not changed.
        """
        super().resize(staticSpaceSize, heapSize)

    def shmalloc(self, size):
        """
        Allocate a block in the heap.

        :param size: Size of the payload, will be padded to a multiple of unitSize.
        :return: Offset of the allocated block from the heap head.
        """
        return super().shmalloc(size)

    def shrealloc(self, offset, size):
        """
        Reallocate a block in the heap, keeping the content.

        :param offset: Offset of the original block from the heap head.
        :param size: New size of the payload, padded to a multiple of unitSize.
        :return: Offset of the reallocated block from the heap head.
        """
        return super().shrealloc(offset, size)

    def shfree(self, offset):
        """
        Free a block in the heap.

        :param offset: Offset of the payload from the heap head.
        :return: 0 on success, -1 on failure.
        """
        return super().shfree(offset)

    def getHCap(self):
        """
        Check the current purposed heap capacity.

        :return: Purposed heap capacity.
        """
        return super().getHCap()

    def getSCap(self):
        """
        Check the current purposed static space capacity.

        :return: Purposed static space capacity.
        """
        return super().getSCap()

    def staticCapacity(self):
        """
        Get the size of the static space recorded in the first size_t(8 bytes) of the heap.

        :return: Reference to the first 8 bytes of the heap.
        """
        return super().staticCapacity()

    def heapCapacity(self):
        """
        Get the size of the heap recorded in the second size_t(8 bytes) of the heap.

        :return: Reference to the second 8 bytes of the heap.
        """
        return super().heapCapacity()

    def freeBlockListOffset(self):
        """
        Get the offset of the free block list, recorded in the third size_t(8 bytes) of the heap.

        :return: Reference to the offset of the free block list.
        """
        return super().freeBlockListOffset()

    def entranceOffset(self):
        """
        Get the offset of the entrance, recorded in the fourth size_t(8 bytes) of the heap.

        :return: Reference to the offset of the entrance point object.
        """
        return super().entranceOffset()

    def staticSpaceHead(self):
        """
        Get the head pointer of the static space. Check connection before using.

        :return: Pointer to the start of the static space.
        """
        return super().staticSpaceHead()

    def entrance(self):
        """
        Entrance point of the underlying data structure.

        :return: Pointer to the entrance of the heap.
        """
        return super().entrance()

    def heapHead(self):
        """
        Get the head pointer of the heap. Check connection before using.

        :return: Pointer to the start of the heap.
        """
        return super().heapHead()

    def heapTail(self):
        """
        Get the tail pointer of the heap. Check connection before using.

        :return: Pointer to the end of the heap.
        """
        return super().heapTail()

    def freeBlockList(self):
        """
        Get the head pointer of the free block list. Check connection before using.

        :return: Pointer to the head of the free block list.
        """
        return super().freeBlockList()

    def setHCap(self, size):
        """
        Repurpose the heap capacity.

        :param size: Purposed heap capacity.
        """
        super().setHCap(size)

    def setSCap(self, size):
        """
        Repurpose the static space capacity.

        :param size: Purposed static space capacity.
        """
        super().setSCap(size)

    def setLogLevel(self, level):
        """
        Set the logger level.

        :param level: Logger level.
        """
        super().setLogLevel(level)

    def setLogPattern(self, pattern):
        """
        Set the logger pattern (spdlog format string).
        It's very alike to python format string, but please check specific placeholders used by spdlog.

        :param pattern: Logger pattern.
        """
        super().setLogPattern(pattern)

    def getLogger(self):
        """
        Get the logger associated with the shared memory heap.

        :return: Shared logger instance.
        """
        return super().getLogger()

    def printShmHeap(self):
        """
        Print the layout of the heap in detail.
        """
        super().printShmHeap()

    def briefLayout(self):
        """
        Get sizes of all blocks in the heap without state information.

        :return: List of sizes of all blocks.
        """
        return super().briefLayout()

    def briefLayoutStr(self):
        """
        Get a string representation of the payload sizes and allocation status of all blocks in the heap.

        :return: A string describing the layout of the heap.
        """
        return super().briefLayoutStr()
