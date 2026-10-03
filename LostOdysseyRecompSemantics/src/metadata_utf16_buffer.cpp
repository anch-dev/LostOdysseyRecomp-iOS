#include "lo_semantics/metadata_utf16_buffer.h"

#include "lo_semantics/registered_metadata_composed.h"
#include "lo_semantics/registered_metadata_string.h"
#include "lo_semantics/registered_metadata_words.h"
#include "lo_semantics/string_property_initializer.h"

namespace lo::semantic::gpu::metadata_utf16_buffer
{
namespace
{
constexpr GuestAddress kAppend = 0x8232d378u;
constexpr GuestAddress kCompose = 0x8232d418u;

void Write64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}

void EnterAppendFrame(GuestMemory& memory, std::uint64_t caller_sp,
    const FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    Write64(memory, sp - 32u, frame.r29);
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - 112u, sp);
}

void LeaveAppendFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    frame.r29 = Read64(memory, sp - 32u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}

std::uint64_t AppendUtf16(GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t header_register,
    std::uint64_t source_register, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    EnterAppendFrame(memory, caller_sp, frame);
    frame.r30 = source_register;
    frame.r31 = header_register;
    const GuestAddress header = static_cast<GuestAddress>(frame.r31);
    if (memory.ReadU16(static_cast<GuestAddress>(frame.r30)) != 0)
    {
        const std::uint32_t count = memory.ReadU32(header + 4u);
        if (count != 0)
            frame.r29 = std::uint64_t{count} - 1u;
        frame.lr = count == 0 ? 0x8232d3e8u : 0x8232d3b0u;
        const std::uint64_t length =
            registered_metadata_string::Utf16Length(memory, frame.r30);
        const std::uint64_t added = count == 0 ? length + 1u : length;
        frame.lr = count == 0 ? 0x8232d3fcu : 0x8232d3c4u;
        (void)registered_metadata_words::AddArrayElements(memory,
            resize_services, header, static_cast<std::uint32_t>(added), 2u, 8u);

        // The callback can replace storage and the source contents. The old
        // terminator offset is based on the count observed before the call.
        const GuestAddress storage = memory.ReadU32(header);
        const GuestAddress offset = count == 0 ? 0u :
            (static_cast<std::uint32_t>(frame.r29) << 1u) & 0xfffffffeu;
        frame.lr = count == 0 ? 0x8232d408u : 0x8232d3d8u;
        std::uint64_t after = 0;
        (void)registered_metadata_composed::CopyUtf16UntilNull(memory,
            std::uint64_t{storage} + offset, frame.r30, after);
    }
    const std::uint64_t result = frame.r31;
    LeaveAppendFrame(memory, caller_sp, frame);
    return result;
}

void EnterComposeFrame(GuestMemory& memory, std::uint64_t caller_sp,
    const FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 128u, sp);
}

void LeaveComposeFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
}

std::uint64_t CopyHeader(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    std::uint64_t destination, std::uint64_t source,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    string_property_initializer::FrameRegisters lower{
        frame.lr, frame.r28, frame.r29, frame.r30, frame.r31};
    std::uint64_t result = 0;
    (void)string_property_initializer::Apply(0x822a06c0u, memory,
        resize_services, manager_services, destination, source,
        caller_sp, lower, result);
    frame.lr = lower.lr;
    frame.r28 = lower.r28;
    frame.r29 = lower.r29;
    frame.r30 = lower.r30;
    frame.r31 = lower.r31;
    return result;
}

std::uint64_t ComposeUtf16(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    std::uint64_t destination_register, std::uint64_t initial_header,
    std::uint64_t appended_source, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    EnterComposeFrame(memory, caller_sp, frame);
    const std::uint64_t sp = caller_sp - 128u;
    const GuestAddress temporary = static_cast<GuestAddress>(sp + 80u);
    frame.r31 = destination_register;
    frame.r30 = appended_source;

    frame.lr = 0x8232d43cu;
    (void)CopyHeader(memory, resize_services, manager_services,
        sp + 80u, initial_header, sp, frame);
    frame.lr = 0x8232d444u;
    const std::uint64_t appended = AppendUtf16(memory, resize_services,
        sp + 80u, frame.r30, sp, frame);
    frame.lr = 0x8232d450u;
    (void)CopyHeader(memory, resize_services, manager_services,
        frame.r31, appended, sp, frame);

    const std::uint32_t capacity = memory.ReadU32(temporary + 8u);
    memory.WriteU32(temporary + 4u, 0);
    if (capacity != 0)
    {
        memory.WriteU32(temporary + 8u, 0);
        frame.lr = 0x8232d478u;
        ResizeArray(memory, resize_services, temporary, 2u, 8u);
    }
    // 82298A98's own 96-byte entry frame is observable before its recovered
    // ReleaseTwoByteArray algorithm, including an aliased saved r31 or LR.
    const GuestAddress nested = static_cast<GuestAddress>(sp);
    memory.WriteU32(nested - 8u, 0x8232d480u);
    Write64(memory, nested - 16u, frame.r31);
    memory.WriteU32(nested - 96u, nested);
    frame.lr = 0x8232d480u;
    (void)ReleaseTwoByteArray(memory, manager_services, temporary, nested);
    frame.lr = memory.ReadU32(nested - 8u);
    frame.r31 = Read64(memory, nested - 16u);

    const std::uint64_t result = frame.r31;
    LeaveComposeFrame(memory, caller_sp, frame);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case kAppend:
        result = AppendUtf16(memory, resize_services, incoming_r3,
            incoming_r4, caller_sp, frame);
        return true;
    case kCompose:
        result = ComposeUtf16(memory, resize_services, manager_services,
            incoming_r3, incoming_r4, incoming_r5, caller_sp, frame);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::metadata_utf16_buffer
