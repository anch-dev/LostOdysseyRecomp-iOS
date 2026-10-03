#include "lo_semantics/metadata_descriptor_names.h"

#include "lo_semantics/metadata_utf16_buffer.h"
#include "lo_semantics/registered_metadata_string.h"

namespace lo::semantic::gpu::metadata_descriptor_names
{
namespace
{
constexpr GuestAddress kPackedName = 0x822a9668u;
constexpr GuestAddress kOwnerName = 0x823ac8e0u;
constexpr GuestAddress kNameTableGlobal = 0x833690d0u;
constexpr std::uint64_t kEmpty = 0xffffffff821a83d0ull;
constexpr std::uint64_t kSuffixSeparator = 0xffffffff8200375cull;
constexpr std::uint64_t kOwnerSeparator = 0xffffffff82000e00ull;
constexpr std::uint64_t kUnnamed = 0xffffffff8218d870ull;
constexpr std::uint64_t kStopped = 0xffffffff8201f9f4ull;

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

void Enter(GuestMemory& memory, std::uint64_t caller_sp,
    const FrameRegisters& frame, bool save_r28)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (save_r28) Write64(memory, sp - 40u, frame.r28);
    Write64(memory, sp - 32u, frame.r29);
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - 160u, sp);
}

void Leave(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, bool save_r28)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (save_r28) frame.r28 = Read64(memory, sp - 40u);
    frame.r29 = Read64(memory, sp - 32u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}

std::uint64_t CopyRecordText(GuestMemory& memory,
    ArrayResizeServices& arrays, std::uint64_t destination,
    std::uint64_t source, std::uint64_t caller_sp, FrameRegisters& frame)
{
    // 8229C8B0's generic public API omits its own 112-byte ABI frame.
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 112u, sp);
    const std::uint64_t result =
        registered_metadata_string::InitializeString(memory, arrays,
            destination, source, sp);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    return result;
}

std::uint64_t AssignText(GuestMemory& memory, ArrayResizeServices& arrays,
    std::uint64_t destination, std::uint64_t source,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 112u, sp);
    const std::uint64_t result =
        registered_metadata_string::AssignString(memory, arrays,
            destination, source, sp);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    return result;
}

void ResetTemporary(GuestMemory& memory, ManagerFacadeServices& manager,
    GuestAddress header, std::uint64_t caller_sp, FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 96u, sp);
    (void)ResetTwoByteArray(memory, manager, header, sp);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r31 = Read64(memory, sp - 16u);
}

std::uint64_t BufferCall(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    std::uint64_t destination, std::uint64_t source,
    std::uint64_t suffix, std::uint64_t caller_sp, FrameRegisters& frame)
{
    metadata_utf16_buffer::FrameRegisters lower{frame.lr, frame.r28,
        frame.r29, frame.r30, frame.r31};
    std::uint64_t result = 0;
    (void)metadata_utf16_buffer::Apply(address, memory, arrays, manager,
        destination, source, suffix, caller_sp, lower, result);
    frame.lr = lower.lr;
    frame.r28 = lower.r28;
    frame.r29 = lower.r29;
    frame.r30 = lower.r30;
    frame.r31 = lower.r31;
    return result;
}

std::uint64_t FormatSuffix(GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    metadata_utf16_slice::VirtualServices& methods,
    std::uint64_t destination, std::uint64_t number,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    metadata_utf16_slice::FrameRegisters lower{frame.lr, frame.r25,
        frame.r26, frame.r27, frame.r28, frame.r29, frame.r30, frame.r31};
    std::uint64_t result = 0;
    (void)metadata_utf16_slice::Apply(0x8232ced8u, memory, arrays, manager,
        methods, destination, number, 0, 0, caller_sp, lower, result);
    frame.lr = lower.lr;
    frame.r25 = lower.r25;
    frame.r26 = lower.r26;
    frame.r27 = lower.r27;
    frame.r28 = lower.r28;
    frame.r29 = lower.r29;
    frame.r30 = lower.r30;
    frame.r31 = lower.r31;
    return result;
}

std::uint64_t PackedName(GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    metadata_utf16_slice::VirtualServices& methods,
    std::uint64_t destination, std::uint64_t packed,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, false);
    const std::uint64_t sp = caller_sp - 160u;
    const GuestAddress packed_address = static_cast<GuestAddress>(packed);
    const std::uint32_t suffix = memory.ReadU32(packed_address + 4u);
    frame.r29 = destination;
    if (suffix == 0u)
    {
        const GuestAddress index = memory.ReadU32(packed_address);
        const GuestAddress table = memory.ReadU32(kNameTableGlobal);
        const GuestAddress record = memory.ReadU32(
            table + ((index << 2u) & 0xfffffffcu));
        frame.lr = 0x822a9738u;
        (void)CopyRecordText(memory, arrays, frame.r29,
            std::uint64_t{record} + 16u, sp, frame);
    }
    else
    {
        const GuestAddress index = memory.ReadU32(packed_address);
        frame.r30 = (index << 2u) & 0xfffffffcu;
        frame.r31 = memory.ReadU32(kNameTableGlobal);
        frame.lr = 0x822a96a0u;
        const std::uint64_t formatted = FormatSuffix(memory, arrays,
            manager, methods,
            sp + 112u, std::uint64_t{suffix} - 1u, sp, frame);
        const GuestAddress live_record = memory.ReadU32(
            static_cast<GuestAddress>(frame.r31) +
            static_cast<GuestAddress>(frame.r30));
        frame.r31 = formatted;
        frame.r30 = kSuffixSeparator;
        frame.lr = 0x822a96bcu;
        (void)CopyRecordText(memory, arrays, sp + 96u,
            std::uint64_t{live_record} + 16u, sp, frame);
        frame.lr = 0x822a96ccu;
        const std::uint64_t separator = BufferCall(0x8232d418u, memory,
            arrays, manager, sp + 80u, sp + 96u, frame.r30, sp, frame);
        const GuestAddress formatted_header =
            static_cast<GuestAddress>(frame.r31);
        const std::uint64_t digits =
            memory.ReadU32(formatted_header + 4u) == 0u ?
            kEmpty : memory.ReadU32(formatted_header);
        frame.lr = 0x822a96f4u;
        (void)BufferCall(0x8232d418u, memory, arrays, manager,
            frame.r29, separator, digits, sp, frame);
        frame.lr = 0x822a96fcu;
        ResetTemporary(memory, manager, static_cast<GuestAddress>(sp + 80u),
            sp, frame);
        frame.lr = 0x822a9704u;
        ResetTemporary(memory, manager, static_cast<GuestAddress>(sp + 96u),
            sp, frame);
        frame.lr = 0x822a970cu;
        ResetTemporary(memory, manager, static_cast<GuestAddress>(sp + 112u),
            sp, frame);
    }
    const std::uint64_t result = frame.r29;
    Leave(memory, caller_sp, frame, false);
    return result;
}

std::uint64_t OwnerName(GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    metadata_utf16_slice::VirtualServices& methods,
    std::uint64_t destination, std::uint64_t owner,
    std::uint64_t stop, std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, true);
    const std::uint64_t sp = caller_sp - 160u;
    frame.r30 = destination;
    frame.r29 = owner;
    const GuestAddress output = static_cast<GuestAddress>(frame.r30);
    memory.WriteU32(output, 0);
    memory.WriteU32(output + 4u, 0);
    memory.WriteU32(output + 8u, 0);
    if (static_cast<GuestAddress>(frame.r29) == static_cast<GuestAddress>(stop) ||
        static_cast<GuestAddress>(frame.r29) == 0u)
    {
        frame.lr = 0x823ac9e8u;
        (void)AssignText(memory, arrays, frame.r30, kStopped, sp, frame);
    }
    else
    {
        frame.r28 = kEmpty;
        const GuestAddress parent = memory.ReadU32(
            static_cast<GuestAddress>(frame.r29) + 40u);
        if (parent != 0u && parent != static_cast<GuestAddress>(stop))
        {
            frame.r31 = kOwnerSeparator;
            frame.lr = 0x823ac940u;
            const std::uint64_t prefix = OwnerName(memory, arrays, manager,
                methods, sp + 96u, parent, stop, sp, frame);
            frame.lr = 0x823ac950u;
            const std::uint64_t joined = BufferCall(0x8232d418u, memory,
                arrays, manager, sp + 80u, prefix, frame.r31, sp, frame);
            const GuestAddress header = static_cast<GuestAddress>(joined);
            const std::uint64_t text = memory.ReadU32(header + 4u) == 0u ?
                frame.r28 : memory.ReadU32(header);
            frame.lr = 0x823ac970u;
            (void)BufferCall(0x8232d378u, memory, arrays, manager,
                frame.r30, text, 0, sp, frame);
            frame.lr = 0x823ac978u;
            ResetTemporary(memory, manager,
                static_cast<GuestAddress>(sp + 80u), sp, frame);
            frame.lr = 0x823ac980u;
            ResetTemporary(memory, manager,
                static_cast<GuestAddress>(sp + 96u), sp, frame);
        }
        const GuestAddress live_owner = static_cast<GuestAddress>(frame.r29);
        frame.lr = 0x823ac99cu;
        if (memory.ReadU32(live_owner + 4u) == 0xffffffffu)
            (void)CopyRecordText(memory, arrays, sp + 80u,
                kUnnamed, sp, frame);
        else
        {
            frame.lr = 0x823ac9a8u;
            (void)PackedName(memory, arrays, manager, methods,
                sp + 80u, frame.r29 + 44u, sp, frame);
        }
        const GuestAddress suffix = static_cast<GuestAddress>(sp + 80u);
        const std::uint64_t text = memory.ReadU32(suffix + 4u) == 0u ?
            frame.r28 : memory.ReadU32(suffix);
        frame.lr = 0x823ac9c4u;
        (void)BufferCall(0x8232d378u, memory, arrays, manager,
            frame.r30, text, 0, sp, frame);
        frame.lr = 0x823ac9ccu;
        ResetTemporary(memory, manager, suffix, sp, frame);
    }
    const std::uint64_t result = frame.r30;
    Leave(memory, caller_sp, frame, true);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    metadata_utf16_slice::VirtualServices& methods,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case kPackedName:
        result = PackedName(memory, arrays, manager, methods,
            incoming_r3, incoming_r4, caller_sp, frame);
        return true;
    case kOwnerName:
        result = OwnerName(memory, arrays, manager, methods,
            incoming_r3, incoming_r4, incoming_r5, caller_sp, frame);
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::metadata_descriptor_names
