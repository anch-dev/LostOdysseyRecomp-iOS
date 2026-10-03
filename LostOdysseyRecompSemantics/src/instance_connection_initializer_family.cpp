#include "lo_semantics/instance_connection_initializer_family.h"

#include "lo_semantics/instance_linker_composed_family.h"
#include "lo_semantics/loaded_single.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/string_property_initializer.h"

#include <array>
#include <bit>
#include <stdexcept>

namespace lo::semantic::gpu::instance_connection_initializer_family
{
namespace
{
constexpr GuestAddress kNullTail = 0x82679F50u;
constexpr GuestAddress kChild = 0x8267A040u;
constexpr GuestAddress kNet = 0x8267A1F0u;
constexpr GuestAddress kTcpip = 0x8272CBD8u;
constexpr GuestAddress kBitWriter = 0x82752768u;

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

std::uint64_t& Register(FrameRegisters& frame, unsigned number)
{
    switch (number)
    {
    case 25: return frame.r25;
    case 26: return frame.r26;
    case 27: return frame.r27;
    case 28: return frame.r28;
    case 29: return frame.r29;
    case 30: return frame.r30;
    default: return frame.r31;
    }
}

void EnterFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& frame, unsigned first, GuestAddress size,
    GuestAddress helper_return)
{
    for (unsigned number = first; number <= 31; ++number)
        WriteU64(memory, sp - (33u - number) * 8u,
            Register(frame, number));
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    frame.lr = helper_return;
    memory.WriteU32(sp - size, sp);
}

// Child/Tcpip use inline prologues, with LR stored before the nonvolatile
// doublewords. The savegprlr helpers above store the doublewords first.
void EnterDirectFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& frame, unsigned first, GuestAddress size)
{
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    for (unsigned number = first; number <= 31; ++number)
        WriteU64(memory, sp - (33u - number) * 8u,
            Register(frame, number));
    memory.WriteU32(sp - size, sp);
}

void LeaveFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& frame, unsigned first)
{
    for (unsigned number = first; number <= 31; ++number)
        Register(frame, number) = ReadU64(memory,
            sp - (33u - number) * 8u);
    frame.lr = memory.ReadU32(sp - 8u);
}

void LeaveDirectFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& frame, unsigned first)
{
    frame.lr = memory.ReadU32(sp - 8u);
    for (unsigned number = first; number <= 31; ++number)
        Register(frame, number) = ReadU64(memory,
            sp - (33u - number) * 8u);
}

std::uint32_t RoundedByteCount(std::uint64_t bits)
{
    const std::uint32_t shifted = static_cast<std::uint32_t>(bits + 7u);
    const auto signed_word = std::bit_cast<std::int32_t>(shifted);
    return static_cast<std::uint32_t>(signed_word >> 3);
}

void BitWriter(GuestMemory& memory, ArrayResizeServices& resize_services,
    std::uint64_t object_register, std::uint64_t bit_count,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    EnterFrame(memory, sp, frame,
        29, 112u, 0x82752770u);
    const std::uint64_t nested_sp = caller_sp - 112u;
    frame.r31 = object_register;
    frame.r30 = bit_count;
    GuestAddress object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object, 0x82189968u);
    frame.lr = 0x8275278Cu;
    instance_linker_composed_family::FrameRegisters archive_frame{
        frame.lr, frame.r30, frame.r31};
    std::uint64_t archive_result = 0;
    if (!instance_linker_composed_family::Apply(0x823F34B8u, memory,
            resize_services, frame.r31,
            static_cast<GuestAddress>(nested_sp), archive_frame,
            archive_result))
        throw std::logic_error("archive-fields dependency missing");

    const std::uint32_t capacity = RoundedByteCount(frame.r30);
    object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object, 0x821FE828u);
    memory.WriteU32(object + 116u, capacity);
    memory.WriteU32(object + 112u, 0);
    memory.WriteU32(object + 120u, capacity);
    frame.r29 = 0;
    frame.lr = 0x827527C0u;
    ResizeArray(memory, resize_services, object + 112u, 1u, 8u);

    // The count and storage are reloaded after the resize callback. They may
    // have been changed by it or by an alias with the destination header.
    const std::uint32_t live_count = memory.ReadU32(object + 116u);
    const GuestAddress storage = memory.ReadU32(object + 112u);
    memory.WriteU32(object + 124u, static_cast<GuestAddress>(frame.r29));
    memory.WriteU32(object + 128u, static_cast<GuestAddress>(frame.r30));
    frame.lr = 0x827527D8u;
    (void)FillGuestMemory(memory, storage, 0, live_count);
    const std::uint32_t flags = memory.ReadU32(object + 8u);
    memory.WriteU32(object + 20u, 1u);
    memory.WriteU32(object + 28u, 1u);
    memory.WriteU32(object + 8u, flags | 0x80000000u);
    result = frame.r31;
    LeaveFrame(memory, sp, frame, 29);
}

void ApplyProperty(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    std::uint64_t object_register, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    frame.lr = 0x8267A254u;
    string_property_initializer::FrameRegisters property{
        frame.lr, frame.r28, frame.r29, frame.r30, frame.r31};
    std::uint64_t property_result = 0;
    if (!string_property_initializer::Apply(0x82496948u, memory,
            resize_services, manager_services, object_register, 0,
            caller_sp, property, property_result))
        throw std::logic_error("string-property dependency missing");
    frame.lr = property.lr;
    frame.r28 = property.r28;
    frame.r29 = property.r29;
    frame.r30 = property.r30;
    frame.r31 = property.r31;
}

void NetConnection(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t object_register, std::uint64_t caller_sp,
    FrameRegisters& frame, FpEffects& effects, std::uint64_t& result)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    EnterFrame(memory, sp, frame,
        25, 176u, 0x8267A1F8u);
    const std::uint64_t nested_sp = caller_sp - 176u;
    frame.r31 = object_register;
    frame.r30 = 0;
    GuestAddress object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object + 60u, 0x82062CC0u);
    memory.WriteU32(object + 96u, 0x821898F0u);
    memory.WriteU32(object, 0x821F6B58u);
    memory.WriteU32(object + 60u, 0x821F6C9Cu);
    memory.WriteU32(object + 96u, 0x821F6CA0u);
    memory.WriteU32(object + 100u, 0);
    memory.WriteU32(object + 104u, 0);
    ApplyProperty(memory, resize_services, manager_services,
        frame.r31 + 108u, nested_sp, frame);

    object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object + 176u, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 192u, static_cast<GuestAddress>(frame.r30));
    frame.r29 = frame.r31 + 312u;
    memory.WriteU32(object + 196u, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 200u, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 204u, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 188u, 1u);
    const std::uint32_t global_word = memory.ReadU32(0x83235AA0u);
    memory.WriteU32(object + 212u, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 208u, global_word);
    for (GuestAddress offset = 228u; offset <= 248u; offset += 4u)
        memory.WriteU32(object + offset, static_cast<GuestAddress>(frame.r30));
    for (GuestAddress offset = 276u; offset <= 308u; offset += 4u)
        memory.WriteU32(object + offset, static_cast<GuestAddress>(frame.r30));
    frame.lr = 0x8267A2CCu;
    BitWriter(memory, resize_services, frame.r29, 0, nested_sp,
        frame, result);

    object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(static_cast<GuestAddress>(frame.r29), 0x821FEE28u);
    memory.WriteU32(object + 512u, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 548u, static_cast<GuestAddress>(frame.r30));

    fp_services.DisableFlushMode(); // lfd f0 is the first FP load.
    const std::uint64_t initial_f0 = ReadU64(memory, 0x82000FE8u);
    for (GuestAddress offset : {80u, 88u, 96u, 104u})
        WriteU64(memory, static_cast<GuestAddress>(nested_sp + offset),
            initial_f0);
    const LoadedSingle f13 = LoadedSingle::FromWord(
        memory.ReadU32(0x8218958Cu));
    effects.f13 = f13.FprValue();
    memory.WriteU32(object + 492u, f13.StoreWord());
    const LoadedSingle f0 = LoadedSingle::FromWord(
        memory.ReadU32(0x822184DCu));
    effects.f0 = f0.FprValue();
    for (GuestAddress offset : {496u, 500u, 504u, 508u})
        memory.WriteU32(object + offset, f0.StoreWord());

    // All eight stack words are loaded before the first of the eight object
    // stores. Keep that live read order when the object aliases this frame.
    std::array<std::uint32_t, 8> words{};
    for (unsigned index = 0; index < words.size(); ++index)
        words[index] = memory.ReadU32(static_cast<GuestAddress>(
            nested_sp + 80u + index * 4u));
    frame.r29 = words[3];
    frame.r28 = words[4];
    frame.r27 = words[5];
    frame.r26 = words[6];
    frame.r25 = words[7];
    for (unsigned index = 0; index < words.size(); ++index)
        memory.WriteU32(object + 516u + index * 4u, words[index]);
    frame.lr = 0x8267A378u;
    BitWriter(memory, resize_services, frame.r31 + 552u, 0,
        nested_sp, frame, result);

    object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object + 3760u, static_cast<GuestAddress>(frame.r30));
    result = frame.r31;
    memory.WriteU32(object + 3756u, 0xFFFFFFFFu);
    memory.WriteU32(object + 3764u, 0xFFFFFFFFu);
    for (GuestAddress offset = 20136u; offset <= 20196u; offset += 4u)
        memory.WriteU32(object + offset, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 20200u, 8u);
    for (GuestAddress offset = 20208u; offset <= 20228u; offset += 4u)
        memory.WriteU32(object + offset, static_cast<GuestAddress>(frame.r30));
    for (GuestAddress offset = 20240u; offset <= 20272u; offset += 4u)
        memory.WriteU32(object + offset, static_cast<GuestAddress>(frame.r30));
    for (GuestAddress offset = 20288u; offset <= 20316u; offset += 4u)
        memory.WriteU32(object + offset, static_cast<GuestAddress>(frame.r30));
    LeaveFrame(memory, sp, frame, 25);
}

void ChildConnection(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, FpEffects& effects, std::uint64_t& result)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    EnterDirectFrame(memory, sp, frame, 31, 96u);
    const std::uint64_t nested_sp = caller_sp - 96u;
    frame.r31 = incoming_r3;
    result = incoming_r3;
    if (static_cast<GuestAddress>(frame.r31) != 0)
    {
        frame.lr = 0x8267A060u;
        NetConnection(memory, resize_services, manager_services,
            fp_services, frame.r31, nested_sp, frame, effects, result);
        const GuestAddress object = static_cast<GuestAddress>(frame.r31);
        memory.WriteU32(object, 0x821F6F20u);
        memory.WriteU32(object + 60u, 0x821F6C9Cu);
        memory.WriteU32(object + 96u, 0x821F7064u);
    }
    LeaveDirectFrame(memory, sp, frame, 31);
}

void TcpipConnection(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, FpEffects& effects, std::uint64_t& result)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    EnterDirectFrame(memory, sp, frame, 30, 112u);
    const std::uint64_t nested_sp = caller_sp - 112u;
    frame.r31 = incoming_r3;
    result = incoming_r3;
    if (static_cast<GuestAddress>(frame.r31) != 0)
    {
        frame.lr = 0x8272CBFCu;
        NetConnection(memory, resize_services, manager_services,
            fp_services, frame.r31, nested_sp, frame, effects, result);
        const GuestAddress object = static_cast<GuestAddress>(frame.r31);
        frame.r30 = frame.r31 + 20320u;
        memory.WriteU32(object, 0x82210890u);
        memory.WriteU32(object + 60u, 0x821F6C9Cu);
        memory.WriteU32(object + 96u, 0x822109D4u);
        frame.lr = 0x8272CC34u;
        (void)FillGuestMemory(memory, static_cast<GuestAddress>(frame.r30),
            0, 16);
        result = frame.r30; // Fill preserves full incoming r3.
        memory.WriteU16(static_cast<GuestAddress>(frame.r30), 2);
    }
    LeaveDirectFrame(memory, sp, frame, 30);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, FpEffects& effects,
    std::uint64_t& result)
{
    switch (address)
    {
    case kBitWriter:
        BitWriter(memory, resize_services, incoming_r3, incoming_r4,
            caller_sp, frame, result);
        return true;
    case kNet:
        NetConnection(memory, resize_services, manager_services, fp_services,
            incoming_r3, caller_sp, frame, effects, result);
        return true;
    case kNullTail:
        if (static_cast<GuestAddress>(incoming_r3) == 0)
            result = incoming_r3;
        else
            NetConnection(memory, resize_services, manager_services, fp_services,
                incoming_r3, caller_sp, frame, effects, result);
        return true;
    case kChild:
        ChildConnection(memory, resize_services, manager_services, fp_services,
            incoming_r3, caller_sp, frame, effects, result);
        return true;
    case kTcpip:
        TcpipConnection(memory, resize_services, manager_services, fp_services,
            incoming_r3, caller_sp, frame, effects, result);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::instance_connection_initializer_family
