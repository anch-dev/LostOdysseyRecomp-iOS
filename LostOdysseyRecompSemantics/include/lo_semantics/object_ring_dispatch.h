#pragma once

#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/float_triplet_transfer.h"
#include "lo_semantics/ring_reservation.h"

namespace lo::semantic::gpu::object_ring_dispatch
{
struct Registers : crt_stream_operations::Registers
{
    std::uint64_t f0_bits = 0;
    std::uint32_t cached_fp_control = 0;
};

class DynamicServices
{
public:
    virtual ~DynamicServices() = default;
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

struct Dependencies
{
    ring_reservation::SynchronizationServices& synchronization;
    float_triplet_transfer::NativeServices& fp;
    DynamicServices& dynamic;
};

// 823EFA30 performs three ordered virtual dispatches and publishes three
// loaded floats. 82372FB0 chooses its accepted ring-reservation lower or
// the actual 823EFA30 lower, then closes the caller frame.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
}
