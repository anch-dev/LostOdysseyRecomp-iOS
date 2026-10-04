#pragma once

#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/legacy_token_float_cursor.h"
#include "lo_semantics/metadata_name_record.h"
#include "lo_semantics/registered_metadata_words.h"

#include <array>
#include <cstdint>

namespace lo::semantic::gpu::legacy_config_command_dispatch
{
struct Registers
{
    crt_stream_operations::Registers integer{};
    std::array<std::uint64_t, 32> fpr_bits{};
    std::uint32_t cached_fp_control = 0;
};

class GuestBoundaries
{
public:
    virtual ~GuestBoundaries() = default;
    // The five not-yet-recovered guest functions receive selected live GPR,
    // CR0/CR6, FPR and cached host FP-control state. They may mutate these
    // fields and ordinary guest RAM; other PPC context is outside this ABI.
    virtual void Call(GuestAddress entry, GuestMemory& memory,
        Registers& registers) = 0;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

struct Dependencies
{
    CrtThreadDataServices& thread;
    InvalidParameterServices& invalid;
    metadata_name_record::Services& records;
    ArrayResizeServices& arrays;
    legacy_token_float_cursor::NumberServices& numbers;
    GuestBoundaries& guests;
};

// The complete 82296008 command dispatch body. Accepted lower functions are
// composed at their call sites; 82713828, 82479058, 824790A8, 82296B00 and
// 82295DA8 remain explicit PPC guest boundaries. This model covers ordinary
// guest RAM and selected context, not faults, MMIO or concurrent mutation.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_config_command_dispatch
