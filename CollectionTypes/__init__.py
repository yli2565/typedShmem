from typing import Dict, Literal, Type, Union
from .ShmemCollection import ShmemCollection
from .vector import ShmemVector
from .map import ShmemMap
from .tuple import ShmemTuple

CollectionTypeList = [ShmemVector, ShmemMap, ShmemTuple]
CollectionTypeNameList = [
    "vector",
    "map",
    "tuple",
    "Vector",
    "Map",
    "Tuple",
    "ShmemVector",
    "ShmemMap",
    "ShmemTuple",
]

CollectionType = ShmemCollection
CollectionTypeHint = Union[
    Type[ShmemCollection],
    Literal["vector"],
    Literal["map"],
    Literal["tuple"],
    Literal["Vector"],
    Literal["Map"],
    Literal["Tuple"],
]

CollectionTypeSelfMapping = {
    ShmemVector: ShmemVector,
    ShmemMap: ShmemMap,
    ShmemTuple: ShmemTuple,
}
CType2CollectionType = {"vector": ShmemVector, "map": ShmemMap, "tuple": ShmemTuple}
Short2CollectionType = {"Vector": ShmemVector, "Map": ShmemMap, "Tuple": ShmemTuple}
FullName2CollectionType = {
    "ShmemVector": ShmemVector,
    "ShmemMap": ShmemMap,
    "ShmemTuple": ShmemTuple,
}

Indicator2CollectionType: Dict[Union[str, Type], Type[ShmemCollection]] = {
    **CollectionTypeSelfMapping,
    **CType2CollectionType,
    **Short2CollectionType,
    **FullName2CollectionType,
}

from StreamUtil.PrimitiveStream import PrimitiveStream

def readCollection(collectionType, stream:PrimitiveStream):
    rootType=Indicator2CollectionType[collectionType]
    if rootType is ShmemVector:
        contentType=stream.readStr()
        readCollection(collectionType, stream)
        len=stream.readInt()
        return ShmemVector(contentType, len)
    elif rootType is ShmemMap:

        return ShmemMap.read(stream)
    elif rootType is ShmemTuple:
        return ShmemTuple.read(stream)
    
def readDescriptor(stream:PrimitiveStream):
    rootType=stream.readStr()
    result=None
    if rootType is ShmemVector:
        contentType=readDescriptor(stream)
        result= ShmemVector(contentType,len)
    elif rootType is ShmemTuple:
        contentTypeArray=[]
        len=stream.readUInt()
        for contentType in range(len):
            contentTypeArray.append(readDescriptor(stream))
        result=ShmemTuple(contentTypeArray)

    elif rootType is ShmemMap:

        return ShmemMap.read(stream)

    else:
