from typing import Any

from PrimitiveTypes import *
from .StreamAlikeWrapper import StreamAlikeWrapper, StreamAlike
from CollectionTypes import ShmemCollection
"""Type acceptable to init a StreamUtil"""


class StreamUtil(StreamAlikeWrapper):
    """Suggestion: if you assign a stream to StreamUtil, you should not use it elsewhere"""

    def __init__(self, stream: StreamAlike, showProgress=False, desc="Streaming"):
        """
        stream: Can be any type of object that supports read(), seek(), tell(), close(), but it might trigger warnings from StreamAlike type check
        """
        super().__init__(stream, showProgress, desc)

    # Read Primitives (Core function)
    def readPrimitives(self, typeIndicator: PrimitiveTypeHint, length: int = 1) -> Any:
        """
        General function to read all primitives
        typeIndicator: String (C format or np format) or Type format of target primitive, e.g. 'unsigned char' = 'UChar' = np.uint8 = UChar, will read an np.uint8 value
        length: Number of primitives to read, ==1 will return single value, ==-1 will first read a UInt to determine the array length then read the array, else read the array with the given length
        """
        type = Indicator2PrimitiveType[typeIndicator]
        originalLength = length

        if type is Str:
            return self.readStr(originalLength)
        else:
            if length == -1:
                length = np.frombuffer(self.read(self.getSize(UInt)), np.uint32)[0]

            numBytes = self.getSize(type) * length
            result = np.frombuffer(self.read(numBytes), type)

        if originalLength == 1:
            result = result[0]
        return result

    def readStr(self, length=1) -> Any:
        """Special function for reading Str Non-Numpy Primitive"""
        if length == 1:
            return self.read(self.readPrimitives(UInt)).decode("ascii")
        elif length == -1:
            length = self.readPrimitives(UInt)

        return [
            self.read(self.readPrimitives(UInt)).decode("ascii") for _ in range(length)
        ]

    # Specific read functions
    def readQueueHeader(self) -> tuple[UInt, UInt, UInt]:
        """(high, messages, low)"""
        all = self.readLInt()
        low = UInt(all & 0xFFFFFFFF)
        messages = UInt((all >> (32)) & 0xFFFFFFF)
        high = UInt((all >> (32 + 28)) & 0xF)
        return (high, messages, low)

    def readMessageHeader(self) -> tuple[UChar, UInt]:
        """(messageId, messageSize)"""
        header = self.read(4)
        messageId = int.from_bytes(header[0:1], "little")
        messageSize = int.from_bytes(header[1:4], "little")
        return UChar(messageId), UInt(messageSize)

    def readTypeDescriptor(self) -> str:
        rootType = self.readStr()
        if rootType in Indicator2PrimitiveType:
            return Indicator2PrimitiveType[rootType]
        elif isinstance(rootType, ShmemCollection):
            rootType.readDescriptor(self)
            vectorContentType = self.readTypeDescriptor()
            vectorLength = self.readInt()
            return 

            


    # Util functions
    def readUInt(self, length=1) -> Any:
        return self.readPrimitives(UInt, length)

    def readInt(self, length=1) -> Any:
        return self.readPrimitives(Int, length)

    def readUChar(self, length=1) -> Any:
        return self.readPrimitives(UChar, length)

    def readChar(self, length=1) -> Any:
        return self.readPrimitives(Char, length)

    def readBool(self, length=1) -> Any:
        return self.readPrimitives(Bool, length)

    def readUShort(self, length=1) -> Any:
        return self.readPrimitives(UShort, length)

    def readLong(self, length=1) -> Any:
        return self.readPrimitives(ULong, length)

    def readShort(self, length=1) -> Any:
        return self.readPrimitives(Short, length)

    def readDouble(self, length=1) -> Any:
        return self.readPrimitives(Double, length)

    def readFloat(self, length=1) -> Any:
        return self.readPrimitives(Float, length)

    def readSChar(self, length=1) -> Any:
        return self.readPrimitives(SChar, length)

    # Helper functions
    def isHashable(self, obj) -> bool:
        """Check if an object is hashable"""
        try:
            hash(obj)
            return True
        except TypeError:
            return False

    def getSize(self, type: PrimitiveTypeHint) -> int:
        """Get size of a ReadAble type"""
        if type not in Indicator2PrimitiveType:
            errMsg=f"Unsupported type: {type}"
            self.logger.error(errMsg)
            raise ValueError(errMsg)
        type = Indicator2PrimitiveType[type]
        if type is Str:
            return 1
        elif type in NPPrimitiveTypeList:
            return np.dtype(type).itemsize

        raise ValueError(f"Code should not reach here: {type}")
