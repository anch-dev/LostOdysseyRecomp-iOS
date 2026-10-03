#include "lo_semantics/metadata_utf16_slice.h"

#include "lo_semantics/manager_object_registration.h"
#include "lo_semantics/metadata_utf16_buffer.h"

#include <array>
#include <bit>

namespace lo::semantic::gpu::metadata_utf16_slice
{
namespace
{
constexpr GuestAddress kEmpty = 0x821a83d0u;

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
    const auto registers = std::array{&frame.r25, &frame.r26, &frame.r27,
        &frame.r28, &frame.r29, &frame.r30, &frame.r31};
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    for (unsigned index = first - 25u; index < registers.size(); ++index)
        Write64(memory, sp - (64u - index * 8u), *registers[index]);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - size, sp);
}
void Leave(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first)
{
    const auto registers = std::array{&frame.r25, &frame.r26, &frame.r27,
        &frame.r28, &frame.r29, &frame.r30, &frame.r31};
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    for (unsigned index = first - 25u; index < registers.size(); ++index)
        *registers[index] = Read64(memory, sp - (64u - index * 8u));
    frame.lr = memory.ReadU32(sp - 8u);
}

std::uint64_t ConstructSlice(GuestMemory& memory, ArrayResizeServices& arrays,
    std::uint64_t destination, std::uint64_t length,
    std::uint64_t source, std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 27u, 128u);
    frame.r29 = length;
    frame.r31 = destination;
    frame.r27 = source;
    frame.r28 = 0;
    const GuestAddress header = static_cast<GuestAddress>(frame.r31);
    const std::uint64_t count = static_cast<std::uint32_t>(frame.r29) == 0u ?
        frame.r28 : frame.r29 + 1u;
    memory.WriteU32(header, 0);
    memory.WriteU32(header + 4u, static_cast<std::uint32_t>(count));
    memory.WriteU32(header + 8u, static_cast<std::uint32_t>(count));
    frame.lr = 0x8232d288u;
    ResizeArray(memory, arrays, header, 2u, 8u);
    if (memory.ReadU32(header + 4u) != 0)
    {
        frame.r30 = memory.ReadU32(header);
        frame.r29 += 1u;
        frame.lr = 0x8232d2acu;
        (void)manager_object_registration::CopyUtf16Padded(memory,
            frame.r30, frame.r27, frame.r29);
        memory.WriteU16(static_cast<GuestAddress>(frame.r30 +
            ((static_cast<std::uint32_t>(frame.r29) << 1u) & 0xfffffffeu) - 2u),
            static_cast<std::uint16_t>(frame.r28));
    }
    const std::uint64_t result = frame.r31;
    Leave(memory, caller_sp, frame, 27u);
    return result;
}

std::uint64_t Slice(GuestMemory& memory, ArrayResizeServices& arrays,
    std::uint64_t destination, std::uint64_t source,
    std::uint64_t start, std::uint64_t length,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 31u, 96u);
    const GuestAddress input = static_cast<GuestAddress>(source);
    const std::uint32_t count = memory.ReadU32(input + 4u);
    frame.r31 = destination;
    std::uint64_t end = start + length;
    const std::uint64_t maximum = count == 0u ? 0u : count - 1u;
    if (static_cast<std::uint32_t>(start) >= maximum)
        start = maximum;
    if (static_cast<std::uint32_t>(end) < static_cast<std::uint32_t>(start))
        end = start;
    else if (static_cast<std::uint32_t>(end) >= maximum)
        end = maximum;
    const std::uint64_t storage = count == 0u ?
        0xffffffff00000000ull | kEmpty : memory.ReadU32(input);
    frame.lr = 0x8232d224u;
    (void)ConstructSlice(memory, arrays, frame.r31, end - start,
        storage + ((static_cast<std::uint32_t>(start) << 1u) & 0xfffffffeu),
        caller_sp - 96u, frame);
    const std::uint64_t result = frame.r31;
    Leave(memory, caller_sp, frame, 31u);
    return result;
}

std::uint64_t Append(GuestMemory& memory, ArrayResizeServices& arrays,
    ManagerFacadeServices& manager, std::uint64_t destination,
    std::uint64_t source, std::uint64_t sp, FrameRegisters& frame)
{
    metadata_utf16_buffer::FrameRegisters lower{frame.lr, frame.r28,
        frame.r29, frame.r30, frame.r31};
    std::uint64_t result = 0;
    (void)metadata_utf16_buffer::Apply(0x8232d378u, memory, arrays,
        manager, destination, source, 0, sp, lower, result);
    frame.lr = lower.lr;
    frame.r28 = lower.r28;
    frame.r29 = lower.r29;
    frame.r30 = lower.r30;
    frame.r31 = lower.r31;
    return result;
}

GuestAddress Manager(GuestMemory& memory, ManagerFacadeServices& services,
    std::uint64_t sp, FrameRegisters& frame, std::uint64_t return_lr)
{
    const auto live_global = [&frame] {
        return static_cast<GuestAddress>(frame.r30 - 18936u);
    };
    GuestAddress manager = memory.ReadU32(live_global());
    if (manager == 0u)
    {
        frame.lr = return_lr;
        (void)InitializeManager(memory, services,
            static_cast<GuestAddress>(sp - 112u));
        manager = memory.ReadU32(live_global());
    }
    return manager;
}

void RemoveTemporary(GuestMemory& memory, ArrayResizeServices& arrays,
    GuestAddress array, std::uint32_t count, std::uint64_t sp,
    FrameRegisters& frame)
{
    Enter(memory, sp, frame, 28u, 128u);
    RemoveArrayRange(memory, arrays, array, 0u, count, 2u, 8u,
        static_cast<GuestAddress>(sp - 128u));
    Leave(memory, sp, frame, 28u);
}

std::uint64_t Reverse(GuestMemory& memory, ArrayResizeServices& arrays,
    ManagerFacadeServices& manager, VirtualServices& methods,
    std::uint64_t destination, std::uint64_t source,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 25u, 160u);
    const std::uint64_t sp = caller_sp - 160u;
    const GuestAddress temp = static_cast<GuestAddress>(sp + 80u);
    frame.r27 = destination;
    frame.r29 = 0;
    frame.r26 = source;
    for (unsigned index = 0; index < 3; ++index)
        memory.WriteU32(static_cast<GuestAddress>(frame.r27 + index * 4u),
            static_cast<std::uint32_t>(frame.r29));
    const std::uint32_t count = memory.ReadU32(
        static_cast<GuestAddress>(frame.r26 + 4u));
    frame.r28 = count == 0u ? UINT64_MAX :
        std::uint64_t{count - 1u} - 1u;
    if (std::bit_cast<std::int64_t>(frame.r28) > -1)
    {
        frame.r30 = 0xffffffff83310000ull;
        frame.r25 = 0xffffffff00000000ull | kEmpty;
    }
    while (std::bit_cast<std::int64_t>(frame.r28) > -1)
    {
        frame.lr = 0x8232d0a4u;
        const std::uint64_t built = Slice(memory, arrays,
            sp + 80u, frame.r26, frame.r28, 1u, sp, frame);
        const std::uint64_t text = memory.ReadU32(
            static_cast<GuestAddress>(built + 4u)) == 0u ? frame.r25 :
            memory.ReadU32(static_cast<GuestAddress>(built));
        frame.lr = 0x8232d0c4u;
        (void)Append(memory, arrays, manager, frame.r27, text, sp, frame);
        const std::uint32_t capacity = memory.ReadU32(temp + 8u);
        memory.WriteU32(temp + 4u, static_cast<std::uint32_t>(frame.r29));
        if (capacity != 0u)
        {
            const GuestAddress old = memory.ReadU32(temp);
            memory.WriteU32(temp + 8u, static_cast<std::uint32_t>(frame.r29));
            if (old != 0u)
            {
                const GuestAddress object = Manager(memory, manager, sp,
                    frame, 0x8232d0f8u);
                const GuestAddress table = memory.ReadU32(object);
                const GuestAddress method = memory.ReadU32(table + 8u) & ~3u;
                frame.r31 = old;
                frame.lr = 0x8232d118u;
                const std::uint64_t returned = methods.CallMethod(method, memory,
                    object, frame.r31, 0, 8, sp, frame);
                memory.WriteU32(temp, static_cast<std::uint32_t>(returned));
            }
        }
        const std::uint32_t removed = memory.ReadU32(temp + 4u);
        frame.lr = 0x8232d134u;
        RemoveTemporary(memory, arrays, temp, removed, sp, frame);
        const GuestAddress remaining = memory.ReadU32(temp);
        if (remaining != 0u)
        {
            const GuestAddress object = Manager(memory, manager, sp,
                frame, 0x8232d154u);
            const GuestAddress table = memory.ReadU32(object);
            const GuestAddress method = memory.ReadU32(table + 12u) & ~3u;
            frame.r31 = remaining;
            frame.lr = 0x8232d16cu;
            (void)methods.CallMethod(method, memory, object, frame.r31,
                removed, 2, sp, frame);
        }
        frame.r28 -= 1u;
        memory.WriteU32(temp, static_cast<std::uint32_t>(frame.r29));
        memory.WriteU32(temp + 8u, static_cast<std::uint32_t>(frame.r29));
        memory.WriteU32(temp + 4u, static_cast<std::uint32_t>(frame.r29));
    }
    const std::uint64_t result = frame.r27;
    Leave(memory, caller_sp, frame, 25u);
    return result;
}

std::uint64_t FormatReverse(GuestMemory& memory, ArrayResizeServices& arrays,
    ManagerFacadeServices& manager, VirtualServices& methods,
    std::uint64_t destination, std::uint64_t number,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 28u, 176u);
    const std::uint64_t sp = caller_sp - 176u;
    const GuestAddress temporary = static_cast<GuestAddress>(sp + 80u);
    const GuestAddress table = static_cast<GuestAddress>(sp + 96u);
    constexpr std::array<GuestAddress, 10> digits = {
        0x820009f8u, 0x82000b90u, 0x821a6b78u, 0x821a83bcu,
        0x82000cd4u, 0x82000cd0u, 0x821a83c0u, 0x821a83c4u,
        0x82000cc4u, 0x82000cccu};
    frame.r31 = 0;
    for (unsigned i = 0; i < digits.size(); ++i)
        memory.WriteU32(table + i * 4u, digits[i]);
    memory.WriteU32(temporary, 0);
    memory.WriteU32(temporary + 4u,
        static_cast<std::uint32_t>(frame.r31));
    memory.WriteU32(temporary + 8u, 0);
    frame.r28 = destination;
    std::int64_t value = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(number));
    frame.r29 = value < 0 ? 1u : 0u;
    if (value < 0) value = -value;
    do
    {
        const std::int64_t quotient = value / 10;
        const std::uint32_t digit = static_cast<std::uint32_t>(value - quotient * 10);
        frame.r30 = static_cast<std::uint64_t>(quotient);
        frame.lr = 0x8232cfd8u;
        (void)Append(memory, arrays, manager, sp + 80u,
            memory.ReadU32(table + digit * 4u), sp, frame);
        value = static_cast<std::int64_t>(frame.r30);
    } while (value != 0);
    if (static_cast<std::uint32_t>(frame.r29) != 0u)
    {
        frame.lr = 0x8232cffcu;
        (void)Append(memory, arrays, manager, sp + 80u,
            0xffffffff821a83c8ull, sp, frame);
    }
    frame.lr = 0x8232d008u;
    (void)Reverse(memory, arrays, manager, methods, frame.r28,
        sp + 80u, sp, frame);
    const std::uint32_t capacity = memory.ReadU32(temporary + 8u);
    memory.WriteU32(temporary + 4u,
        static_cast<std::uint32_t>(frame.r31));
    if (capacity != 0u)
    {
        memory.WriteU32(temporary + 8u,
            static_cast<std::uint32_t>(frame.r31));
        frame.lr = 0x8232d02cu;
        ResizeArray(memory, arrays, temporary, 2u, 8u);
    }
    frame.lr = 0x8232d034u;
    Enter(memory, sp, frame, 31u, 96u);
    (void)ReleaseTwoByteArray(memory, manager, temporary,
        static_cast<GuestAddress>(sp));
    Leave(memory, sp, frame, 31u);
    const std::uint64_t result = frame.r28;
    Leave(memory, caller_sp, frame, 28u);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    VirtualServices& methods, std::uint64_t r3, std::uint64_t r4,
    std::uint64_t r5, std::uint64_t r6, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case 0x8232d240u:
        result = ConstructSlice(memory, arrays, r3, r4, r5, caller_sp, frame);
        return true;
    case 0x8232d190u:
        result = Slice(memory, arrays, r3, r4, r5, r6, caller_sp, frame);
        return true;
    case 0x8232d040u:
        result = Reverse(memory, arrays, manager, methods,
            r3, r4, caller_sp, frame);
        return true;
    case 0x8232ced8u:
        result = FormatReverse(memory, arrays, manager, methods,
            r3, r4, caller_sp, frame);
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::metadata_utf16_slice
