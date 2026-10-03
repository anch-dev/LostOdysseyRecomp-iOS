#include "lo_semantics/string_property_initializer.h"

#include "lo_semantics/memory_move.h"

namespace lo::semantic::gpu::string_property_initializer
{
namespace
{
struct PropertySpec
{
    GuestAddress first_header;
    GuestAddress second_header;
    GuestAddress default_header;
    GuestAddress fourth_header;
    GuestAddress global_word;
};

constexpr PropertySpec kProperty = {
    // BEGIN GENERATED STRING PROPERTY PARAMETERS
    0x8336A894u, 0x8336A8D0u, 0x8336A8ACu, 0x8336A8DCu, 0x833180ACu,
    // END GENERATED STRING PROPERTY PARAMETERS
};

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}

GuestAddress EnterCopyFrame(GuestMemory& memory, GuestAddress sp,
    const FrameRegisters& frame)
{
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 112u, sp);
    return sp - 112u;
}

void LeaveCopyFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& frame)
{
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
}

std::uint64_t CopyString(GuestMemory& memory, ArrayResizeServices& services,
    std::uint64_t destination_register, std::uint64_t source_register,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    const GuestAddress entry_sp = static_cast<GuestAddress>(caller_sp);
    const GuestAddress nested_sp = EnterCopyFrame(memory, entry_sp, frame);
    frame.r30 = source_register;
    frame.r31 = destination_register;
    const GuestAddress destination = static_cast<GuestAddress>(frame.r31);
    const GuestAddress source = static_cast<GuestAddress>(frame.r30);
    const std::uint32_t count = memory.ReadU32(source + 4u);
    memory.WriteU32(destination, 0);
    memory.WriteU32(destination + 4u, count);
    memory.WriteU32(destination + 8u, count);
    ResizeArray(memory, services, destination, 2, 8);

    const std::uint32_t live_count = memory.ReadU32(destination + 4u);
    if (live_count != 0)
    {
        const std::uint32_t live_source = memory.ReadU32(source);
        const std::uint64_t live_destination = memory.ReadU32(destination);
        (void)CopyGuestMemory(memory, live_destination, live_source,
            (live_count << 1u) & 0xFFFFFFFEu, nested_sp);
    }
    const std::uint64_t result = frame.r31;
    LeaveCopyFrame(memory, entry_sp, frame);
    return result;
}

std::uint64_t InitializeTemporary(GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t destination_register,
    std::uint64_t source_register, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress entry_sp = static_cast<GuestAddress>(caller_sp);
    // 8229C8B0's public semantic API omits its generic ABI frame. Its exact
    // prologue/epilogue are visible to this caller's 144-byte stack frame.
    memory.WriteU32(entry_sp - 8u, static_cast<std::uint32_t>(frame.lr));
    WriteU64(memory, entry_sp - 24u, frame.r30);
    WriteU64(memory, entry_sp - 16u, frame.r31);
    memory.WriteU32(entry_sp - 112u, entry_sp);
    const std::uint64_t result = registered_metadata_string::InitializeString(
        memory, services, destination_register, source_register, entry_sp);
    LeaveCopyFrame(memory, entry_sp, frame);
    return result;
}

void ResetTemporary(GuestMemory& memory, ManagerFacadeServices& services,
    GuestAddress header, std::uint64_t caller_sp, FrameRegisters& frame)
{
    const GuestAddress entry_sp = static_cast<GuestAddress>(caller_sp);
    // 82298938 saves LR/r31 in its 96-byte entry frame before resetting.
    memory.WriteU32(entry_sp - 8u, static_cast<std::uint32_t>(frame.lr));
    WriteU64(memory, entry_sp - 16u, frame.r31);
    memory.WriteU32(entry_sp - 96u, entry_sp);
    (void)ResetTwoByteArray(memory, services, header, entry_sp);
    frame.lr = memory.ReadU32(entry_sp - 8u);
    frame.r31 = ReadU64(memory, entry_sp - 16u);
}

void EnterPropertyFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    // __savegprlr_28 is called with r12 holding the incoming LR.
    WriteU64(memory, sp - 40u, frame.r28);
    WriteU64(memory, sp - 32u, frame.r29);
    WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    frame.lr = 0x82496950u;
    memory.WriteU32(sp - 144u, sp);
}

void LeavePropertyFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    // __restgprlr_28 reloads from guest memory after the frame is popped.
    frame.r28 = ReadU64(memory, sp - 40u);
    frame.r29 = ReadU64(memory, sp - 32u);
    frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}

std::uint64_t InitializeProperty(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    std::uint64_t object_register, std::uint64_t source_register,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    EnterPropertyFrame(memory, caller_sp, frame);
    const std::uint64_t nested_sp = caller_sp - 144u;
    frame.r28 = 0;
    frame.r29 = 0;
    frame.r30 = source_register;
    frame.r31 = object_register;
    memory.WriteU32(static_cast<GuestAddress>(nested_sp + 80u), 0);

    frame.lr = 0x82496974u;
    (void)CopyString(memory, resize_services, frame.r31,
        0xFFFFFFFF00000000ull | kProperty.first_header, nested_sp, frame);
    frame.lr = 0x82496984u;
    (void)CopyString(memory, resize_services, frame.r31 + 12u,
        0xFFFFFFFF00000000ull | kProperty.second_header, nested_sp, frame);

    const bool has_source = static_cast<GuestAddress>(frame.r30) != 0;
    const std::uint32_t global_word = memory.ReadU32(kProperty.global_word);
    memory.WriteU32(static_cast<GuestAddress>(frame.r31) + 24u, global_word);
    std::uint64_t third_header =
        0xFFFFFFFF00000000ull | kProperty.default_header;
    if (has_source)
    {
        frame.r29 = 1;
        frame.lr = 0x824969A8u;
        third_header = InitializeTemporary(memory, resize_services,
            nested_sp + 88u, frame.r30, nested_sp, frame);
    }
    frame.lr = 0x824969C0u;
    (void)CopyString(memory, resize_services, frame.r31 + 28u,
        third_header, nested_sp, frame);

    if ((static_cast<std::uint32_t>(frame.r29) & 1u) != 0)
    {
        frame.lr = 0x824969D4u;
        ResetTemporary(memory, manager_services,
            static_cast<GuestAddress>(nested_sp + 88u),
            nested_sp, frame);
    }
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object + 40u, static_cast<std::uint32_t>(frame.r28));
    memory.WriteU32(object + 44u, static_cast<std::uint32_t>(frame.r28));
    memory.WriteU32(object + 48u, static_cast<std::uint32_t>(frame.r28));
    frame.lr = 0x824969F0u;
    (void)CopyString(memory, resize_services, frame.r31 + 52u,
        0xFFFFFFFF00000000ull | kProperty.fourth_header,
        nested_sp, frame);
    memory.WriteU32(static_cast<GuestAddress>(frame.r31) + 64u, 1);
    const std::uint64_t result = frame.r31;
    LeavePropertyFrame(memory, caller_sp, frame);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result)
{
    if (address == 0x822A06C0u)
    {
        result = CopyString(memory, resize_services, incoming_r3,
            incoming_r4, caller_sp, frame);
        return true;
    }
    if (address == 0x82496948u)
    {
        result = InitializeProperty(memory, resize_services,
            manager_services, incoming_r3, incoming_r4,
            caller_sp, frame);
        return true;
    }
    return false;
}

} // namespace lo::semantic::gpu::string_property_initializer
