#pragma once

#include "lo_semantics/allocation_array.h"

namespace lo::semantic::gpu
{

// The manager's allocate method is an indirect guest ABI boundary.
class SpecialAllocationServices : public ArrayResizeServices
{
public:
    [[nodiscard]] virtual GuestAddress AllocateStorage(GuestAddress method,
                                                       GuestAddress manager,
                                                       std::uint32_t bytes,
                                                       std::uint32_t flags) = 0;
};

// 827C9A40. frame_base is the original 176-byte guest stack frame; its
// +80/+84 locals remain observable to callbacks and therefore live in guest memory.
[[nodiscard]] GuestAddress AllocateSpecialBlock(GuestMemory& memory,
                                                SpecialAllocationServices& services,
                                                GuestAddress allocator,
                                                std::uint32_t requested_bytes,
                                                GuestAddress frame_base);

// 827C9C60. frame_base is its post-prologue r1 (160-byte frame). The guest
// body does not define a stable r3 return value.
void FreeSpecialBlock(GuestMemory& memory, SpecialAllocationServices& services,
                      GuestAddress allocator, GuestAddress address,
                      GuestAddress frame_base);

} // namespace lo::semantic::gpu
