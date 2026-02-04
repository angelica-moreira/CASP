cmake_minimum_required(VERSION 3.13.4)
project(CASP)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add LLVM definitions and include directories
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Add our include directory
include_directories(${CMAKE_SOURCE_DIR}/include)

# Common source files
set(CASP_SOURCES
    lib/StaticProfileExporter.cpp
)

# Build the standalone tool (links against LLVM libraries)
add_executable(llvm-sprofgen
    lib/casp-tool.cpp
    ${CASP_SOURCES}
)

# Link the tool against LLVM libraries
llvm_map_components_to_libnames(llvm_libs
    core
    support
    analysis
    irreader
    profiledata
    passes
)
target_link_libraries(llvm-sprofgen PRIVATE ${llvm_libs})

# Build the CASP plugin as a shared library
add_library(CASP MODULE
    lib/CASPPlugin.cpp
    ${CASP_SOURCES}
)

# Don't link against LLVM libraries for plugins - they should use the host compiler's symbols
# This avoids symbol conflicts
target_compile_options(CASP PRIVATE -fno-rtti)

# Set the output name
set_target_properties(CASP PROPERTIES
    PREFIX ""
    OUTPUT_NAME "CASP"
)

# On macOS, set the output extension to .dylib for compatibility
if(APPLE)
    set_target_properties(CASP PROPERTIES SUFFIX ".dylib")
endif()

# Installation
install(TARGETS CASP llvm-sprofgen
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
