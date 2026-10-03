cmake_minimum_required(VERSION 3.28)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LostOdysseyRecompSemantics LANGUAGES CXX)
endif()

# This target does not need the private XEX, generated PPC, a renderer, or SDKs.
add_library(LostOdysseyRecompSemantics STATIC
    src/query.cpp
    src/query_pool.cpp
    src/query_release.cpp
    src/allocation.cpp
    src/allocation_backend.cpp
    src/allocation_array.cpp
    src/special_allocation.cpp
    src/raw_allocation.cpp
    src/allocation_failure.cpp
    src/crt_thread_data.cpp
    src/crt_allocation.cpp
    src/manager_init.cpp
    src/manager_construction.cpp
    src/manager_storage.cpp
    src/manager_lock.cpp
    src/manager_allocate.cpp
    src/manager_resize.cpp
    src/fallback_resize.cpp
    src/leaf_family.cpp
    src/accessor_family.cpp
    src/integer_leaf.cpp
    src/pointer_vector.cpp
    src/field_bits.cpp
    src/pointer_fields.cpp
    src/global_assignments.cpp
    src/field_operations.cpp
    src/memory_services.cpp
    src/memory_fill.cpp
    src/memory_move.cpp
    src/heap.cpp
    src/heap_free.cpp
    src/heap_allocate.cpp
    src/heap_growth.cpp
    src/heap_decommit.cpp
    src/heap_ranges.cpp
    src/heap_segment.cpp
    src/thread_state.cpp
    src/cache.cpp)
add_library(LO::Semantics ALIAS LostOdysseyRecompSemantics)
target_compile_features(LostOdysseyRecompSemantics PUBLIC cxx_std_20)
target_include_directories(LostOdysseyRecompSemantics PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(LostOdysseyRecompSemantics PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(LostOdysseyRecompSemantics PRIVATE /W4 /WX)
else()
    target_compile_options(LostOdysseyRecompSemantics PRIVATE -Wall -Wextra -Werror)
endif()
