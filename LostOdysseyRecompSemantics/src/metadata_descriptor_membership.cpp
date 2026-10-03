#include "lo_semantics/metadata_descriptor_membership.h"
#include "lo_semantics/registered_metadata_words.h"

#include <array>
#include <bit>

namespace lo::semantic::gpu::metadata_descriptor_membership
{
namespace
{
constexpr std::uint64_t kGlobalOwners = 0xffffffff8336913cull;
constexpr GuestAddress kObserver = 0x83315f78u;

std::int32_t Signed(std::uint64_t value)
{
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}
std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}
void Write64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}
auto SavedRegisters(FrameRegisters& frame)
{
    return std::array{&frame.r27, &frame.r28, &frame.r29,
        &frame.r30, &frame.r31};
}
void Enter(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first)
{
    const auto sp = static_cast<GuestAddress>(caller_sp);
    const auto registers = SavedRegisters(frame);
    for (unsigned index = first - 27u; index < registers.size(); ++index)
        Write64(memory, sp - (48u - 8u * index), *registers[index]);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - 128u, sp);
}
void Leave(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first)
{
    const auto sp = static_cast<GuestAddress>(caller_sp);
    const auto registers = SavedRegisters(frame);
    for (unsigned index = first - 27u; index < registers.size(); ++index)
        *registers[index] = Read64(memory, sp - (48u - 8u * index));
    frame.lr = memory.ReadU32(sp - 8u);
}
std::uint64_t Call(GuestMemory& memory, Services& services,
    std::uint64_t receiver, std::uint64_t argument, std::uint32_t offset,
    std::uint64_t sp, std::uint64_t return_address, FrameRegisters& frame)
{
    const auto table = memory.ReadU32(static_cast<GuestAddress>(receiver));
    frame.ctr = memory.ReadU32(table + offset);
    frame.lr = return_address;
    return services.Dispatch(static_cast<GuestAddress>(frame.ctr) & ~3u,
        receiver, argument, sp, frame);
}

void RemoveSlot(GuestMemory& memory, Services& services,
    std::uint64_t owner, std::uint64_t member, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    Enter(memory, caller_sp, frame, 27u);
    const std::uint64_t sp = caller_sp - 128u;
    const auto index = memory.ReadU32(static_cast<GuestAddress>(member + 36u));
    frame.r28 = owner;
    result = owner;
    if (Signed(index) >= 0 && Signed(index) < Signed(memory.ReadU32(
            static_cast<GuestAddress>(frame.r28 + 116u))))
    {
        const auto storage = memory.ReadU32(static_cast<GuestAddress>(frame.r28 + 112u));
        const auto offset = (index << 2u) & 0xfffffffcu;
        if (memory.ReadU32(storage + offset) == static_cast<std::uint32_t>(member))
        {
            frame.r30 = 0;
            frame.r27 = 0xffffffff83310000ull;
            memory.WriteU32(storage + offset, 0);
            const auto old_count = memory.ReadU32(static_cast<GuestAddress>(frame.r28 + 124u));
            result = memory.ReadU32(static_cast<GuestAddress>(frame.r27 + 24440u));
            memory.WriteU32(static_cast<GuestAddress>(frame.r28 + 124u), old_count - 1u);
            if (static_cast<std::uint32_t>(result) != 0u)
            {
                result = Call(memory, services, result, member, 12u,
                    sp, 0x82408118u, frame);
                result = memory.ReadU32(static_cast<GuestAddress>(frame.r27 + 24440u));
            }
            if (Signed(memory.ReadU32(static_cast<GuestAddress>(frame.r28 + 124u))) == 0)
            {
                frame.r31 = frame.r30;
                frame.r29 = kGlobalOwners;
                std::uint32_t bound = memory.ReadU32(static_cast<GuestAddress>(frame.r29 + 4u));
                if (Signed(bound) > 0)
                {
                    do
                    {
                        const auto data = memory.ReadU32(static_cast<GuestAddress>(frame.r29));
                        if (memory.ReadU32(data + static_cast<GuestAddress>(frame.r30)) ==
                                static_cast<std::uint32_t>(frame.r28))
                        {
                            frame.lr = 0x82408168u;
                            RemoveArrayRange(memory, services,
                                static_cast<GuestAddress>(frame.r29),
                                static_cast<std::uint32_t>(frame.r31), 1u, 4u, 8u,
                                static_cast<GuestAddress>(sp - 128u));
                            bound = memory.ReadU32(static_cast<GuestAddress>(frame.r29 + 4u));
                            frame.r31 -= 1u;
                            frame.r30 -= 4u;
                        }
                        frame.r31 += 1u;
                        frame.r30 += 4u;
                    } while (Signed(frame.r31) < Signed(bound));
                    result = memory.ReadU32(static_cast<GuestAddress>(frame.r27 + 24440u));
                }
                if (static_cast<std::uint32_t>(result) != 0u)
                    result = Call(memory, services, result, frame.r28, 8u,
                        sp, 0x824081a4u, frame);
            }
        }
    }
    Leave(memory, caller_sp, frame, 27u);
}

void ChangeSlot(GuestMemory& memory, Services& services,
    std::uint64_t member, std::uint64_t index, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    Enter(memory, caller_sp, frame, 29u);
    const std::uint64_t sp = caller_sp - 128u;
    frame.r29 = member;
    frame.r30 = index;
    const auto old_index = memory.ReadU32(static_cast<GuestAddress>(frame.r29 + 36u));
    result = member;
    if (Signed(frame.r30) != Signed(old_index))
    {
        frame.r31 = frame.r29;
        for (;;)
        {
            const auto parent = memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 40u));
            if (parent == 0u) break;
            frame.r31 = parent;
        }
        if ((memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 148u)) & 4u) == 0u)
        {
            if (Signed(old_index) != -1)
            {
                frame.lr = 0x823aae60u;
                RemoveSlot(memory, services, frame.r31, frame.r29, sp, frame, result);
            }
            memory.WriteU32(static_cast<GuestAddress>(frame.r29 + 36u),
                static_cast<std::uint32_t>(frame.r30));
            if (Signed(frame.r30) >= 0 && Signed(frame.r30) < Signed(memory.ReadU32(
                    static_cast<GuestAddress>(frame.r31 + 116u))))
            {
                const auto data = memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 112u));
                const auto offset = (static_cast<std::uint32_t>(frame.r30) << 2u) & 0xfffffffcu;
                if (memory.ReadU32(data + offset) == 0u)
                {
                    memory.WriteU32(data + offset, static_cast<std::uint32_t>(frame.r29));
                    const auto next = memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 124u)) + 1u;
                    memory.WriteU32(static_cast<GuestAddress>(frame.r31 + 124u), next);
                    if (Signed(next) == 1)
                    {
                        memory.WriteU32(static_cast<GuestAddress>(sp + 80u),
                            static_cast<std::uint32_t>(frame.r31));
                        frame.lr = 0x823aaec0u;
                        result = registered_metadata_words::AppendMetadataWord(memory,
                            services, static_cast<GuestAddress>(kGlobalOwners),
                            static_cast<GuestAddress>(sp + 80u));
                        result = memory.ReadU32(kObserver);
                        if (static_cast<std::uint32_t>(result) != 0u)
                            result = Call(memory, services, result, frame.r31, 4u,
                                sp, 0x823aaee4u, frame);
                    }
                }
            }
        }
    }
    Leave(memory, caller_sp, frame, 29u);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory, Services& services,
    std::uint64_t r3, std::uint64_t r4, std::uint64_t sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case 0x824080a8u: RemoveSlot(memory, services, r3, r4, sp, frame, result); return true;
    case 0x823aae00u: ChangeSlot(memory, services, r3, r4, sp, frame, result); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::metadata_descriptor_membership
