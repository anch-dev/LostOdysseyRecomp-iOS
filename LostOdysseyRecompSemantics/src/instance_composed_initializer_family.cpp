#include "lo_semantics/instance_composed_initializer_family.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/registered_metadata_words.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>

namespace lo::semantic::gpu::instance_composed_initializer_family
{
namespace
{
struct Spec
{
    GuestAddress address;
    GuestAddress callee;
    bool null_guard;
    GuestAddress wrapper_vtable;
    GuestAddress wrapper_inner_vtable;
};

constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE COMPOSED PARAMETERS
    {0x825a56d0u, 0x825a56d0u, false, 0x00000000u, 0x00000000u},
    {0x825a80b0u, 0x825a56d0u, true, 0x00000000u, 0x00000000u},
    {0x825e0928u, 0x826db6a8u, true, 0x00000000u, 0x00000000u},
    {0x8262e498u, 0x825a56d0u, false, 0x821e8910u, 0x00000000u},
    {0x826b50b8u, 0x826b50c8u, true, 0x00000000u, 0x00000000u},
    {0x826b50c8u, 0x826b50c8u, false, 0x00000000u, 0x00000000u},
    {0x826d6c20u, 0x826d6c30u, true, 0x00000000u, 0x00000000u},
    {0x826d6c30u, 0x826d6c30u, false, 0x00000000u, 0x00000000u},
    {0x826db6a8u, 0x826db6a8u, false, 0x00000000u, 0x00000000u},
    {0x82716058u, 0x826db6a8u, false, 0x8220cbb8u, 0x00000000u},
    {0x8272ce60u, 0x826b50c8u, false, 0x82210b68u, 0x82210c78u},
    // END GENERATED INSTANCE COMPOSED PARAMETERS
};

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    const std::uint64_t high = memory.ReadU32(address);
    return (high << 32) | memory.ReadU32(address + 4u);
}

GuestAddress EnterFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& registers, GuestAddress size, bool save_r30, bool save_r31)
{
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(registers.lr));
    if (save_r30)
        WriteU64(memory, sp - 24u, registers.r30);
    if (save_r31)
        WriteU64(memory, sp - 16u, registers.r31);
    memory.WriteU32(sp - size, sp);
    return sp - size;
}

void LeaveFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& registers, bool restore_r30, bool restore_r31)
{
    registers.lr = memory.ReadU32(sp - 8u);
    if (restore_r30)
        registers.r30 = ReadU64(memory, sp - 24u);
    if (restore_r31)
        registers.r31 = ReadU64(memory, sp - 16u);
}

std::uint64_t InitializeMaterial(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress object, GuestAddress sp,
    FrameRegisters& frame, std::uint64_t full_r3)
{
    const GuestAddress nested_sp = EnterFrame(memory, sp, frame, 112u, true, true);
    frame.r31 = full_r3;
    frame.r30 = 0;
    // The 64-bit load precedes the vtable write. rlwinm uses its low word.
    (void)memory.ReadU32(object + 8u);
    const bool already_initialized = (memory.ReadU32(object + 12u) & 0x200u) != 0;
    memory.WriteU32(object, 0x82009420u);
    if (!already_initialized)
    {
        frame.lr = 0x825a5710u;
        const std::uint64_t allocation = AllocateManagerBuffer(memory, services,
            12, nested_sp);
        GuestAddress created = 0;
        if (static_cast<GuestAddress>(allocation) != 0)
        {
            created = static_cast<GuestAddress>(allocation);
            memory.WriteU32(created + 4u, object);
            memory.WriteU32(created + 8u, 0);
            memory.WriteU32(created, 0x82001694u);
        }
        memory.WriteU32(object + 548u, created);
    }
    memory.WriteU32(object + 540u, 0);
    memory.WriteU32(object + 544u, 0);
    LeaveFrame(memory, sp, frame, true, true);
    return full_r3;
}

std::uint64_t InitializeComponent(GuestMemory& memory,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    GuestAddress object, std::uint64_t full_r3)
{
    fp_services.DisableFlushMode();
    const LoadedSingle first = LoadedSingle::FromWord(memory.ReadU32(0x8218958cu));
    const LoadedSingle second = LoadedSingle::FromWord(memory.ReadU32(0x82000e50u));
    memory.WriteU32(object + 96u, 0);
    for (GuestAddress offset : {368u, 372u, 376u, 476u, 480u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 496u, first.StoreWord());
    memory.WriteU32(object + 500u, second.StoreWord());
    memory.WriteU32(object, 0x8200c930u);
    for (GuestAddress offset : {504u, 508u, 512u})
        memory.WriteU32(object + offset, second.StoreWord());
    memory.WriteU32(object + 516u, first.StoreWord());
    for (GuestAddress offset : {520u, 524u, 528u, 532u})
        memory.WriteU32(object + offset, second.StoreWord());
    memory.WriteU32(object + 536u, first.StoreWord());
    for (GuestAddress offset : {540u, 544u, 548u, 552u})
        memory.WriteU32(object + offset, second.StoreWord());
    memory.WriteU32(object + 556u, first.StoreWord());
    for (GuestAddress offset : {700u, 704u, 708u, 712u, 716u, 720u})
        memory.WriteU32(object + offset, 0);
    return full_r3;
}

std::uint64_t InitializeModel(GuestMemory& memory,
    ArrayResizeServices& services, GuestAddress object, GuestAddress sp,
    FrameRegisters& frame, std::uint64_t full_r3)
{
    const GuestAddress nested_sp = EnterFrame(memory, sp, frame, 112u, true, true);
    frame.r31 = full_r3;
    frame.r30 = full_r3 + 60u;
    const GuestAddress inner = object + 60u;
    memory.WriteU32(object, 0x8218d6f8u);
    memory.WriteU32(inner, 0x8205f03cu);
    const bool append = memory.ReadU32(0x83313660u) == 0;
    if (append)
    {
        const GuestAddress outgoing = nested_sp + 80u;
        memory.WriteU32(outgoing, inner);
        frame.lr = 0x826b5120u;
        (void)registered_metadata_words::AppendMetadataWord(memory, services,
            0x8336b110u, outgoing);
    }
    memory.WriteU32(object, 0x821ff7d0u);
    memory.WriteU32(inner, 0x821ff8e0u);
    for (GuestAddress offset = 64u; offset <= 116u; offset += 4u)
        memory.WriteU32(object + offset, 0);
    LeaveFrame(memory, sp, frame, true, true);
    return full_r3;
}

std::uint64_t InitializeTexture(GuestMemory& memory,
    ArrayResizeServices& services, GuestAddress object, GuestAddress sp,
    FrameRegisters& frame, std::uint64_t full_r3)
{
    const GuestAddress nested_sp = EnterFrame(memory, sp, frame, 112u, true, true);
    frame.r31 = full_r3;
    frame.r30 = full_r3 + 244u;
    const GuestAddress inner = object + 244u;
    memory.WriteU32(object + 96u, 0x821a9edcu);
    memory.WriteU32(object + 108u, 0xffffffffu);
    memory.WriteU32(object + 112u, 0xffffffffu);
    for (GuestAddress offset : {100u, 104u, 116u, 132u, 136u, 140u})
        memory.WriteU32(object + offset, 0);
    for (GuestAddress offset : {120u, 124u, 128u})
        memory.WriteU32(object + offset, 0xffffffffu);
    memory.WriteU32(object, 0x82004d68u);
    for (GuestAddress offset : {188u, 192u, 196u, 224u, 232u, 236u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(inner, 0x8205f03cu);
    const bool append = memory.ReadU32(0x83313660u) == 0;
    if (append)
    {
        const GuestAddress outgoing = nested_sp + 80u;
        memory.WriteU32(outgoing, inner);
        frame.lr = 0x826d6ce0u;
        (void)registered_metadata_words::AppendMetadataWord(memory, services,
            0x8336b110u, outgoing);
    }
    memory.WriteU32(object, 0x82202638u);
    memory.WriteU32(inner, 0x8220277cu);
    LeaveFrame(memory, sp, frame, true, true);
    return full_r3;
}

std::uint64_t Run(GuestAddress callee, GuestMemory& memory,
    ManagerFacadeServices& manager_services, ArrayResizeServices& array_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t full_r3, GuestAddress sp, FrameRegisters& frame)
{
    const GuestAddress object = static_cast<GuestAddress>(full_r3);
    switch (callee)
    {
    case 0x825a56d0u: return InitializeMaterial(memory, manager_services,
                                object, sp, frame, full_r3);
    case 0x826db6a8u: return InitializeComponent(memory, fp_services,
                                object, full_r3);
    case 0x826b50c8u: return InitializeModel(memory, array_services,
                                object, sp, frame, full_r3);
    case 0x826d6c30u: return InitializeTexture(memory, array_services,
                                object, sp, frame, full_r3);
    default: return full_r3;
    }
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services, ArrayResizeServices& array_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, GuestAddress caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    const Spec* spec = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& current, GuestAddress target)
        { return current.address < target; });
    if (spec == std::end(kSpecs) || spec->address != address)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    if (spec->null_guard && object == 0)
    {
        result = incoming_r3;
        return true;
    }
    const bool framed = spec->wrapper_vtable != 0;
    if (framed)
    {
        const bool save_r31 = address != 0x82716058u;
        (void)EnterFrame(memory, caller_sp, frame, 96u, false, save_r31);
        if (save_r31)
            frame.r31 = incoming_r3;
        if (object != 0)
        {
            frame.lr = address == 0x8262e498u ? 0x8262e4b8u :
                       address == 0x82716058u ? 0x82716070u : 0x8272ce80u;
            result = Run(spec->callee, memory, manager_services, array_services,
                fp_services, incoming_r3, caller_sp - 96u, frame);
            const GuestAddress live_object = save_r31 ?
                static_cast<GuestAddress>(frame.r31) : static_cast<GuestAddress>(result);
            memory.WriteU32(live_object, spec->wrapper_vtable);
            if (spec->wrapper_inner_vtable != 0)
                memory.WriteU32(live_object + 60u, spec->wrapper_inner_vtable);
        }
        else
            result = incoming_r3;
        LeaveFrame(memory, caller_sp, frame, false, save_r31);
    }
    else
        result = Run(spec->callee, memory, manager_services, array_services,
            fp_services, incoming_r3, caller_sp, frame);
    return true;
}

} // namespace lo::semantic::gpu::instance_composed_initializer_family
