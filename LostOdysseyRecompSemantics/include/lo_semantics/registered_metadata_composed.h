#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu::registered_metadata_composed
{

// The vtable method is a guest callback, not a recovered implementation.
// TailCall receives the original full r3 and may update the live SP/LR.
struct TailControl
{
    std::uint64_t sp;
    std::uint64_t lr;
};

class VirtualServices
{
public:
    virtual ~VirtualServices() = default;
    virtual std::uint64_t TailCall(GuestAddress method,
        GuestMemory& memory, std::uint64_t incoming_r3,
        TailControl& control) = 0;
};

// 826D6C10: read the object's vtable+292 and tail-call its method.
// The original masks the two low method bits only when dispatching.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    VirtualServices& services, std::uint64_t incoming_r3,
    TailControl& control, std::uint64_t& result);

// 8230BAC0: ordered UTF-16 copy including the first zero code unit.
// The full incoming r3 remains the result; source_after is the live full r4.
[[nodiscard]] std::uint64_t CopyUtf16UntilNull(GuestMemory& memory,
    std::uint64_t destination, std::uint64_t source,
    std::uint64_t& source_after);

} // namespace lo::semantic::gpu::registered_metadata_composed
