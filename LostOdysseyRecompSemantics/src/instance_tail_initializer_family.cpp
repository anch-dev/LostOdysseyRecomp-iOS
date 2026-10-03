#include "lo_semantics/instance_tail_initializer_family.h"

#include "lo_semantics/memory_fill.h"
#include "lo_semantics/pointer_fields.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace lo::semantic::gpu::instance_tail_initializer_family
{
namespace
{
struct Entry
{
    GuestAddress address;
    GuestAddress callee;
    bool null_guard;
};

constexpr Entry kEntries[] = {
    // BEGIN GENERATED INSTANCE TAIL ENTRIES
    {0x82419178u, 0x82419178u, false},
    {0x824192a0u, 0x82419178u, true},
    {0x8255dbc0u, 0x8255dbd0u, true},
    {0x8255dbd0u, 0x8255dbd0u, false},
    {0x825fc400u, 0x825fc498u, true},
    {0x825fc498u, 0x825fc498u, false},
    {0x826b2850u, 0x826b2850u, false},
    {0x826f89c8u, 0x826b2850u, true},
    {0x827ce228u, 0x825a7f18u, true},
    // END GENERATED INSTANCE TAIL ENTRIES
};

void InitializeLinker(GuestMemory& memory, GuestAddress object)
{
    memory.WriteU32(object, 0x82191cb0u);
    for (GuestAddress offset : {80u, 84u, 88u, 136u, 140u, 144u,
                                160u, 164u, 168u})
        memory.WriteU32(object + offset, 0);
    (void)FillGuestMemory(memory, object + 64u, 0, 108);
    for (GuestAddress offset = 172u; offset <= 228u; offset += 4u)
        memory.WriteU32(object + offset, 0);
}

void InitializePolys(GuestMemory& memory, ArrayResizeServices& services,
    GuestAddress object)
{
    memory.WriteU32(object, 0x821ff128u);
    const GuestAddress array = object + 60u;
    memory.WriteU32(array, 0);
    memory.WriteU32(array + 4u, 0);
    memory.WriteU32(array + 8u, 0);
    ResizeArray(memory, services, array, 100, 8);
    memory.WriteU32(array + 12u, object);
}

void InitializeMaterialPointer(GuestMemory& memory, std::uint64_t incoming_r3)
{
    PointerFieldRegisters registers{};
    registers.r3 = incoming_r3;
    constexpr std::array assignments{
        ConstantFieldAssignment{PointerFieldRegister::R11, 0x821da0a0u},
    };
    constexpr std::array writes{
        ConstantFieldWrite{0, PointerFieldWidth::Word, 0x821da0a0u},
    };
    InitializeConstantFields(memory, registers, PointerFieldRegister::R3,
        assignments, writes);
}

void InitializeSkeletalMesh(GuestMemory& memory, GuestAddress object,
    GuestAddress caller_sp)
{
    // BEGIN GENERATED INSTANCE TAIL STAGED INITIALIZERS
    memory.WriteU32(object + 0u, 0x821cb820u);
    memory.WriteU32(caller_sp - 28u, 0x00000000u);
    memory.WriteU32(caller_sp - 24u, 0x00000000u);
    memory.WriteU32(caller_sp - 20u, 0x00000000u);
    memory.WriteU32(caller_sp - 16u, 0x00000000u);
    memory.WriteU32(caller_sp - 12u, 0x00000000u);
    memory.WriteU32(caller_sp - 8u, 0x00000000u);
    memory.WriteU32(object + 88u, 0x00000000u);
    memory.WriteU32(caller_sp - 4u, 0x00000000u);
    memory.WriteU32(caller_sp - 32u, 0x00000000u);
    memory.WriteU32(object + 92u, 0x00000000u);
    const std::uint32_t staged_1 = memory.ReadU32(caller_sp - 28u);
    memory.WriteU32(object + 96u, staged_1);
    const std::uint32_t staged_2 = memory.ReadU32(caller_sp - 24u);
    memory.WriteU32(object + 124u, staged_2);
    const std::uint32_t staged_3 = memory.ReadU32(caller_sp - 20u);
    memory.WriteU32(object + 128u, staged_3);
    const std::uint32_t staged_4 = memory.ReadU32(caller_sp - 16u);
    memory.WriteU32(object + 132u, staged_4);
    const std::uint32_t staged_5 = memory.ReadU32(caller_sp - 12u);
    memory.WriteU32(object + 156u, 0x00000008u);
    memory.WriteU32(object + 140u, staged_5);
    const std::uint32_t staged_6 = memory.ReadU32(caller_sp - 8u);
    memory.WriteU32(object + 144u, staged_6);
    const std::uint32_t staged_7 = memory.ReadU32(caller_sp - 4u);
    memory.WriteU32(object + 148u, staged_7);
    memory.WriteU32(object + 152u, 0x00000000u);
    memory.WriteU32(object + 160u, 0x00000000u);
    memory.WriteU32(object + 164u, 0x00000000u);
    memory.WriteU32(object + 168u, 0x00000000u);
    memory.WriteU32(object + 172u, 0x00000000u);
    memory.WriteU32(object + 176u, 0x00000000u);
    memory.WriteU32(object + 180u, 0x00000000u);
    memory.WriteU32(object + 184u, 0x00000000u);
    memory.WriteU32(object + 188u, 0x00000000u);
    memory.WriteU32(object + 192u, 0x00000000u);
    memory.WriteU32(object + 200u, 0x00000000u);
    memory.WriteU32(object + 204u, 0x00000000u);
    memory.WriteU32(object + 208u, 0x00000000u);
    memory.WriteU32(object + 212u, 0x00000000u);
    memory.WriteU32(object + 216u, 0x00000000u);
    memory.WriteU32(object + 220u, 0x00000000u);
    memory.WriteU32(object + 244u, 0x00000000u);
    memory.WriteU32(object + 248u, 0x00000000u);
    memory.WriteU32(object + 252u, 0x00000000u);
    memory.WriteU32(object + 256u, 0x00000000u);
    memory.WriteU32(object + 260u, 0x00000000u);
    memory.WriteU32(object + 264u, 0x00000000u);
    memory.WriteU32(object + 268u, 0x00000000u);
    memory.WriteU32(object + 272u, 0x00000000u);
    memory.WriteU32(object + 276u, 0x00000000u);
    memory.WriteU32(object + 284u, 0x00000000u);
    memory.WriteU32(object + 288u, 0x00000000u);
    memory.WriteU32(object + 292u, 0x00000000u);
    memory.WriteU32(object + 296u, 0x00000000u);
    memory.WriteU32(object + 300u, 0x00000000u);
    memory.WriteU32(object + 304u, 0x00000000u);
    memory.WriteU32(object + 340u, 0x00000000u);
    // END GENERATED INSTANCE TAIL STAGED INITIALIZERS
}

void InitializeSoundNodeWave(GuestMemory& memory, GuestAddress object,
    GuestAddress caller_sp)
{
    // BEGIN GENERATED INSTANCE TAIL SOUND INITIALIZER
    memory.WriteU32(caller_sp - 32u, 0x00000000u);
    memory.WriteU32(object + 0u, 0x821e1c00u);
    memory.WriteU32(object + 132u, 0x00000000u);
    memory.WriteU32(caller_sp - 20u, 0x00000000u);
    memory.WriteU32(caller_sp - 4u, 0x821a9edcu);
    memory.WriteU32(caller_sp - 28u, 0xffffffffu);
    memory.WriteU32(caller_sp - 24u, 0xffffffffu);
    memory.WriteU32(caller_sp - 16u, 0xffffffffu);
    memory.WriteU32(caller_sp - 12u, 0xffffffffu);
    memory.WriteU32(caller_sp - 8u, 0xffffffffu);
    memory.WriteU32(object + 136u, 0x00000000u);
    const std::uint32_t staged_1 = memory.ReadU32(caller_sp - 4u);
    const std::uint32_t staged_2 = memory.ReadU32(caller_sp - 28u);
    memory.WriteU32(object + 128u, staged_1);
    memory.WriteU32(object + 140u, staged_2);
    const std::uint32_t staged_3 = memory.ReadU32(caller_sp - 24u);
    memory.WriteU32(object + 144u, staged_3);
    memory.WriteU32(object + 148u, 0x00000000u);
    const std::uint32_t staged_4 = memory.ReadU32(caller_sp - 16u);
    memory.WriteU32(object + 152u, staged_4);
    const std::uint32_t staged_5 = memory.ReadU32(caller_sp - 12u);
    memory.WriteU32(object + 156u, staged_5);
    const std::uint32_t staged_6 = memory.ReadU32(caller_sp - 8u);
    memory.WriteU32(object + 160u, staged_6);
    memory.WriteU32(object + 164u, 0x00000000u);
    memory.WriteU32(object + 168u, 0x00000000u);
    memory.WriteU32(object + 172u, 0x00000000u);
    memory.WriteU32(object + 184u, 0x00000000u);
    memory.WriteU32(object + 188u, 0x00000000u);
    memory.WriteU32(object + 192u, 0xffffffffu);
    memory.WriteU32(object + 196u, 0xffffffffu);
    memory.WriteU32(object + 200u, 0x00000000u);
    memory.WriteU32(object + 204u, 0xffffffffu);
    memory.WriteU32(object + 208u, 0xffffffffu);
    memory.WriteU32(object + 212u, 0xffffffffu);
    memory.WriteU32(object + 216u, 0x00000000u);
    memory.WriteU32(object + 220u, 0x00000000u);
    memory.WriteU32(object + 224u, 0x00000000u);
    memory.WriteU32(object + 180u, staged_1);
    memory.WriteU32(object + 232u, 0x00000000u);
    memory.WriteU32(object + 236u, 0x00000000u);
    memory.WriteU32(object + 240u, 0xffffffffu);
    memory.WriteU32(object + 244u, 0xffffffffu);
    memory.WriteU32(object + 248u, 0x00000000u);
    memory.WriteU32(object + 252u, 0xffffffffu);
    memory.WriteU32(object + 256u, 0xffffffffu);
    memory.WriteU32(object + 260u, 0xffffffffu);
    memory.WriteU32(object + 264u, 0x00000000u);
    memory.WriteU32(object + 268u, 0x00000000u);
    memory.WriteU32(object + 272u, 0x00000000u);
    memory.WriteU32(object + 228u, staged_1);
    memory.WriteU32(object + 280u, 0x00000000u);
    memory.WriteU32(object + 284u, 0x00000000u);
    memory.WriteU32(object + 288u, 0xffffffffu);
    memory.WriteU32(object + 292u, 0xffffffffu);
    memory.WriteU32(object + 296u, 0x00000000u);
    memory.WriteU32(object + 300u, 0xffffffffu);
    memory.WriteU32(object + 304u, 0xffffffffu);
    memory.WriteU32(object + 308u, 0xffffffffu);
    memory.WriteU32(object + 312u, 0x00000000u);
    memory.WriteU32(object + 316u, 0x00000000u);
    memory.WriteU32(object + 320u, 0x00000000u);
    memory.WriteU32(object + 276u, staged_1);
    // END GENERATED INSTANCE TAIL SOUND INITIALIZER
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, std::uint64_t& result)
{
    const Entry* entry = std::lower_bound(std::begin(kEntries), std::end(kEntries),
        address, [](const Entry& current, GuestAddress target)
        { return current.address < target; });
    if (entry == std::end(kEntries) || entry->address != address)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    if (!entry->null_guard || object != 0)
    {
        switch (entry->callee)
        {
        case 0x82419178u: InitializeLinker(memory, object); break;
        case 0x8255dbd0u: InitializeSkeletalMesh(memory, object, caller_sp); break;
        case 0x825a7f18u: InitializeMaterialPointer(memory, incoming_r3); break;
        case 0x825fc498u: InitializeSoundNodeWave(memory, object, caller_sp); break;
        case 0x826b2850u: InitializePolys(memory, resize_services, object); break;
        }
    }
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_tail_initializer_family
