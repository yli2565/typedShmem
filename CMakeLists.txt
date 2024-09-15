# Minimum required version of CMake
cmake_minimum_required(VERSION 3.15...3.27)

# Set the project name and specify the language as C/C++
if(NOT DEFINED SKBUILD_PROJECT_NAME)
    set(SKBUILD_PROJECT_NAME "TypedShmem")
endif()

if(NOT DEFINED SKBUILD_PROJECT_VERSION)
    set(SKBUILD_PROJECT_VERSION "0.0.1")
endif()

set(PROJECT_NAME ${SKBUILD_PROJECT_NAME})
set(PROJECT_VERSION ${SKBUILD_PROJECT_VERSION})

message(STATUS "Start building project: ${PROJECT_NAME} - ${PROJECT_VERSION}")
project(
    ${PROJECT_NAME}
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,/lib:/usr/lib")

# Set and enforce the C++11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# For courses (need to be strict)
# set(CMAKE_C_EXTENSIONS ON)  # This enables the GNU extensions, equivalent to -std=gnu11
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wno-unused-result -Wconversion") # More warnings
# set(CMAKE_CXX_EXTENSIONS ON)  # This enables the GNU extensions, equivalent to -std=gnu11
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-result -Wconversion") # More warnings


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

# < ====================================== Locate libraries ======================================= >
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

# Find python in current environment
# Set conda environment path
set(PYTHON_ENV_PATH "/home/Shared/conda_envs/TypedShmemTest") # Get with `conda env list`
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

# spdlog
# As we are compiling spdlog - a shared library, we need to set the -fPIC
set_target_properties(spdlog PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Find Python-dev
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include_directories(${Python3_INCLUDE_DIRS})

# Get GTest
# find_package(GTest REQUIRED)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(BASIC_DEPENDENT_LIBS)
set(PYTHON_PACKAGE_DEPENDENT_LIBS)
set(CXX_TEST_DEPENDENT_LIBS)

# Basic dependent libraries
list(APPEND BASIC_DEPENDENT_LIBS "rt" "pthread")
list(APPEND BASIC_DEPENDENT_LIBS spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
list(APPEND BASIC_DEPENDENT_LIBS ${Python3_LIBRARIES})
list(APPEND BASIC_DEPENDENT_LIBS "pybind11::headers")

# Python dependent libraries
# list(APPEND PYTHON_PACKAGE_DEPENDENT_LIBS ${BASIC_DEPENDENT_LIBS})
# Currently NONE

# CXX test dependent libraries
# list(APPEND CXX_TEST_DEPENDENT_LIBS ${BASIC_DEPENDENT_LIBS})
list(APPEND CXX_TEST_DEPENDENT_LIBS "gmock" "gmock_main" "gtest" "gtest_main")

# < ====================================== Collect source files ======================================= >
# Gather all source files from the /src directory
file(GLOB CXX_LIB_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Gather pybind11 specific source files
file(GLOB PYTHON_ONLY_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/python/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/python/*.c")

# Gather all test source files from the /test directory
file(GLOB TEST_ONLY_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/test/*.c")

# < ====================================== Setup targets ======================================= >
# Target 1 - CXX lib (no main, static)
set(CXX_LIB_NAME "${PROJECT_NAME}Lib")
add_library(${CXX_LIB_NAME} STATIC ${CXX_LIB_SRC_FILES})
target_compile_options(${CXX_LIB_NAME} PRIVATE -Wall -Wextra -pedantic -Wno-unused-result -Wconversion)
target_include_directories(${CXX_LIB_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${CXX_LIB_NAME} PRIVATE ${BASIC_DEPENDENT_LIBS})

# Target 2 - CXX test executable
set(TEST_EXCUTABLE "test")
add_executable(${TEST_EXCUTABLE} ${TEST_ONLY_SRC_FILES})
target_link_libraries(${TEST_EXCUTABLE} PRIVATE ${CXX_LIB_NAME} ${BASIC_DEPENDENT_LIBS} ${CXX_TEST_DEPENDENT_LIBS})

# Target 3 - Pybind11 lib
set(PYTHON_MODULE_NAME ${PROJECT_NAME}) # Name of the Python module
python3_add_library(${PYTHON_MODULE_NAME} MODULE ${PYTHON_ONLY_SRC_FILES} WITH_SOABI)
# target_compile_options(${PYTHON_MODULE_NAME} PRIVATE -Wall -Wextra -pedantic -Wno-unused-result -Wconversion)
target_include_directories(${PYTHON_MODULE_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/python")
target_link_libraries(${PYTHON_MODULE_NAME} PRIVATE ${CXX_LIB_NAME} ${BASIC_DEPENDENT_LIBS} ${PYTHON_PACKAGE_DEPENDENT_LIBS})
target_compile_definitions(${PYTHON_MODULE_NAME} PRIVATE VERSION_INFO=${PROJECT_VERSION})
install(TARGETS ${PYTHON_MODULE_NAME} DESTINATION ${PYTHON_MODULE_NAME})

