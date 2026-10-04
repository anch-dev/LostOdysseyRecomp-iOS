#pragma once
#include "lo_semantics/manager_init_context.h"
#include "lo_semantics/object_metadata_storage.h"
namespace lo::semantic::gpu::object_metadata_manager_chain
{
struct Dependencies
{
    manager_init_context::PpcBoundaryServices& manager;
    object_metadata_storage::PpcBoundaryServices& allocator;
};
// Actual metadata append/grow/reallocation with the selected-context
// 827C5F38 manager initializer. Remaining guest allocation and construction
// and virtual implementations are explicitly mutable boundaries.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies,
    object_child_float::Registers&);
}
