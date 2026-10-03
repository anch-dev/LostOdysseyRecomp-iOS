#pragma once

#include "lo_semantics/manager_facade.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_utf16_slice
{
struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r25 = 0, r26 = 0, r27 = 0, r28 = 0;
    std::uint64_t r29 = 0, r30 = 0, r31 = 0;
};

// Only the two manager vtable targets are external. The callback sees full
// r3-r6/SP/LR, mutable nonvolatile registers and live guest stack/RAM.
class VirtualServices
{
public:
    virtual ~VirtualServices() = default;
    virtual std::uint64_t CallMethod(GuestAddress method, GuestMemory& memory,
        std::uint64_t r3, std::uint64_t r4, std::uint64_t r5,
        std::uint64_t r6, std::uint64_t caller_sp,
        FrameRegisters& frame) = 0;
};

// 8232D240 constructs a UTF-16 array, 8232D190 slices a source array,
// 8232D040 reverses it through real nested slices and append/remove helpers,
// and 8232CED8 formats a signed 32-bit number before reversing the result.
// Own full input/result and nonvolatile frame saves are observable. Recovered
// resize/manager/UTF helpers retain their documented guest32 and volatile ABI
// boundaries; dynamic vtable target implementations remain external.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    VirtualServices& methods, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t incoming_r5,
    std::uint64_t incoming_r6, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_utf16_slice
