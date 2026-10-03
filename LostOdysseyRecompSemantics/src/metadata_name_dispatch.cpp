#include "lo_semantics/metadata_name_dispatch.h"

namespace lo::semantic::gpu::metadata_name_dispatch
{
namespace
{
constexpr GuestAddress kEntry = 0x825e7600u;
constexpr GuestAddress kVtable = 0x821a7d70u;
constexpr std::uint64_t kFlagBits = 0x0400400000000000ull;

void WriteU64(GuestMemory& memory, GuestAddress address,
    std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}

void SaveFrame(GuestMemory& memory, std::uint64_t caller_sp,
    const FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    WriteU64(memory, sp - 32u, frame.r29);
    WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - 112u, sp);
}

void RestoreFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    frame.r29 = ReadU64(memory, sp - 32u);
    frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    metadata_name_record::Services& record_services,
    ArrayResizeServices& resize_services,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    DispatchServices& dispatch_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != kEntry)
        return false;
    (void)incoming_r4; // The original replaces r4 with incoming r6.

    SaveFrame(memory, caller_sp, frame);
    const std::uint64_t nested_sp = caller_sp - 112u;
    frame.r10 = 0xffffffff821a7d70ull;
    frame.r31 = incoming_r3;
    frame.r9 = 1u;
    frame.r30 = 0u;
    frame.r29 = incoming_r5;
    GuestAddress object = static_cast<GuestAddress>(frame.r31);
    WriteU64(memory, object + 76u, incoming_r7);
    memory.WriteU32(object, kVtable);
    memory.WriteU32(object + 68u, 1u);
    memory.WriteU32(object + 60u, 0u);

    frame.lr = 0x825e7650u;
    std::uint64_t lookup_result = 0;
    (void)metadata_name_lookup::Apply(0x82296d30u, memory,
        record_services, resize_services, thread_services,
        invalid_services, frame.r31 + 84u, incoming_r6,
        0u, 1u, 1u, nested_sp, frame, lookup_result);

    object = static_cast<GuestAddress>(frame.r31);
    const std::uint64_t old_flags = ReadU64(memory, object + 8u);
    const std::uint64_t receiver = memory.ReadU32(object + 40u);
    memory.WriteU32(object + 96u, static_cast<std::uint32_t>(frame.r30));
    memory.WriteU32(object + 100u, static_cast<std::uint32_t>(frame.r29));
    memory.WriteU32(object + 116u, static_cast<std::uint32_t>(frame.r30));
    WriteU64(memory, object + 8u, old_flags | kFlagBits);

    const GuestAddress vtable = memory.ReadU32(
        static_cast<GuestAddress>(receiver));
    const GuestAddress method = memory.ReadU32(vtable + 264u);
    frame.ctr = method;
    frame.lr = 0x825e7688u;
    dispatch_services.CallMethod(method & ~3u, memory, receiver,
        frame.r31, nested_sp, frame.lr, frame);
    result = frame.r31;
    RestoreFrame(memory, caller_sp, frame);
    return true;
}

} // namespace lo::semantic::gpu::metadata_name_dispatch
