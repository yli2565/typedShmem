import numpy as np

from .PrimitiveDefinitions import *

NumpySelfMapping = {
    np.uint32: np.uint32,
    np.int32: np.int32,
    np.uint8: np.uint8,
    np.int8: np.int8,
    str: str,
    np.bool_: np.bool_,
    np.uint16: np.uint16,
    np.int16: np.int16,
    np.int64: np.int64,
    np.float64: np.float64,
    np.float32: np.float32,
    np.int8: np.int8,
}

CType2Numpy = {
    "unsigned int": np.uint32,  # NumPy's unsigned 32-bit integer
    "int": np.int32,  # NumPy's 32-bit integer
    "unsigned char": np.uint8,  # NumPy's unsigned 8-bit integer (0 to 255)
    "char": np.int8,  # NumPy's 8-bit integer, can also represent characters with ord/chr
    "std::string": str,  # For Unicode strings; str for byte strings
    "bool": np.bool_,  # NumPy's boolean type
    "unsigned short": np.uint16,  # NumPy's unsigned 16-bit integer
    "short": np.int16,  # NumPy's 16-bit integer
    "double": np.float64,  # NumPy's 64-bit floating point number
    "float": np.float32,  # NumPy's 32-bit floating point number
    "signed char": np.int8,  # NumPy's signed 8-bit integer (-128 to 127)
    # Extra Alias
    "size_t": np.uint64,  # NumPy's unsigned 64-bit integer
}


Short2Numpy = {
    "UInt": np.uint32,  # NumPy's unsigned 32-bit integer
    "Int": np.int32,  # NumPy's 32-bit integer
    "UChar": np.uint8,  # NumPy's unsigned 8-bit integer (0 to 255)
    "Char": np.int8,  # NumPy's 8-bit integer, can also represent characters with ord/chr
    "Str": str,  # For Unicode strings; str for byte strings
    "Bool": np.bool_,  # NumPy's boolean type
    "UShort": np.uint16,  # NumPy's unsigned 16-bit integer
    "Short": np.int16,  # NumPy's 16-bit integer
    "Double": np.float64,  # NumPy's 64-bit floating point number
    "Float": np.float32,  # NumPy's 32-bit floating point number
    "SChar": np.int8,  # NumPy's signed 8-bit integer (-128 to 127)
    # Extra Alias
    "SizeT": np.uint64,  # NumPy's unsigned 64-bit integer
}

Indicator2PrimitiveType = {**NumpySelfMapping, **CType2Numpy, **Short2Numpy}
"""map indicator to real type"""

NPPrimitiveTypeList = [
    UInt,
    Int,
    UChar,
    Char,
    Bool,
    UShort,
    ULong,
    Short,
    Double,
    Float,
    SChar,
]

CType2PyStr = {
    "unsigned int": "UInt",
    "int": "Int",
    "unsigned char": "UChar",
    "char": "Char",
    "std::string": "Str",
    "bool": "Bool",
    "unsigned short": "UShort",
    "unsigned long": "ULong",
    "short": "Short",
    "double": "Double",
    "float": "Float",
    "signed char": "SChar",
    # Extra Alias
    "size_t": "ULong",
}

PrimitiveTypeList = [*NPPrimitiveTypeList, Str]


Primitive2PythonTypeMapping = {
    np.uint32: int,
    np.int32: int,
    np.uint8: int,
    np.int8: int,
    str: str,
    np.bool_: bool,
    np.uint16: int,
    np.uint64: int,
    np.int16: int,
    np.float64: float,
    np.float32: float,
    np.int8: int,
}
