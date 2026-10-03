#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_copy_initializer_family
{

struct EntryAbi
{
    std::uint64_t caller_sp;
    std::uint64_t incoming_lr;
    std::uint64_t incoming_r30;
    std::uint64_t incoming_r31;
};

struct Result
{
    std::uint64_t r3;
    std::uint64_t lr;
    std::uint64_t r30;
    std::uint64_t r31;
};

// SceneCapture2DComponent initializes its vtable/flags and copies two 64-byte
// defaults using CopyGuestMemory. The second copy's live stack reload supplies
// r3; it can differ from object+208 when a destination aliases the saved r3.
// The 112-byte frame is written even for low-word-null objects. Saved LR/r30/
// r31 are reloaded after the copies, retaining object/stack aliases.
// Ordinary memory only; volatile registers and fault/MMIO widths are excluded.
// An unknown address leaves memory and result untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, const EntryAbi& abi, Result& result);

} // namespace lo::semantic::gpu::instance_copy_initializer_family
