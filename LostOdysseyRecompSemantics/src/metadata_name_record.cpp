#include "lo_semantics/metadata_name_record.h"

#include "lo_semantics/registered_metadata_composed.h"
#include "lo_semantics/registered_metadata_string.h"

namespace lo::semantic::gpu::metadata_name_record
{
namespace
{
constexpr GuestAddress kEntry = 0x823f7b08u;
constexpr GuestAddress kManagerGlobal = 0x8330b608u;

void Store64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t Load64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}

void SaveFrame(GuestMemory& memory, GuestAddress sp,
    const FrameRegisters& frame)
{
    Store64(memory, sp - 48u, frame.r27);
    Store64(memory, sp - 40u, frame.r28);
    Store64(memory, sp - 32u, frame.r29);
    Store64(memory, sp - 24u, frame.r30);
    Store64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
}

void RestoreFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& frame)
{
    frame.r27 = Load64(memory, sp - 48u);
    frame.r28 = Load64(memory, sp - 40u);
    frame.r29 = Load64(memory, sp - 32u);
    frame.r30 = Load64(memory, sp - 24u);
    frame.r31 = Load64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}

// The already recovered 827C5F38 algorithm begins after its 112-byte
// prologue. Compose the actual caller-visible ABI stores around that model.
void InitializeNestedManager(GuestMemory& memory, Services& services,
    std::uint64_t full_sp, FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(full_sp);
    memory.WriteU32(sp - 8u, 0x823f7b40u);
    Store64(memory, sp - 24u, frame.r30);
    Store64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 112u, sp);
    frame.lr = 0x823f7b40u;
    (void)InitializeManager(memory, services, sp - 112u);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r30 = Load64(memory, sp - 24u);
    frame.r31 = Load64(memory, sp - 16u);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    Services& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t incoming_r5,
    std::uint64_t incoming_r6, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != kEntry)
        return false;
    (void)incoming_r5;

    const GuestAddress caller = static_cast<GuestAddress>(caller_sp);
    SaveFrame(memory, caller, frame);
    const std::uint64_t full_sp = caller_sp - 128u;
    memory.WriteU32(static_cast<GuestAddress>(full_sp), caller);
    frame.r30 = incoming_r3;
    frame.r29 = incoming_r4;
    frame.r28 = incoming_r6;

    const std::uint64_t length =
        registered_metadata_string::Utf16Length(memory, incoming_r3);
    frame.r31 = static_cast<std::uint64_t>(std::int64_t{-2093940736});
    frame.r27 = static_cast<std::uint32_t>((length + 9u) << 1u);
    GuestAddress manager = memory.ReadU32(kManagerGlobal);
    if (manager == 0)
    {
        InitializeNestedManager(memory, services, full_sp, frame);
        manager = memory.ReadU32(kManagerGlobal);
    }

    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 4u) & ~GuestAddress{3};
    frame.lr = 0x823f7b60u;
    const std::uint64_t allocation = services.AllocateRecord(method, memory,
        manager, frame.r27, 8u, full_sp, frame);
    frame.r31 = allocation;

    const GuestAddress record = static_cast<GuestAddress>(allocation);
    memory.WriteU32(record, static_cast<std::uint32_t>(frame.r29));
    Store64(memory, record + 4u, 0);
    memory.WriteU32(record + 12u, static_cast<std::uint32_t>(frame.r28));
    std::uint64_t source_after = 0;
    (void)registered_metadata_composed::CopyUtf16UntilNull(memory,
        allocation + 16u, frame.r30, source_after);
    result = frame.r31;
    RestoreFrame(memory, caller, frame);
    return true;
}

} // namespace lo::semantic::gpu::metadata_name_record
