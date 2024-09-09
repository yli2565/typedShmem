from .TypedShmem import setShmemUtilLogLevel as setShmemUtilLogLevel_pybind11

def setShmemUtilLogLevel(level: int):
    setShmemUtilLogLevel_pybind11(level)
