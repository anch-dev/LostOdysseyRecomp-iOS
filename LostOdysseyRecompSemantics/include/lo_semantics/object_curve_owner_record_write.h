#pragma once

#include "lo_semantics/object_curve_record_displacement.h"

namespace lo::semantic::gpu::object_curve_owner_record_write
{
using Registers = object_curve_record_displacement::Registers;
class NativeServices : public object_curve_record_displacement::NativeServices
{
public:
    // 8242C998 cold path: the exact direct targets 82629F98/8262A050.
    virtual void CallColdDirect(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// Actual 8262AFB0 record write, 8260CF60 owner lookup, and warm 8242C998
// singleton. Accepted vector/scalar callees remain real lower bodies; the
// singleton cold path and nested vtable+268 stay explicit boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::object_curve_owner_record_write
