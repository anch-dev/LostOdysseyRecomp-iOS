#include "lo_semantics/metadata_name_lookup.h"

#include "lo_semantics/manager_metadata_compare.h"
#include "lo_semantics/manager_metadata_parsing.h"
#include "lo_semantics/metadata_name_index.h"
#include "lo_semantics/metadata_name_registry.h"
#include "lo_semantics/registered_metadata_composed.h"

#include <bit>

namespace lo::semantic::gpu::metadata_name_lookup
{
namespace
{
constexpr GuestAddress kEntry = 0x82296d30u;
constexpr GuestAddress kReady = 0x83246260u;
constexpr GuestAddress kIdIndex = 0x833690d0u;

std::int32_t Signed(std::uint32_t value)
{
    return std::bit_cast<std::int32_t>(value);
}

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

void SaveOwnFrame(GuestMemory& memory, GuestAddress sp,
    const FrameRegisters& frame)
{
    Write64(memory, sp - 56u, frame.r26);
    Write64(memory, sp - 48u, frame.r27);
    Write64(memory, sp - 40u, frame.r28);
    Write64(memory, sp - 32u, frame.r29);
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - 416u, sp);
}

void RestoreOwnFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& frame)
{
    frame.r26 = Read64(memory, sp - 56u);
    frame.r27 = Read64(memory, sp - 48u);
    frame.r28 = Read64(memory, sp - 40u);
    frame.r29 = Read64(memory, sp - 32u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}

metadata_name_record::FrameRegisters RecordFrame(const FrameRegisters& frame)
{
    return {frame.lr, frame.r27, frame.r28, frame.r29, frame.r30, frame.r31};
}

void MergeFrame(FrameRegisters& target,
    const metadata_name_record::FrameRegisters& source)
{
    target.lr = source.lr;
    target.r27 = source.r27;
    target.r28 = source.r28;
    target.r29 = source.r29;
    target.r30 = source.r30;
    target.r31 = source.r31;
}

std::uint64_t Hash(GuestMemory& memory, std::uint64_t source,
    std::uint64_t sp, FrameRegisters& frame)
{
    metadata_name_index::FrameRegisters lower{frame.lr, frame.r27,
        frame.r28, frame.r29, frame.r30, frame.r31, frame.r0, frame.ctr};
    const std::uint64_t result = metadata_name_index::HashName(memory,
        source, sp, lower);
    MergeFrame(frame, {lower.lr, lower.r27, lower.r28,
        lower.r29, lower.r30, lower.r31});
    frame.r0 = lower.r0;
    frame.ctr = lower.ctr;
    return result;
}

void ClearOutput(GuestMemory& memory, std::uint64_t destination,
    FrameRegisters& frame)
{
    const GuestAddress output = static_cast<GuestAddress>(destination);
    memory.WriteU32(output, 0);
    memory.WriteU32(output + 4u, 0);
    frame.r30 = destination;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    metadata_name_record::Services& record_services,
    ArrayResizeServices& resize_services,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != kEntry)
        return false;
    const GuestAddress caller = static_cast<GuestAddress>(caller_sp);
    SaveOwnFrame(memory, caller, frame);
    const std::uint64_t sp = caller_sp - 416u;
    frame.r30 = incoming_r3;
    frame.r28 = incoming_r4;
    frame.r31 = incoming_r5;
    frame.r26 = incoming_r6;
    frame.r29 = incoming_r7;
    result = incoming_r3;

    if (memory.ReadU32(kReady) == 0)
    {
        frame.lr = 0x82296d64u;
        auto lower = RecordFrame(frame);
        (void)metadata_name_registry::Apply(0x823f4700u, memory,
            record_services, resize_services, sp, lower, result);
        MergeFrame(frame, lower);
    }

    if (Signed(static_cast<std::uint32_t>(frame.r31)) == 0 &&
        Signed(static_cast<std::uint32_t>(frame.r29)) == 1)
    {
        manager_metadata_parsing::FrameRegisters lower{};
        lower.lr = 0x82296d88u;
        lower.r13 = frame.r13;
        lower.r8 = frame.r8;
        lower.r9 = frame.r9;
        lower.r23_through_r31[0] = frame.r23;
        lower.r23_through_r31[1] = frame.r24;
        lower.r23_through_r31[2] = frame.r25;
        lower.r23_through_r31[26u - 23u] = frame.r26;
        lower.r23_through_r31[27u - 23u] = frame.r27;
        lower.r23_through_r31[28u - 23u] = frame.r28;
        lower.r23_through_r31[29u - 23u] = frame.r29;
        lower.r23_through_r31[30u - 23u] = frame.r30;
        lower.r23_through_r31[31u - 23u] = frame.r31;
        frame.lr = 0x82296d88u;
        (void)manager_metadata_parsing::Apply(0x82296e80u, memory,
            thread_services, invalid_services, frame.r28, sp + 96u, 128u,
            sp + 80u, frame.r29, sp, lower, result);
        frame.lr = lower.lr;
        frame.r13 = lower.r13;
        frame.r8 = lower.r8;
        frame.r9 = lower.r9;
        frame.r23 = lower.r23_through_r31[0];
        frame.r24 = lower.r23_through_r31[1];
        frame.r25 = lower.r23_through_r31[2];
        frame.r26 = lower.r23_through_r31[26u - 23u];
        frame.r27 = lower.r23_through_r31[27u - 23u];
        frame.r28 = lower.r23_through_r31[28u - 23u];
        frame.r29 = lower.r23_through_r31[29u - 23u];
        frame.r30 = lower.r23_through_r31[30u - 23u];
        frame.r31 = lower.r23_through_r31[31u - 23u];
        if (Signed(static_cast<std::uint32_t>(result)) != 0)
        {
            const std::uint32_t suffix = memory.ReadU32(
                static_cast<GuestAddress>(sp + 80u));
            frame.r28 = sp + 96u;
            frame.r31 = std::uint64_t{suffix} + 1u;
        }
    }

    if (memory.ReadU16(static_cast<GuestAddress>(frame.r28)) == 0)
    {
        ClearOutput(memory, frame.r30, frame);
        RestoreOwnFrame(memory, caller, frame);
        return true;
    }

    memory.WriteU32(static_cast<GuestAddress>(frame.r30) + 4u,
        static_cast<std::uint32_t>(frame.r31));
    frame.lr = 0x82296dc8u;
    result = Hash(memory, frame.r28, sp, frame);
    frame.r10 = static_cast<std::uint64_t>(std::int64_t{-2094071808});
    frame.r27 = (static_cast<std::uint32_t>(result) << 2u) & 0x3ffcu;
    frame.r29 = static_cast<std::uint64_t>(std::int64_t{-2094078616});
    GuestAddress node = memory.ReadU32(
        static_cast<GuestAddress>(frame.r29 + frame.r27));

    while (node != 0)
    {
        frame.r31 = node;
        InvalidParameterCall call{};
        call.arguments = {frame.r28, frame.r31 + 16u, incoming_r5,
            incoming_r6, incoming_r7, frame.r8, frame.r9, frame.r10};
        call.thread_environment = frame.r13;
        frame.lr = 0x82296decu;
        (void)manager_metadata_compare::Apply(0x822971e0u, memory,
            thread_services, invalid_services, call, sp, frame.lr, result);
        frame.r13 = call.thread_environment;
        frame.r8 = call.arguments[5];
        frame.r9 = call.arguments[6];
        frame.r10 = call.arguments[7];
        if (Signed(static_cast<std::uint32_t>(result)) == 0)
        {
            const std::uint32_t identifier = memory.ReadU32(
                static_cast<GuestAddress>(frame.r31));
            memory.WriteU32(static_cast<GuestAddress>(frame.r30), identifier);
            if (Signed(static_cast<std::uint32_t>(frame.r26)) == 2)
            {
                frame.lr = 0x82296e74u;
                std::uint64_t after = 0;
                result = registered_metadata_composed::CopyUtf16UntilNull(
                    memory, frame.r31 + 16u, frame.r28, after);
            }
            RestoreOwnFrame(memory, caller, frame);
            return true;
        }
        node = memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 12u));
    }

    if (Signed(static_cast<std::uint32_t>(frame.r26)) == 0)
    {
        ClearOutput(memory, frame.r30, frame);
        RestoreOwnFrame(memory, caller, frame);
        return true;
    }

    frame.r31 = static_cast<std::uint64_t>(std::int64_t{-2093575984});
    frame.lr = 0x82296e24u;
    const std::uint64_t identifier = registered_metadata_words::AddArrayElements(
        memory, resize_services, kIdIndex, 1u, 4u, 8u);
    memory.WriteU32(static_cast<GuestAddress>(frame.r30),
        static_cast<std::uint32_t>(identifier));
    const std::uint64_t previous = memory.ReadU32(
        static_cast<GuestAddress>(frame.r29 + frame.r27));
    auto lower = RecordFrame(frame);
    lower.lr = 0x82296e3cu;
    (void)metadata_name_record::Apply(0x823f7b08u, memory, record_services,
        frame.r28, identifier, 0, previous, sp, lower, result);
    MergeFrame(frame, lower);
    memory.WriteU32(static_cast<GuestAddress>(frame.r29 + frame.r27),
        static_cast<std::uint32_t>(result));
    const std::uint32_t live_identifier = memory.ReadU32(
        static_cast<GuestAddress>(frame.r30));
    const std::uint32_t offset = live_identifier << 2u;
    frame.r10 = offset & 0xfffffffcu;
    const GuestAddress storage = memory.ReadU32(
        static_cast<GuestAddress>(frame.r31));
    memory.WriteU32(storage + static_cast<GuestAddress>(frame.r10),
        static_cast<std::uint32_t>(result));
    RestoreOwnFrame(memory, caller, frame);
    return true;
}

} // namespace lo::semantic::gpu::metadata_name_lookup
