#include "lo_semantics/metadata_descriptor_array.h"

#include "lo_semantics/memory_move.h"
#include "lo_semantics/registered_metadata_words.h"

#include <bit>

namespace lo::semantic::gpu::metadata_descriptor_array
{
namespace
{
constexpr GuestAddress kPop = 0x823b9268u;
constexpr GuestAddress kAssign = 0x822b3f50u;
constexpr GuestAddress kRegister = 0x82400bc0u;
constexpr GuestAddress kFreeSlots = 0x83369100u;
constexpr GuestAddress kDescriptors = 0x833690f4u;
constexpr GuestAddress kThreshold = 0x83315f48u;
constexpr GuestAddress kCounter = 0x832383c4u;
constexpr GuestAddress kPairBuckets = 0x832fa568u;
constexpr GuestAddress kTripleBuckets = 0x832f2568u;
constexpr std::uint64_t kNeedsFreshIndex = 1ull << 39u;
constexpr GuestAddress kEmptySource = 0x821a83d0u;

std::int32_t Signed(std::uint32_t word)
{
    return std::bit_cast<std::int32_t>(word);
}

void Write64(GuestMemory& memory, GuestAddress address,
    std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}

void EnterFrame(GuestMemory& memory, std::uint64_t caller_sp,
    std::uint32_t frame_size, const FrameRegisters& frame,
    bool save_r29, bool save_r30)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    // 82400BC0 uses the real savegprlr_29 order; the two smaller entries
    // store LR first and then their explicit saved GPRs.
    if (!save_r29)
        memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    if (save_r29) Write64(memory, sp - 32u, frame.r29);
    if (save_r30) Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    if (save_r29)
        memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - frame_size, sp);
}

void LeaveFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, bool save_r29, bool save_r30)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (!save_r29) frame.lr = memory.ReadU32(sp - 8u);
    if (save_r29) frame.r29 = Read64(memory, sp - 32u);
    if (save_r30) frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    if (save_r29) frame.lr = memory.ReadU32(sp - 8u);
}

void PopLast(GuestMemory& memory, ArrayResizeServices& services,
    std::uint64_t array_register, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    EnterFrame(memory, caller_sp, 96u, frame, false, false);
    const GuestAddress array = static_cast<GuestAddress>(array_register);
    const std::uint32_t count = memory.ReadU32(array + 4u);
    const GuestAddress storage = memory.ReadU32(array);
    const GuestAddress last = storage + ((count << 2u) & 0xfffffffcu) - 4u;
    frame.r31 = memory.ReadU32(last);
    frame.lr = 0x823b92a0u;
    RemoveArrayRange(memory, services, array, count - 1u, 1u,
        4u, 8u, static_cast<GuestAddress>(caller_sp - 96u - 128u));
    result = frame.r31;
    LeaveFrame(memory, caller_sp, frame, false, false);
}

void AssignBytes(GuestMemory& memory, ArrayResizeServices& services,
    std::uint64_t destination_register, std::uint64_t source_register,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result)
{
    EnterFrame(memory, caller_sp, 112u, frame, false, true);
    frame.r31 = destination_register;
    frame.r30 = source_register;
    result = destination_register;
    const GuestAddress destination = static_cast<GuestAddress>(frame.r31);
    const GuestAddress source = static_cast<GuestAddress>(frame.r30);
    if (destination != source)
    {
        const std::uint32_t source_count = memory.ReadU32(source + 4u);
        memory.WriteU32(destination + 8u, source_count);
        memory.WriteU32(destination + 4u, source_count);
        frame.lr = 0x822b3f8cu;
        ResizeArray(memory, services, destination, 2u, 8u);
        const std::uint32_t destination_count =
            memory.ReadU32(destination + 4u);
        if (Signed(destination_count) != 0)
        {
            const std::uint32_t live_source_count =
                memory.ReadU32(source + 4u);
            const GuestAddress copy_source = Signed(live_source_count) != 0 ?
                memory.ReadU32(source) : kEmptySource;
            const GuestAddress copy_destination = memory.ReadU32(destination);
            frame.lr = 0x822b3fc0u;
            (void)CopyGuestMemory(memory, copy_destination, copy_source,
                (destination_count << 1u) & 0xfffffffeu,
                static_cast<GuestAddress>(caller_sp - 112u));
        }
        result = frame.r31;
    }
    LeaveFrame(memory, caller_sp, frame, false, true);
}

void RegisterDescriptor(GuestMemory& memory, ArrayResizeServices& services,
    std::uint64_t object_register, std::uint64_t desired_index,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result)
{
    EnterFrame(memory, caller_sp, 128u, frame, true, true);
    const std::uint64_t sp = caller_sp - 128u;
    frame.r31 = object_register;
    frame.r29 = 0xffffffff83310000ull;
    frame.r30 = 0xffffffff833690f4ull;
    GuestAddress object = static_cast<GuestAddress>(frame.r31);
    result = desired_index;
    if (Signed(static_cast<std::uint32_t>(result)) == -1)
    {
        if ((Read64(memory, object + 8u) & kNeedsFreshIndex) != 0)
        {
            result = std::uint64_t{memory.ReadU32(kCounter)} + 1u;
            memory.WriteU32(kCounter, static_cast<std::uint32_t>(result));
            if (Signed(static_cast<std::uint32_t>(result)) <
                Signed(memory.ReadU32(kThreshold)))
                goto index_ready;
        }
        if (Signed(memory.ReadU32(kFreeSlots + 4u)) != 0)
        {
            frame.lr = 0x82400c34u;
            PopLast(memory, services, kFreeSlots, sp, frame, result);
        }
        else
        {
            frame.lr = 0x82400c4cu;
            result = registered_metadata_words::AddArrayElements(
                memory, services, kDescriptors, 1u, 4u, 8u);
        }
    }
index_ready:
    // The pop helper restores its r31 from guest RAM. A resize callback can
    // change that saved slot, so the parent's following reads use live r31.
    object = static_cast<GuestAddress>(frame.r31);
    if (Signed(static_cast<std::uint32_t>(result)) >=
        Signed(memory.ReadU32(kThreshold)))
        Write64(memory, object + 8u,
            Read64(memory, object + 8u) & ~kNeedsFreshIndex);

    const GuestAddress storage = memory.ReadU32(kDescriptors);
    memory.WriteU32(storage +
        ((static_cast<std::uint32_t>(result) << 2u) & 0xfffffffcu),
        static_cast<std::uint32_t>(frame.r31));
    memory.WriteU32(object + 4u, static_cast<std::uint32_t>(result));

    const GuestAddress stage = static_cast<GuestAddress>(sp + 80u);
    Write64(memory, stage, Read64(memory, object + 44u));
    const std::uint32_t pair_hash =
        ((memory.ReadU32(stage) ^ memory.ReadU32(stage + 4u)) << 2u) & 0x7ffcu;
    const GuestAddress pair_bucket = kPairBuckets + pair_hash;
    memory.WriteU32(object + 16u, memory.ReadU32(pair_bucket));
    memory.WriteU32(pair_bucket, static_cast<std::uint32_t>(frame.r31));

    const std::uint64_t second_pair = Read64(memory, object + 44u);
    const std::uint32_t leading_word = memory.ReadU32(object + 40u);
    Write64(memory, stage, second_pair);
    const std::uint32_t triple_hash =
        ((leading_word ^ memory.ReadU32(stage) ^
            memory.ReadU32(stage + 4u)) << 2u) & 0x7ffcu;
    const GuestAddress triple_bucket = kTripleBuckets + triple_hash;
    memory.WriteU32(object + 20u, memory.ReadU32(triple_bucket));
    memory.WriteU32(triple_bucket, static_cast<std::uint32_t>(frame.r31));
    LeaveFrame(memory, caller_sp, frame, true, true);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case kPop:
        (void)incoming_r4;
        PopLast(memory, services, incoming_r3, caller_sp, frame, result);
        return true;
    case kAssign:
        AssignBytes(memory, services, incoming_r3, incoming_r4,
            caller_sp, frame, result);
        return true;
    case kRegister:
        RegisterDescriptor(memory, services, incoming_r3, incoming_r4,
            caller_sp, frame, result);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::metadata_descriptor_array
