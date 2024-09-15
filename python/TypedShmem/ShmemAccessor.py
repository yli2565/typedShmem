from typing import Any, Dict, List, Optional, Union

from .ShmemHeap import ShmemHeap
from .ShmemObjInitializer import ShmemObjInitializer
from .TypedShmem import ShmemAccessor as ShmemAccessor_pybind11

KeyType = Union[int, str]
ValueType = Union[int, str, float, bool, List, Dict]

class ShmemAccessor(ShmemAccessor_pybind11):
    """
    A Python wrapper for the ShmemAccessor class from pybind11, providing access
    to shared memory with dictionary-like behavior and additional functionalities.
    """

    def __init__(self, heap: ShmemHeap, keys: Optional[List[KeyType]] = None):
        """
        Constructor for ShmemAccessor.

        :param heap: A reference to a ShmemHeap instance.
        :param keys: Optional list of keys to initialize the accessor.
        """
        if keys is None:
            super().__init__(heap)
        else:
            super().__init__(heap, keys)

    def __getitem__(self, key: KeyType) -> ValueType:
        """
        Get an item from the shared memory using the given key.

        :param key: The key to retrieve the associated value.
        :return: The value associated with the given key.
        """
        return super().__getitem__(key)

    def __setitem__(self, key: KeyType, value: ValueType) -> None:
        """
        Set an item in the shared memory with the given key and value.

        :param key: The key under which to store the value.
        :param value: The value to be stored.
        """
        super().__setitem__(key, value)

    def __delitem__(self, key: KeyType) -> None:
        """
        Delete an item from the shared memory by key (in a dict) / index (in a list).

        :param key: The key of the item to delete.
        """
        super().__delitem__(key)

    def __contains__(self, key: KeyType) -> bool:
        """
        Check if the shared memory contains a given key.

        :param key: The key to check for existence.
        :return: True if the key exists, otherwise False.
        """
        return super().__contains__(key)

    def contains(self, key: KeyType) -> bool:
        """
        Check if the shared memory contains a given key.

        :param key: The key to check for existence.
        :return: True if the key exists, otherwise False.
        """
        return super().contains(key)
    
    def __len__(self) -> int:
        """
        Get the number of items in the shared memory.

        :return: The number of items.
        """
        return super().__len__()

    def len(self) -> int:
        """
        Get the number of items in the shared memory.

        :return: The number of items.
        """
        return super().len()

    def __str__(self) -> str:
        """
        Get a string representation of the shared memory content.

        :return: String representation of the accessor.
        """
        return super().__str__()
    
    def toString(self) -> str:
        """
        Get a string representation of the shared memory content.

        :return: String representation of the accessor.
        """
        return super().toString()

    def __repr__(self) -> str:
        """
        Get a brief string representation of the ShmemAccessor.

        """
        return super().__repr__()

    def typeId(self) -> int:
        """
        Get the type ID of the accessor.

        :return: The type ID.
        """
        return super().typeId()

    def typeStr(self) -> str:
        """
        Get the type as a string.

        :return: String representation of the type.
        """
        return super().typeStr()
    
    def fetch(self) -> ValueType:
        """
        Fetch the object pointed by the current accessor.

        :return: The object pointed by the current accessor.
        """
        return super().fetch()
    
    def get(self, key) -> ValueType:
        """
        Retrieve an object from the shared memory using the given key.

        :param key: The key to retrieve the object.
        :return: The object associated with the key.
        """
        return super().get(key)

    def set(self, value: ValueType):
        """
        Set the value of obejct pointed by current accessor to the given value

        :param value: The object to be stored.
        """
        super().set(value)

    def add(self, value: ValueType):
        """
        Add a value pair to underlying shared memory list.

        :param value: The value associated with the key.
        """
        super().add(value)

    def insert(self, key: KeyType, value: ValueType):
        """
        Insert a key-value pair into underlying shared memory dict.

        :param key: The key to insert.
        :param value: The value associated with the key.
        """
        super().insert(key, value)

    def __iter__(self):
        """
        Return an iterator over the current shared memory object if it is a collection.
        Note: Actually, it just provide the and accessor with path to the next item

        :return: Self as an iterator.
        """
        return super().__iter__()

    def __next__(self) -> "ShmemAccessor":
        """
        Yield current item and move to next, current item is also an accessor

        :return: The next item.
        """
        return super().__next__()

    def __eq__(self, other: Union["ShmemAccessor", ValueType]) -> bool:
        """
        if other is a ShmemAccessor, check if their path euqals
        if other is a python value, check if it is equal to the object pointed by current accessor

        :param other: The other ShmemAccessor / a python value
        :return: True if equal, otherwise False.
        """
        return super().__eq__(other)
