#include "lo_semantics/instance_shared_metadata_initializer.h"

#include <initializer_list>
#include <stdexcept>

namespace lo::semantic::gpu::instance_shared_metadata_initializer
{
namespace
{
constexpr GuestAddress kInitializer = 0x824108C8u;
constexpr GuestAddress kNullTail = 0x82412100u;
constexpr GuestAddress kFrameSize = 160u;

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

void EnterFrame(GuestMemory& memory, GuestAddress sp, FrameRegisters& frame)
{
    // The actual __savegprlr_28 helper stores GPRs in this order before LR.
    WriteU64(memory, sp - 40u, frame.r28);
    WriteU64(memory, sp - 32u, frame.r29);
    WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    frame.lr = 0x824108D0u;
    memory.WriteU32(sp - kFrameSize, sp);
}

void LeaveFrame(GuestMemory& memory, GuestAddress sp, FrameRegisters& frame)
{
    frame.r28 = ReadU64(memory, sp - 40u);
    frame.r29 = ReadU64(memory, sp - 32u);
    frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}

void Clear(GuestMemory& memory, GuestAddress object,
    std::initializer_list<GuestAddress> offsets, std::uint32_t value)
{
    for (GuestAddress offset : offsets)
        memory.WriteU32(object + offset, value);
}

void Initialize(GuestMemory& memory, ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    registered_callback_family::Services& callback_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    EnterFrame(memory, sp, frame);
    const std::uint64_t nested_sp = caller_sp - kFrameSize;
    frame.r31 = incoming_r3;
    frame.r30 = 0;
    frame.r28 = 8;
    frame.r29 = 0xFFFFFFFF83310000ull;
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    Clear(memory, object, {84u, 88u, 92u, 128u, 132u, 136u,
        164u, 168u, 172u, 176u}, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 180u, static_cast<GuestAddress>(frame.r28));
    memory.WriteU32(object, 0x82005160u);

    const GuestAddress global = static_cast<GuestAddress>(frame.r29) + 24416u;
    std::uint32_t shared = memory.ReadU32(global);
    if (shared == 0)
    {
        std::uint64_t constructed = 0;
        frame.lr = 0x82410934u;
        if (!registered_constructor_family::Apply(0x82403148u, memory,
                manager_services, registration_services,
                0xFFFFFFFF8218C210ull,
                static_cast<GuestAddress>(nested_sp), constructed))
            throw std::logic_error("shared metadata constructor missing");
        memory.WriteU32(global, static_cast<GuestAddress>(constructed));
        frame.lr = 0x8241093Cu;
        (void)registered_callback_family::RegisterSharedMetadataObject(
            memory, manager_services, callback_services, constructed,
            static_cast<GuestAddress>(nested_sp));
        shared = memory.ReadU32(global);
    }
    memory.WriteU32(object + 196u, shared);
    memory.WriteU32(object + 208u, static_cast<GuestAddress>(frame.r30));
    result = frame.r31; // mr r3,r31 precedes all following stack staging.
    memory.WriteU32(static_cast<GuestAddress>(nested_sp + 80u),
        static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(static_cast<GuestAddress>(nested_sp + 84u),
        static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(static_cast<GuestAddress>(nested_sp + 88u),
        static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 212u, static_cast<GuestAddress>(frame.r30));
    for (GuestAddress offset : {92u, 96u, 100u, 104u})
        memory.WriteU32(static_cast<GuestAddress>(nested_sp + offset),
            static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 216u, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(static_cast<GuestAddress>(nested_sp + 108u),
        static_cast<GuestAddress>(frame.r30));
    Clear(memory, object, {220u, 224u, 228u, 232u, 236u, 240u,
        244u, 248u, 252u, 256u, 260u, 264u, 268u, 272u, 276u,
        296u, 300u, 304u, 308u}, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 312u, static_cast<GuestAddress>(frame.r28));
    Clear(memory, object, {316u, 320u, 324u, 328u},
        static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 332u, static_cast<GuestAddress>(frame.r28));
    Clear(memory, object, {336u, 340u, 344u, 352u, 356u, 360u,
        364u, 368u, 372u}, static_cast<GuestAddress>(frame.r30));
    LeaveFrame(memory, sp, frame);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    registered_callback_family::Services& callback_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != kInitializer && address != kNullTail)
        return false;
    if (address == kNullTail && static_cast<GuestAddress>(incoming_r3) == 0)
    {
        result = incoming_r3;
        return true;
    }
    Initialize(memory, manager_services, registration_services,
        callback_services, incoming_r3, caller_sp, frame, result);
    return true;
}

} // namespace lo::semantic::gpu::instance_shared_metadata_initializer
