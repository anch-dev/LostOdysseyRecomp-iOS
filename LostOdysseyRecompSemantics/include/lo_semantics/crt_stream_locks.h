#pragma once

#include "lo_semantics/crt_allocation.h"
#include "lo_semantics/crt_stream_index_unlock.h"
#include "lo_semantics/crt_stream_pointer_unlock.h"
#include "lo_semantics/crt_stream_state.h"
#include "lo_semantics/crt_thread_data.h"
#include "lo_semantics/invalid_parameter.h"
#include "lo_semantics/raw_allocation.h"

#include <cstdint>

namespace lo::semantic::gpu::crt_stream_locks
{

struct Condition
{
    bool lt = false;
    bool gt = false;
    bool eq = false;
    bool so = false;
    bool operator==(const Condition&) const = default;
};

// Selected live PPC state needed by these four entries and their native edges.
struct Registers
{
    std::uint64_t sp = 0, lr = 0, ctr = 0;
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0;
    std::uint64_t r8 = 0, r9 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0;
    std::uint64_t r28 = 0, r29 = 0, r30 = 0, r31 = 0;
    Condition cr0, cr6;
    std::uint8_t xer_ca = 0, xer_so = 0;
    bool operator==(const Registers&) const = default;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    // Imported RtlEnterCriticalSection is void. Keep its live r3, full SP/LR,
    // nonvolatile registers and memory mutable for a returning test boundary.
    virtual void EnterCriticalSection(GuestMemory& memory,
        Registers& registers) = 0;
    // 82B7BED8's real loaded function pointer, masked by ~3 before dispatch.
    // A real fatal handler may not return; a returning one exposes continuation.
    virtual void CallFatal(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

struct Dependencies
{
    CrtThreadDataServices& thread;
    InvalidParameterServices& invalid;
    RawAllocationServices& raw;
    CrtAllocationServices& allocation;
    crt_stream_state::NativeServices& initialization;
    crt_stream_pointer_unlock::NativeServices& pointer_unlock;
    crt_stream_index_unlock::NativeServices& index_unlock;
    NativeServices& native;
};

// 82B81B28/82B819E8 form one recursive lock-initialization component.
// 82B7BED8 supplies its fatal path; 82B862F8 supplies per-stream locks.
// The accepted lower semantic models retain their documented generic ABI
// limits; this adapter preserves the selected live state at the four entries.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);

} // namespace lo::semantic::gpu::crt_stream_locks
