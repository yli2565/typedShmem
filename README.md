# TypedShmem

TypedShmem is a library for typed shared memory communication between processes in C++ and Python. It provides a high-performance way to share complex data structures like dictionaries and lists between different processes with type safety.

## Features

- Shared memory communication between processes
- Support for primitive types, lists, and dictionaries
- Full type safety across language boundaries
- Python bindings with intuitive API
- Efficient memory management with custom heap implementation
- Thread-safe operations with semaphore protections

## Installation

### From Source

```bash
# Clone the repository
git clone https://github.com/yourusername/TypedShmem.git
cd TypedShmem

# Build C++ library and Python bindings
cmake -S . -B build && cmake --build build

# Install Python package
pip install .
```

### Using pip

```bash
pip install TypedShmem
```

## Basic Usage

### Python Example

```python
from TypedShmem import ShmemHeap, ShmemAccessor, SDict, SList

# Create a shared memory heap
heap = ShmemHeap("my_shared_memory", staticSpaceSize=16, heapSize=4096)
heap.create()

# Create a shared memory accessor
accessor = ShmemAccessor(heap)

# Store a dictionary in shared memory
accessor.set(SDict({
    "name": "TypedShmem",
    "numbers": [1, 2, 3, 4, 5],
    "nested": {"key": "value"},
    "active": True
}))

# Access the data
print(accessor["name"])  # Output: TypedShmem
print(accessor["numbers"][2])  # Output: 3
print(accessor["nested"]["key"])  # Output: value

# Store a list in shared memory
accessor.set(SList([1, 2, 3, "string", True, {"key": "value"}]))
print(accessor[0])  # Output: 1
print(accessor[3])  # Output: string

# Clean up when done
heap.close()
heap.unlink()
```

### C++ Example

```cpp
#include "ShmemHeap.h"
#include "ShmemDict.h"
#include "ShmemList.h"
#include "ShmemAccessor.h"

int main() {
    // Create a shared memory heap
    ShmemHeap heap("my_shared_memory", 16, 4096);
    heap.create();
    
    // Create a shared memory accessor
    ShmemAccessor accessor(heap);
    
    // Work with the shared memory
    // ... (similar operations as in Python)
    
    // Clean up
    heap.close();
    heap.unlink();
    
    return 0;
}
```

## License

This project is licensed under the MIT License - see the LICENSE.txt file for details.
