#include "lo_semantics/instance_navigation_initializer_family.h"

#include "lo_semantics/pointer_fields.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace lo::semantic::gpu::instance_navigation_initializer_family
{
namespace
{
struct Spec
{
    GuestAddress address;
    GuestAddress callee;
    GuestAddress lower;
    GuestAddress base_vtable;
    GuestAddress derived_vtable;
    bool volume_scratch;
};

// The generator validates each complete generated PPC body and dependency.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE NAVIGATION PARAMETERS
    {0x82b55028u, 0x82b54518u, 0x824d5aa8u, 0x8207acccu, 0x820165b0u, false},
    {0x82b55068u, 0x82b54690u, 0x825295b0u, 0x821c64f0u, 0x820cf850u, false},
    {0x82b550a8u, 0x82b548f8u, 0x824d83a0u, 0x821afcf8u, 0x820cfbd0u, false},
    {0x82b550e8u, 0x82b54980u, 0x82b548f8u, 0x00000000u, 0x820d0008u, false},
    {0x82b55128u, 0x82b54af8u, 0x8249ea88u, 0x8206a974u, 0x820d0440u, false},
    {0x82b55168u, 0x82b54c70u, 0x8249ea88u, 0x8206a974u, 0x820d07c0u, false},
    {0x82b551a8u, 0x82b54de8u, 0x825ba8f8u, 0x8200487cu, 0x820d0b40u, false},
    {0x82b56728u, 0x82b55ff8u, 0x824d5aa8u, 0x8207acccu, 0x820d1000u, true},
    // END GENERATED INSTANCE NAVIGATION PARAMETERS
};

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

void InitializeBase(GuestMemory& memory, std::uint64_t object, GuestAddress base_vtable)
{
    PointerFieldRegisters registers{};
    registers.r3 = object;
    const std::array assignments = {
        ConstantFieldAssignment{PointerFieldRegister::R11, base_vtable},
    };
    const std::array writes = {
        ConstantFieldWrite{0, PointerFieldWidth::Word, base_vtable},
    };
    InitializeConstantFields(memory, registers, PointerFieldRegister::R3,
                             assignments, writes);
}

void RunCallee(const Spec& spec, GuestMemory& memory, std::uint64_t object,
               GuestAddress caller_sp, std::uint64_t incoming_lr,
               std::uint64_t incoming_r30, std::uint64_t incoming_r31)
{
    memory.WriteU32(caller_sp - 8u, static_cast<std::uint32_t>(incoming_lr));
    WriteU64(memory, caller_sp - 24u, incoming_r30);
    WriteU64(memory, caller_sp - 16u, incoming_r31);
    const GuestAddress frame = caller_sp - 112u;
    memory.WriteU32(frame, caller_sp);
    memory.WriteU32(frame + 132u, static_cast<GuestAddress>(object));

    if (spec.lower == 0x82b548f8u)
    {
        // InsteadPawnWSM derives from the recovered InsteadPawn initializer.
        const Spec* parent = std::find_if(std::begin(kSpecs), std::end(kSpecs),
            [](const Spec& candidate) { return candidate.callee == 0x82b548f8u; });
        RunCallee(*parent, memory, object, frame, spec.callee + 44u, object, frame);
    }
    else
    {
        InitializeBase(memory, object, spec.base_vtable);
    }

    if (spec.volume_scratch)
        memory.WriteU32(frame + 80u, static_cast<GuestAddress>(object + 576u));
    memory.WriteU32(static_cast<GuestAddress>(object), spec.derived_vtable);
    if (spec.volume_scratch)
    {
        for (const std::uint32_t offset : {588u, 616u, 628u, 640u, 652u, 664u, 676u})
            memory.WriteU32(frame + 80u, static_cast<GuestAddress>(object + offset));
    }
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory, std::uint64_t incoming_r3,
           const EntryAbi& abi, std::uint64_t& result)
{
    const Spec* wrapper = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& spec, GuestAddress target) { return spec.address < target; });
    if (wrapper != std::end(kSpecs) && wrapper->address == address)
    {
        memory.WriteU32(abi.caller_sp - 8u, static_cast<std::uint32_t>(abi.incoming_lr));
        WriteU64(memory, abi.caller_sp - 16u, abi.incoming_r31);
        const GuestAddress frame = abi.caller_sp - 96u;
        memory.WriteU32(frame, abi.caller_sp);
        if (static_cast<GuestAddress>(incoming_r3) != 0)
            RunCallee(*wrapper, memory, incoming_r3, frame, address + 40u,
                      abi.incoming_r30, incoming_r3);
        result = incoming_r3;
        return true;
    }

    const Spec* callee = std::find_if(std::begin(kSpecs), std::end(kSpecs),
        [address](const Spec& spec) { return spec.callee == address; });
    if (callee == std::end(kSpecs))
        return false;
    RunCallee(*callee, memory, incoming_r3, abi.caller_sp, abi.incoming_lr,
              abi.incoming_r30, abi.incoming_r31);
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_navigation_initializer_family
