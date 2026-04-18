cmake_minimum_required(VERSION 3.13.4)
project(ImplicitConversionHazardAnalyzer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional: ROCm AOMP Clang paths as fallback
if(NOT DEFINED LLVM_DIR AND EXISTS "/usr/lib64/rocm/llvm/lib/cmake/llvm")
    set(LLVM_DIR "/usr/lib64/rocm/llvm/lib/cmake/llvm")
endif()
if(NOT DEFINED Clang_DIR AND EXISTS "/usr/lib64/rocm/llvm/lib/cmake/clang")
    set(Clang_DIR "/usr/lib64/rocm/llvm/lib/cmake/clang")
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Support both monolithic (clang-cpp) and component-based Clang libraries (Ubuntu WSL)
if(TARGET clang-cpp)
    set(CLANG_LIBS clang-cpp)
else()
    set(CLANG_LIBS
        clangTooling
        clangFrontend
        clangSerialization
        clangDriver
        clangParse
        clangSema
        clangAnalysis
        clangASTMatchers
        clangAST
        clangEdit
        clangLex
        clangBasic
    )
endif()

add_executable(implicit-conversion-hazard
    main.cpp
    ImplicitConversionVisitor.cpp
    RiskScorer.cpp
    ContextCollector.cpp
    FixItGenerator.cpp
)

target_include_directories(implicit-conversion-hazard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find what llvm components to link
llvm_map_components_to_libnames(LLVM_LIBS support core option)

target_link_libraries(implicit-conversion-hazard
    ${CLANG_LIBS}
    ${LLVM_LIBS}
)

# Find Clang resource directory for standard headers
find_program(CLANG_EXECUTABLE clang HINTS ${LLVM_TOOLS_BINARY_DIR} /usr/lib64/rocm/llvm/bin)
if (CLANG_EXECUTABLE)
    execute_process(
        COMMAND ${CLANG_EXECUTABLE} -print-resource-dir
        OUTPUT_VARIABLE CLANG_DEFAULT_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(CLANG_DEFAULT_RESOURCE_DIR "/usr/lib64/rocm/llvm/lib/clang/19")
endif()

target_compile_definitions(implicit-conversion-hazard PRIVATE
    DEFAULT_RESOURCE_DIR="${CLANG_DEFAULT_RESOURCE_DIR}"
)

# Set RPATH so it can find shared libraries at runtime
set_target_properties(implicit-conversion-hazard PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${LLVM_LIBRARY_DIR}"
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

# Install target
install(TARGETS implicit-conversion-hazard DESTINATION bin)
