#include "lo_semantics/metadata_utf16_search.h"

#include "lo_semantics/metadata_utf16_buffer.h"
#include "lo_semantics/registered_metadata_string.h"
#include "lo_semantics/string_property_initializer.h"

#include <array>
#include <bit>

namespace lo::semantic::gpu::metadata_utf16_search
{
namespace
{
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
    const FrameRegisters& frame, unsigned first, std::uint32_t size)
{
    const auto regs = std::array{&frame.r25, &frame.r26, &frame.r27,
        &frame.r28, &frame.r29, &frame.r30, &frame.r31};
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    for (unsigned i = first - 25u; i < regs.size(); ++i)
        Write64(memory, sp - (64u - i * 8u), *regs[i]);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - size, sp);
}
void Leave(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first)
{
    const auto regs = std::array{&frame.r25, &frame.r26, &frame.r27,
        &frame.r28, &frame.r29, &frame.r30, &frame.r31};
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    for (unsigned i = first - 25u; i < regs.size(); ++i)
        *regs[i] = Read64(memory, sp - (64u - i * 8u));
    frame.lr = memory.ReadU32(sp - 8u);
}

std::uint64_t Find(GuestMemory& memory, std::uint64_t text,
    std::uint64_t pattern)
{
    if (memory.ReadU16(static_cast<GuestAddress>(pattern)) == 0u)
        return text;
    while (memory.ReadU16(static_cast<GuestAddress>(text)) != 0u)
    {
        std::uint64_t scan = pattern;
        std::uint64_t candidate = text;
        while (memory.ReadU16(static_cast<GuestAddress>(scan)) != 0u &&
            memory.ReadU16(static_cast<GuestAddress>(candidate)) ==
                memory.ReadU16(static_cast<GuestAddress>(scan)))
        {
            scan += 2u;
            candidate += 2u;
            if (memory.ReadU16(static_cast<GuestAddress>(candidate)) == 0u)
                break;
        }
        if (memory.ReadU16(static_cast<GuestAddress>(scan)) == 0u)
            return text;
        text += 2u;
    }
    return 0;
}

void CopyHeader(GuestMemory& memory, ArrayResizeServices& arrays,
    ManagerFacadeServices& manager, std::uint64_t destination,
    std::uint64_t source, std::uint64_t sp, FrameRegisters& frame)
{
    string_property_initializer::FrameRegisters lower{frame.lr,
        frame.r28, frame.r29, frame.r30, frame.r31};
    std::uint64_t result = 0;
    (void)string_property_initializer::Apply(0x822a06c0u, memory,
        arrays, manager, destination, source, sp, lower, result);
    frame.lr = lower.lr;
    frame.r28 = lower.r28;
    frame.r29 = lower.r29;
    frame.r30 = lower.r30;
    frame.r31 = lower.r31;
}
void Append(GuestMemory& memory, ArrayResizeServices& arrays,
    ManagerFacadeServices& manager, std::uint64_t destination,
    std::uint64_t source, std::uint64_t sp, FrameRegisters& frame)
{
    metadata_utf16_buffer::FrameRegisters lower{frame.lr,
        frame.r28, frame.r29, frame.r30, frame.r31};
    std::uint64_t result = 0;
    (void)metadata_utf16_buffer::Apply(0x8232d378u, memory,
        arrays, manager, destination, source, 0, sp, lower, result);
    frame.lr = lower.lr;
    frame.r28 = lower.r28;
    frame.r29 = lower.r29;
    frame.r30 = lower.r30;
    frame.r31 = lower.r31;
}
void Reset(GuestMemory& memory, ManagerFacadeServices& manager,
    GuestAddress header, std::uint64_t sp, FrameRegisters& frame)
{
    Enter(memory, sp, frame, 31u, 96u);
    (void)ResetTwoByteArray(memory, manager, header,
        static_cast<GuestAddress>(sp));
    Leave(memory, sp, frame, 31u);
}

std::uint64_t Prefix(GuestMemory& memory, ArrayResizeServices& arrays,
    ManagerFacadeServices& manager, metadata_utf16_slice::VirtualServices& methods,
    std::uint64_t destination, std::uint64_t source, std::uint64_t length,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 31u, 96u);
    const auto count = memory.ReadU32(static_cast<GuestAddress>(source + 4u));
    frame.r31 = destination;
    const std::uint64_t text = count == 0u ? 0xffffffff821a83d0ull :
        memory.ReadU32(static_cast<GuestAddress>(source));
    std::uint64_t take = count == 0u ? 0u : std::uint64_t{count} - 1u;
    const auto wanted = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(length));
    if (wanted < 0) take = 0;
    else if (wanted < std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(take)))
        take = length;
    frame.lr = 0x823671c8u;
    std::uint64_t nested_result = 0;
    (void)metadata_utf16_slice::Apply(0x8232d240u, memory, arrays,
        manager, methods, frame.r31, take, text, 0, caller_sp - 96u,
        frame, nested_result);
    const auto result = frame.r31;
    Leave(memory, caller_sp, frame, 31u);
    return result;
}

std::uint64_t Replace(GuestMemory& memory, ArrayResizeServices& arrays,
    ManagerFacadeServices& manager, std::uint64_t destination,
    std::uint64_t source, std::uint64_t pattern, std::uint64_t replacement,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 25u, 176u);
    const std::uint64_t sp = caller_sp - 176u;
    frame.r31 = source;
    frame.r25 = destination;
    frame.r29 = pattern;
    frame.r26 = replacement;
    const auto count = memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 4u));
    if (count == 0u || count == 1u)
    {
        frame.lr = 0x82339d70u;
        CopyHeader(memory, arrays, manager, frame.r25, frame.r31, sp, frame);
    }
    else
    {
        frame.r28 = 0;
        for (unsigned i = 0; i < 3; ++i)
            memory.WriteU32(static_cast<GuestAddress>(sp + 80u + i * 4u), 0);
        frame.lr = 0x82339d98u;
        CopyHeader(memory, arrays, manager, sp + 96u, frame.r31, sp, frame);
        frame.r30 = memory.ReadU32(static_cast<GuestAddress>(frame.r31));
        frame.lr = 0x82339da4u;
        frame.r27 = registered_metadata_string::Utf16Length(memory, frame.r29);
        frame.lr = 0x82339db4u;
        frame.r31 = Find(memory, frame.r30, frame.r29);
        if (static_cast<GuestAddress>(frame.r31) != 0u)
            frame.r27 = (static_cast<std::uint32_t>(frame.r27) << 1u) & 0xfffffffeu;
        while (static_cast<GuestAddress>(frame.r31) != 0u)
        {
            memory.WriteU16(static_cast<GuestAddress>(frame.r31),
                static_cast<std::uint16_t>(frame.r28));
            frame.lr = 0x82339dd4u;
            Append(memory, arrays, manager, sp + 80u, frame.r30, sp, frame);
            frame.lr = 0x82339de0u;
            Append(memory, arrays, manager, sp + 80u, frame.r26, sp, frame);
            const auto first = memory.ReadU16(static_cast<GuestAddress>(frame.r29));
            frame.r30 = frame.r27 + frame.r31;
            memory.WriteU16(static_cast<GuestAddress>(frame.r31), first);
            frame.lr = 0x82339df8u;
            frame.r31 = Find(memory, frame.r30, frame.r29);
        }
        frame.lr = 0x82339e10u;
        Append(memory, arrays, manager, sp + 80u, frame.r30, sp, frame);
        frame.lr = 0x82339e1cu;
        CopyHeader(memory, arrays, manager, frame.r25, sp + 80u, sp, frame);
        frame.lr = 0x82339e24u;
        Reset(memory, manager, static_cast<GuestAddress>(sp + 96u), sp, frame);
        frame.lr = 0x82339e2cu;
        Reset(memory, manager, static_cast<GuestAddress>(sp + 80u), sp, frame);
    }
    const auto result = frame.r25;
    Leave(memory, caller_sp, frame, 25u);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    metadata_utf16_slice::VirtualServices& methods,
    std::uint64_t r3, std::uint64_t r4, std::uint64_t r5,
    std::uint64_t r6, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case 0x8229d0e8u: result = Find(memory, r3, r4); return true;
    case 0x82367160u:
        result = Prefix(memory, arrays, manager, methods, r3, r4, r5,
            caller_sp, frame); return true;
    case 0x82339d30u:
        result = Replace(memory, arrays, manager, r3, r4, r5, r6,
            caller_sp, frame); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::metadata_utf16_search
