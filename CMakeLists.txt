# Minimum required version of CMake
cmake_minimum_required(VERSION 3.15...3.27)

# Set the project name and specify the language as C/C++
project(
  ${SKBUILD_PROJECT_NAME}
  VERSION ${SKBUILD_PROJECT_VERSION}
  LANGUAGES CXX)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,/lib:/usr/lib")

# Set and enforce the C++11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# For courses (need to be strict)
# set(CMAKE_C_EXTENSIONS ON)  # This enables the GNU extensions, equivalent to -std=gnu11
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -Wall -Wextra -Werror -pedantic -Wno-unused-result -Wconversion") # More warnings

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Flags to generate intermediate files
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

# Debug and release optimization flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")

# set(CMAKE_BUILD_TYPE Debug)
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# Include the /include directory for header files
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# Gather all source files from the /src directory
file(GLOB MAIN_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
# Gather all test source files from the /test directory
file(GLOB TEST_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/test/*.c")

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
# Include pthread
list(APPEND LIBRARIES_TO_LINK "rt" "pthread")
# Include GTest
list(APPEND LIBRARIES_TO_LINK "gmock" "gmock_main" "gtest" "gtest_main")

# Set conda environment path
set(PYTHON_ENV_PATH "/home/Shared/conda_envs/TypedShmemTest") # Get with `conda env list`
# set(PYTHON_ENV_PATH "/home/chkxwlyh/miniconda3/envs/SimRobotTraining") # Get with `conda env list`
# Set Python executable
if(WIN32)
    set(Python_EXECUTABLE "${PYTHON_ENV_PATH}/python.exe")
else()
    set(Python_EXECUTABLE "${PYTHON_ENV_PATH}/bin/python")
endif()
message(STATUS "Python executable: ${Python_EXECUTABLE}")

execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')"
    OUTPUT_VARIABLE Python_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Detected Python version: ${Python_VERSION}")

# Set Python lib
if(WIN32)
    set(Python_LIBRARY "${PYTHON_ENV_PATH}/libs/python${Python_VERSION}.lib")
else()
    set(Python_LIBRARY "${PYTHON_ENV_PATH}/lib/libpython${Python_VERSION}.so")
endif()
set(Python_INCLUDE_DIR "${PYTHON_ENV_PATH}/include/python${Python_VERSION}")

# Find Python-dev
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include_directories(${Python3_INCLUDE_DIRS})
list (APPEND LIBRARIES_TO_LINK ${Python3_LIBRARIES})

# Add the Python module build process using pybind11
# Define the source files for the Python module
set(PYTHON_MODULE_NAME "TypedShmem")  # Name of the Python module

# Create the Python module as a shared library
python3_add_library(${PYTHON_MODULE_NAME} MODULE ${MAIN_SRC_FILES} WITH_SOABI)
target_link_libraries(${PYTHON_MODULE_NAME} PRIVATE pybind11::headers ${LIBRARIES_TO_LINK})
target_compile_definitions(${PYTHON_MODULE_NAME} PRIVATE VERSION_INFO=${PROJECT_VERSION})
install(TARGETS ${PYTHON_MODULE_NAME} DESTINATION ${PYTHON_MODULE_NAME})

# install(TARGETS ${PYTHON_MODULE_NAME} DESTINATION ./python)


# Link the executables with the specified libraries.
# Modern CMake will handle the inclusion of header files if properly set up.
target_link_libraries(${MAIN_EXCUTABLE} PRIVATE ${LIBRARIES_TO_LINK})
target_link_libraries(${TEST_EXCUTABLE} PRIVATE ${LIBRARIES_TO_LINK})

