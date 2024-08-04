from abc import ABC, abstractmethod


class ShmemCollection(ABC):
    @abstractmethod
    def __init__(self):
        pass

    @abstractmethod
    @classmethod
    def readDescriptor(cls):
        pass

    @abstractmethod
    @classmethod
    def writeDescriptor(cls):
        pass

    @abstractmethod
    @classmethod
    def readContent(cls):
        pass

    @abstractmethod
    @classmethod
    def writeContent(cls):
        pass
