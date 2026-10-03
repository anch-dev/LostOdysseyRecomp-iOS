#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// The manager initialization and indirect resize method remain guest ABI boundaries.
class ArrayResizeServices
{
public:
    virtual ~ArrayResizeServices() = default;

    virtual void InitializeManager() = 0;
    [[nodiscard]] virtual GuestAddress ResizeStorage(GuestAddress method,
                                                     GuestAddress manager,
                                                     GuestAddress old_storage,
                                                     std::uint32_t bytes,
                                                     std::uint32_t argument) = 0;
};

// 8229F678. These helpers have no stable semantic return value in the PPC body.
void ResizeArray(GuestMemory& memory, ArrayResizeServices& services,
                 GuestAddress array, std::uint32_t element_size,
                 std::uint32_t argument);

// 82298AF8. frame_base is its post-prologue guest r1 (128-byte frame),
// observable by the forward memory-copy tail call's stack spill.
void RemoveArrayRange(GuestMemory& memory, ArrayResizeServices& services,
                      GuestAddress array, std::uint32_t first,
                      std::uint32_t count, std::uint32_t element_size,
                      std::uint32_t argument, GuestAddress frame_base);

} // namespace lo::semantic::gpu
