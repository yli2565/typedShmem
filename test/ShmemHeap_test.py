# test_math_utils.py
import pytest
from TypedShmem import ShmemHeap, ShmemAccessor


# Define a fixture for setup and teardown
@pytest.fixture
def setup():
    shmHeap = ShmemHeap("test_shm_heap", 80, 1024)
  
# Test function using the fixture
def test_add(setup):
    # Access the setup data from the fixture
    shmHeap.create()
    acc = ShmemAccessor(shmHeap)
    print(str(acc))
