#pragma once

#include "lo_semantics/fallback_resize.h"

namespace lo::semantic::gpu
{

// 827C4FA0 drains the cached pointers through manager vtable slot +12 and
// resets the live count. It does not grow capacity or replace the backing store.
// frame_base is the post-prologue 144-byte frame; ABI saves are external.
[[nodiscard]] std::uint64_t FlushPointerVector(GuestMemory& memory,
    ManagerLockServices& locks, FallbackResizeServices& manager,
    std::uint64_t vector_register, GuestAddress frame_base);

} // namespace lo::semantic::gpu
