#include "lo_semantics/instance_string_initializer_family.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_string_initializer_family
{
namespace
{
struct Spec
{
    GuestAddress address;
    bool tail;
    GuestAddress first_vtable;
    GuestAddress base_vtable;
    GuestAddress final_vtable;
    std::uint64_t tail_source;
};

constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE STRING PARAMETERS
    {0x82407300u, false, 0x821898F0u, 0x82190E90u, 0x82190F98u, 0xFFFFFFFF821A83D0ull},
    {0x8240AE28u, true, 0, 0, 0, 0xFFFFFFFF821A83D0ull},
    // END GENERATED INSTANCE STRING PARAMETERS
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

Result Initialize(const Spec& spec, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t object_register,
    std::uint64_t source_register, const EntryAbi& abi)
{
    const GuestAddress sp = static_cast<GuestAddress>(abi.caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(abi.incoming_lr));
    WriteU64(memory, sp - 16u, abi.incoming_r31);
    const GuestAddress frame_sp = sp - 96u;
    memory.WriteU32(frame_sp, sp);

    const GuestAddress object = static_cast<GuestAddress>(object_register);
    memory.WriteU32(object + 60u, spec.first_vtable);
    memory.WriteU32(object, spec.base_vtable);
    memory.WriteU32(object + 60u, spec.final_vtable);
    (void)registered_metadata_string::InitializeString(memory, services,
        object_register + 72u, source_register, frame_sp);

    // mr r3,r31 occurs after the string helper; the epilogue reloads r31
    // from guest memory, which may have been aliased by the object writes.
    return {object_register, memory.ReadU32(sp - 8u),
            ReadU64(memory, sp - 16u)};
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, const EntryAbi& abi, Result& result)
{
    // Both bodies overwrite r4 with the same sign-extended source literal.
    (void)incoming_r4;
    const Spec* spec = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& candidate, GuestAddress target)
        { return candidate.address < target; });
    if (spec == std::end(kSpecs) || spec->address != address)
        return false;
    if (spec->tail && static_cast<GuestAddress>(incoming_r3) == 0)
    {
        result = {incoming_r3, abi.incoming_lr, abi.incoming_r31};
        return true;
    }
    if (spec->tail)
    {
        const Spec* direct = std::find_if(std::begin(kSpecs), std::end(kSpecs),
            [](const Spec& entry) { return !entry.tail; });
        result = Initialize(*direct, memory, services, incoming_r3,
            spec->tail_source, abi);
    }
    else
        result = Initialize(*spec, memory, services, incoming_r3,
            spec->tail_source, abi);
    return true;
}

} // namespace lo::semantic::gpu::instance_string_initializer_family
