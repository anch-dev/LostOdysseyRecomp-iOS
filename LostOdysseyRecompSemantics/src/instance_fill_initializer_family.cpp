#include "lo_semantics/instance_fill_initializer_family.h"
#include "lo_semantics/memory_fill.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_fill_initializer_family
{
namespace
{
struct Spec
{
    GuestAddress address;
    GuestAddress vtable;
    std::uint32_t field_offset;
    bool framed;
};

constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE FILL PARAMETERS
    {0x8264e920u, 0x821ec1a0u, 60u, false},
    {0x82667430u, 0x821f0f88u, 72u, false},
    {0x826674b0u, 0x821f1210u, 72u, true},
    {0x82667568u, 0x821f1100u, 72u, true},
    // END GENERATED INSTANCE FILL PARAMETERS
};

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
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, const EntryAbi& abi, Result& result)
{
    const Spec* spec = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& entry, GuestAddress target)
        { return entry.address < target; });
    if (spec == std::end(kSpecs) || spec->address != address)
        return false;

    const GuestAddress sp = static_cast<GuestAddress>(abi.caller_sp);
    if (spec->framed)
    {
        memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(abi.incoming_lr));
        WriteU64(memory, sp - 16u, abi.incoming_r31);
        memory.WriteU32(sp - 96u, sp);
    }

    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    std::uint64_t returned_r3 = incoming_r3;
    if (object != 0)
    {
        returned_r3 += spec->field_offset;
        if (!spec->framed)
            memory.WriteU32(object, spec->vtable);
        (void)FillGuestMemory(memory, static_cast<GuestAddress>(returned_r3),
            0u, 100u);
        if (spec->framed)
            memory.WriteU32(object, spec->vtable);
    }

    result = {returned_r3,
        spec->framed ? memory.ReadU32(sp - 8u) : abi.incoming_lr,
        spec->framed ? ReadU64(memory, sp - 16u) : abi.incoming_r31};
    return true;
}

} // namespace lo::semantic::gpu::instance_fill_initializer_family
