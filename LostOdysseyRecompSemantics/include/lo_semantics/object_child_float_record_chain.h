#pragma once

#include "lo_semantics/object_child_float.h"
#include "lo_semantics/object_float_record_post_chain.h"

namespace lo::semantic::gpu::object_child_float_record_chain
{

struct Dependencies
{
    object_child_float::NativeServices& child_external;
    object_float_record_post_chain::Dependencies record;
};

// Validation-only composition of 822C5E58/822C5F28 through the recovered
// 82384C08 -> 8242D038 -> 82627230 chain. Non-record guest virtual calls
// remain live boundaries; generic volatile ABI state is not claimed.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state);

} // namespace lo::semantic::gpu::object_child_float_record_chain
