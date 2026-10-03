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
    src/crt_last_error.cpp
    src/crt_lifecycle.cpp
    src/crt_stream_state.cpp
    src/crt_allocation.cpp
    src/invalid_parameter.cpp
    src/manager_init.cpp
    src/manager_facade.cpp
    src/array_release_family.cpp
    src/object_registration.cpp
    src/registered_constructor_family.cpp
    src/registered_inline_constructor.cpp
    src/registered_callback_family.cpp
    src/registered_getter_family.cpp
    src/registered_metadata_words.cpp
    src/registered_metadata_initializers.cpp
    src/registered_metadata_range.cpp
    src/registered_metadata_string.cpp
    src/registered_metadata_composed.cpp
    src/string_property_initializer.cpp
    src/owned_state_initializer.cpp
    src/instance_vtable_family.cpp
    src/instance_field_initializer_family.cpp
    src/instance_property_initializer_family.cpp
    src/instance_marker_initializer_family.cpp
    src/instance_component_initializer_family.cpp
    src/instance_scalar_initializer_family.cpp
    src/instance_ui_initializer_family.cpp
    src/instance_default_fields_family.cpp
    src/instance_flag_initializer_family.cpp
    src/instance_fill_initializer_family.cpp
    src/instance_navigation_initializer_family.cpp
    src/instance_tail_initializer_family.cpp
    src/instance_integer_sequence_family.cpp
    src/instance_fp_extension_family.cpp
    src/instance_copy_initializer_family.cpp
    src/instance_composed_initializer_family.cpp
    src/instance_linker_composed_family.cpp
    src/instance_string_initializer_family.cpp
    src/instance_property_chain_family.cpp
    src/instance_connection_initializer_family.cpp
    src/instance_owned_state_family.cpp
    src/instance_shared_metadata_initializer.cpp
    src/state_array_processing.cpp
    src/instance_allocation_composed_family.cpp
    src/instance_navigation_allocation_family.cpp
    src/instance_manager_link_family.cpp
    src/ring_reservation.cpp
    src/manager_ring_node.cpp
    src/manager_index_tables.cpp
    src/manager_index_operations.cpp
    src/manager_object_registration.cpp
    src/manager_metadata_compare.cpp
    src/manager_metadata_parsing.cpp
    src/metadata_name_record.cpp
    src/metadata_name_index.cpp
    src/metadata_name_registry.cpp
    src/metadata_name_lookup.cpp
    src/metadata_name_dispatch.cpp
    src/metadata_utf16_buffer.cpp
    src/metadata_descriptor_array.cpp
    src/metadata_member_dispatch.cpp
    src/metadata_manager_dispatch.cpp
    src/metadata_descriptor_lookup.cpp
    src/metadata_descriptor_index.cpp
    src/metadata_descriptor_membership.cpp
    src/metadata_utf16_slice.cpp
    src/metadata_descriptor_names.cpp
    src/metadata_option_match.cpp
    src/metadata_utf16_search.cpp
    src/metadata_descriptor_cursor.cpp
    src/metadata_descriptor_lifecycle.cpp
    src/object_startup.cpp
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
    src/field_arithmetic.cpp
    src/read_only_fields.cpp
    src/memory_writes.cpp
    src/single_write_fields.cpp
    src/memory_services.cpp
    src/memory_fill.cpp
    src/memory_move.cpp
    src/heap.cpp
    src/heap_free.cpp
    src/heap_lock_exit.cpp
    src/heap_block_resize.cpp
    src/heap_reallocate.cpp
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
