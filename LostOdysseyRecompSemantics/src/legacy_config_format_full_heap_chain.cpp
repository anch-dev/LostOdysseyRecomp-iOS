#include "lo_semantics/legacy_config_format_full_heap_chain.h"

#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::legacy_config_format_full_heap_chain
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using RawRegisters = raw_allocation_context::Registers;

void Save27(GuestMemory& memory, Registers& state)
{
    state.r[12] = state.lr;
    for (unsigned index = 27u; index <= 31u; ++index)
        WriteU64(memory, Address(state.sp - 8u * (33u - index)),
            state.r[index]);
    memory.WriteU32(Address(state.sp - 8u), Address(state.r[12]));
}

void Restore27(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 27u; index <= 31u; ++index)
        state.r[index] = ReadU64(memory,
            Address(state.sp - 8u * (33u - index)));
    state.r[12] = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r[12];
}

RawRegisters ToRaw(const Registers& state)
{
    RawRegisters lower{};
    lower.r = state.r;
    lower.r[1] = state.sp;
    lower.lr = state.lr;
    lower.ctr = state.ctr;
    lower.xer_so = state.xer_so;
    lower.xer_ca = state.xer_ca;
    lower.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq, state.cr0.so};
    lower.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so};
    return lower;
}

void FromRaw(Registers& state, const RawRegisters& lower)
{
    state.r = lower.r;
    state.sp = lower.r[1];
    state.r[1] = 0u; // The accepted caller stores the live SP separately.
    state.lr = lower.lr;
    state.ctr = lower.ctr;
    state.xer_so = lower.xer_so;
    state.xer_ca = lower.xer_ca;
    state.cr0 = {lower.cr0.lt, lower.cr0.gt, lower.cr0.eq, lower.cr0.un};
    state.cr6 = {lower.cr6.lt, lower.cr6.gt, lower.cr6.eq, lower.cr6.un};
}

class RawHeapBoundary final : public raw_allocation_context::PpcBoundaryServices
{
public:
    explicit RawHeapBoundary(heap_allocation_context::BoundaryServices& heap)
        : heap_(heap) {}

    void CallDirect(GuestAddress entry, GuestMemory& memory,
        RawRegisters& state) override
    {
        if (entry != 0x823accb0u ||
            !heap_allocation_context::Apply(entry, memory, heap_, state))
            throw std::logic_error("unselected raw allocation direct call");
    }

private:
    heap_allocation_context::BoundaryServices& heap_;
};

class FullHeapReallocate final : public legacy_config_format_dispatch::ReallocateCalls
{
public:
    explicit FullHeapReallocate(heap_allocation_context::BoundaryServices& heap)
        : heap_(heap) {}

    void Call(GuestMemory& memory, Registers& state) override
    {
        if (Address(state.r[3]) != 0u)
            throw std::logic_error("non-null CRT reallocate branch is outside the selected chain");

        // Actual null-buffer branch of 823ACAD8: save, frame, call raw
        // 823ACBD0, then restore. The raw callee invokes the complete heap
        // 823ACCB0 and cleanup 823AD544 selected-context bodies.
        const auto caller_sp = Address(state.sp);
        Save27(memory, state);
        state.lr = 0x823acae0u;
        memory.WriteU32(Address(state.sp - 128u), caller_sp);
        state.sp -= 128u;
        state.r[28] = state.r[3];
        state.r[31] = state.r[4];
        state.cr6 = {0u, 0u, 1u, state.xer_so};
        state.r[3] = state.r[31];
        state.lr = 0x823acafcu;
        auto raw = ToRaw(state);
        RawHeapBoundary boundary(heap_);
        if (!raw_allocation_context::Apply(0x823acbd0u, memory, boundary, raw))
            throw std::logic_error("raw allocation selected context missing");
        FromRaw(state, raw);
        state.sp += 128u;
        Restore27(memory, state);
    }

private:
    heap_allocation_context::BoundaryServices& heap_;
};
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x824790a8u) return false;
    FullHeapReallocate reallocate(dependencies.heap);
    dependencies.caller.reallocate = &reallocate;
    return legacy_config_format_dispatch::Apply(entry, memory,
        dependencies.caller, registers);
}
} // namespace lo::semantic::gpu::legacy_config_format_full_heap_chain
