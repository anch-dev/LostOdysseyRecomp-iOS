#pragma once

#include "lo_semantics/object_child_float_record_chain.h"

namespace lo::semantic::gpu::object_parent_flag_probe
{

struct Dependencies
{
    object_child_float_record_chain::Dependencies child_chain;
    object_float_record_post_chain::Dependencies record_chain;
    object_child_float::NativeServices& dynamic;
};

// 82384B20 traverses parent/child/node arrays and returns whether a resolved
// object has its +96 high bit. 822C5DC0 combines the recovered FP query and
// this predicate before a live vtable+376 guest call. Only selected ABI state
// of accepted nested lowers is covered by the original-PPC oracle.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state);

} // namespace lo::semantic::gpu::object_parent_flag_probe
