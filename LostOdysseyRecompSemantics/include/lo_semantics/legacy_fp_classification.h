#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_fp_classification
{
struct Registers
{
    crt_stream_operations::Registers integer{};
    std::uint64_t f0_bits = 0;
    std::uint64_t f1_bits = 0;
    std::uint32_t cached_fp_control = 0;
};

class HostFpServices
{
public:
    virtual ~HostFpServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// Four complete adjacent CRT binary64 classifiers. 82B7DFC0 calls the
// recovered 82B82340 lower directly. Guest addresses use low-32 bits;
// nonzero upper GPR/frame bits and ordered ordinary-RAM accesses remain live.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    HostFpServices& services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_fp_classification
