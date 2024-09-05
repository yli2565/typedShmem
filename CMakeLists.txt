# Minimum required version of CMake
cmake_minimum_required(VERSION 3.15...3.27)

# Set the project name and specify the language as C/C++
project("TypedShmem" LANGUAGES C CXX)


# Set and enforce the C++11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# For courses (need to be strict)
# set(CMAKE_C_EXTENSIONS ON)  # This enables the GNU extensions, equivalent to -std=gnu11
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -Wall -Wextra -Werror -pedantic -Wno-unused-result -Wconversion") # More warnings

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build the test suite" ON)

# Flags to generate intermediate files
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -save-temps")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -save-temps")

# Debug and release optimization flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")

# Include the /include directory for header files
include_directories(include)

# Gather all source files from the /src directory
file(GLOB MAIN_SRC_FILES src/*.cpp src/*.c)
# Gather all test source files from the /test directory
file(GLOB TEST_SRC_FILES src/*.cpp src/*.c test/*.cpp test/*.c)

# Avoid the main conflict between test.cpp and main.cpp
list(REMOVE_ITEM TEST_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
list(REMOVE_ITEM TEST_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Specify the names of the executables
set(MAIN_EXCUTABLE "main")
set(TEST_EXCUTABLE "test")

# Create executable targets for main and test
add_executable(${MAIN_EXCUTABLE} ${MAIN_SRC_FILES})
add_executable(${TEST_EXCUTABLE} ${TEST_SRC_FILES})

# Add library subdirectory
set(LIB_DIR "lib")

# automaticly add lib subdirectories
file(GLOB SUBDIRS "${CMAKE_CURRENT_SOURCE_DIR}/${LIB_DIR}/*")
foreach(SUBDIR ${SUBDIRS})
    if(IS_DIRECTORY ${SUBDIR})
        add_subdirectory(${SUBDIR})
        message(STATUS "Added ${SUBDIR} as subdirectory")
    endif()
endforeach()
# Uncomment the following line if you want to add external libraries
# add_subdirectory(lib/<lib_folder_name>)

# As we are compiling a shared library, we need to set the -fPIC
set_target_properties(spdlog PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(LIBRARIES_TO_LINK)

# List of libraries to link against

# Build pybind11 as an interface library
add_library(pybind11 INTERFACE)
target_include_directories(pybind11 INTERFACE lib/pybind11/include)

# Include spdlog
list(APPEND LIBRARIES_TO_LINK spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
# Include pybind11
list(APPEND LIBRARIES_TO_LINK "pybind11")

# Attempt to find GTest system-wide
find_package(GTest)
if(GTEST_FOUND)
    # GTest found system-wide, use the found package
    message(STATUS "Using system-wide GoogleTest")
    list(APPEND LIBRARIES_TO_LINK ${GTEST_LIBRARIES} pthread)
else()
    # GTest not found, fetch and compile it
    message(STATUS "GoogleTest not found system-wide, fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    # Since GTest is being fetched and compiled, add it to the libraries to link
    list(APPEND LIBRARIES_TO_LINK gtest_main pthread)
endif()


# Find Python-dev
find_package(Python3 REQUIRED COMPONENTS Development)
include_directories(${Python3_INCLUDE_DIRS})
list (APPEND LIBRARIES_TO_LINK ${Python3_LIBRARIES})

# Add the Python module build process using pybind11
# Define the source files for the Python module
set(PYTHON_MODULE_NAME "TypedShmem")  # Name of the Python module

# Create the Python module as a shared library
pybind11_add_module(${PYTHON_MODULE_NAME} MODULE ${MAIN_SRC_FILES})
target_link_libraries(${PYTHON_MODULE_NAME} PRIVATE pybind11::module ${LIBRARIES_TO_LINK})
# install(TARGETS ${PYTHON_MODULE_NAME} DESTINATION ./python)


# Link the executables with the specified libraries.
# Modern CMake will handle the inclusion of header files if properly set up.
target_link_libraries(${MAIN_EXCUTABLE} PRIVATE ${LIBRARIES_TO_LINK})
target_link_libraries(${TEST_EXCUTABLE} PRIVATE ${LIBRARIES_TO_LINK})

