#pragma once

#include "lo_semantics/object_child_float_record_chain.h"

namespace lo::semantic::gpu::object_refresh_routes
{

// This baseline delegates direct callees 822C7388, 825F41E8, and 822C42D8
// to explicit guest boundaries. The callback can
// read and change the selected PPC register/FP context and guest RAM.
class PpcBoundaryServices : public object_child_float::NativeServices
{
public:
    virtual void CallDirect(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) = 0;
    virtual void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_child_float::Registers& state) = 0;
};

struct Dependencies
{
    object_child_float_record_chain::Dependencies child_chain;
    PpcBoundaryServices& boundary;
};

// Selected-context recovery of 826099A8 and 8260A3F0. The latter invokes
// the former directly; 822C5E58 uses the accepted child/record/post chain.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state);

} // namespace lo::semantic::gpu::object_refresh_routes
