from .ShmemCollection import ShmemCollection
from StreamUtil.PrimitiveStream import PrimitiveStream


class ShmemVector(ShmemCollection):
    def __init__(self, contentType, Len):
        self._contentType = contentType
        self._Len = Len

    @classmethod
    def read(self,stream:PrimitiveStream):
        contentType=stream.readStr()
        len=stream.readInt()

    @classmethod
    def writeContent(cls):
        pass

    @classmethod
    def readContent(cls):
        pass

    @classmethod
    def writeDescriptor(cls):
        pass

    @classmethod
    def readDescriptor(
        cls,
    ):
        pass
