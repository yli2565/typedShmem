from typing import Any, Union

from .TypedShmem import ShmemObjInitializer as ShmemObjInitializer_pybind11

# from .TypedShmem import SDict as SDict_pybind11
# from .TypedShmem import SList as SList_pybind11
from .TypeEncodings import Dict as Dict_val
from .TypeEncodings import List as List_val


def SDict(initVal=None) -> Union[Any, "ShmemObjInitializer"]:
    return ShmemObjInitializer(Dict_val, initVal)


def SList(initVal=None) -> Union[Any, "ShmemObjInitializer"]:
    return ShmemObjInitializer(List_val, initVal)


class ShmemObjInitializer(ShmemObjInitializer_pybind11):
    """
    Can explicitly initialize a shared memory object to specific type.
    acc.set([0, 1, 2, 3]) will assigned a Primitive array
    acc.set(SList([0, 1, 2, 3])) will assigned a List
    """

    def __init__(self, typeId: int, initVal: Any = None):
        super().__init__(typeId, initVal)

    @property
    def typeId(self) -> int:
        """
        The type ID of the proposed initial value.
        """
        return super().typeId

    @property
    def typeStr(self) -> str:
        """
        The type name of the proposed initial value.
        """
        return super().typeStr

    @property
    def initialVal(self) -> Any:
        """
        The initial value as a Python object.
        """
        return super().initialVal
