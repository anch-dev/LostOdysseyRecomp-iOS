#include "lo_semantics/crt_record_allocation_context.h"

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_record_allocation_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_async_status_transfer::Condition;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.r[index]; }
std::uint32_t Word(std::uint64_t value) { return Address(value); }
std::int32_t Signed(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(Word(value)); }

void Compare(Condition& cr, std::uint64_t lhs, std::uint64_t rhs,
    std::uint8_t so, bool signed_words)
{
    if (signed_words)
    {
        const auto a = Signed(lhs), b = Signed(rhs);
        cr = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), so};
    }
    else
    {
        const auto a = Word(lhs), b = Word(rhs);
        cr = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), so};
    }
}

void Push(GuestMemory& memory, Registers& state, unsigned bytes)
{
    const auto old_sp = R(state, 1);
    R(state, 1) -= bytes;
    memory.WriteU32(Address(R(state, 1)), Address(old_sp));
}

void Save28(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    state.lr = 0x82b816a8u;
    for (unsigned index = 28; index <= 31; ++index)
        WriteU64(memory, Address(R(state, 1) - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), Address(R(state, 12)));
}

void Restore28(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 28; index <= 31; ++index)
        R(state, index) = ReadU64(memory,
            Address(R(state, 1) - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

raw_allocation_context::Registers ToHeap(const Registers& state)
{
    raw_allocation_context::Registers lower{};
    lower.r = state.r;
    lower.lr = state.lr;
    lower.ctr = state.ctr;
    lower.f0_bits = state.fpr_bits[0];
    lower.f1_bits = state.fpr_bits[1];
    lower.f13_bits = state.fpr_bits[13];
    lower.f30_bits = state.fpr_bits[30];
    lower.f31_bits = state.fpr_bits[31];
    lower.cached_fp_control = state.cached_fp_control;
    lower.xer_so = state.xer_so;
    lower.xer_ca = state.xer_ca;
    lower.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq,
        state.cr0.so};
    lower.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq,
        state.cr6.so};
    return lower;
}

void FromHeap(Registers& state, const raw_allocation_context::Registers& lower)
{
    state.r = lower.r;
    state.lr = lower.lr;
    state.ctr = lower.ctr;
    state.fpr_bits[0] = lower.f0_bits;
    state.fpr_bits[1] = lower.f1_bits;
    state.fpr_bits[13] = lower.f13_bits;
    state.fpr_bits[30] = lower.f30_bits;
    state.fpr_bits[31] = lower.f31_bits;
    state.cached_fp_control = lower.cached_fp_control;
    state.xer_so = lower.xer_so;
    state.xer_ca = lower.xer_ca;
    state.cr0 = {lower.cr0.lt, lower.cr0.gt, lower.cr0.eq,
        lower.cr0.un};
    state.cr6 = {lower.cr6.lt, lower.cr6.gt, lower.cr6.eq,
        lower.cr6.un};
}

class RawBoundary final : public raw_allocation_context::PpcBoundaryServices
{
public:
    void CallDirect(GuestAddress, GuestMemory&,
        raw_allocation_context::Registers&) override
    { throw std::logic_error("unexpected raw getter direct call"); }
};

void Accepted(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (!ApplyAcceptedLower(entry, memory, dependencies, state))
        throw std::logic_error("missing accepted allocation lower");
}

void InvokeHandler(GuestMemory& memory, HandlerServices& services,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Address(R(state, 12)));
    Push(memory, state, 96u);
    R(state, 11) = 0xffffffff832d0000ull;
    R(state, 11) = memory.ReadU32(Address(R(state, 11) + 15080u));
    Compare(state.cr0, R(state, 11), 0u, state.xer_so, false);
    if (!state.cr0.eq)
    {
        state.ctr = R(state, 11);
        state.lr = 0x82b7fe8cu;
        services.CallNewHandler(Address(state.ctr) & ~3u,
            memory, state);
        Compare(state.cr0, R(state, 3), 0u, state.xer_so, true);
        R(state, 3) = state.cr0.eq ? 0u : 1u;
    }
    else R(state, 3) = 0u;
    R(state, 1) += 96u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

void AllocateArray(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    Save28(memory, state);
    Push(memory, state, 128u);
    R(state, 28) = R(state, 5);
    R(state, 30) = UINT64_MAX - 4095u;
    Compare(state.cr6, R(state, 3), 0u, state.xer_so, false);
    if (!state.cr6.eq)
    {
        R(state, 11) = (R(state, 11) & 0xffffffff00000000ull) |
            (Word(R(state, 30)) / Word(R(state, 3)));
        Compare(state.cr6, R(state, 11), R(state, 4),
            state.xer_so, false);
        if (state.cr6.lt)
        {
            state.lr = 0x82b816d0u;
            Accepted(0x82b7fd78u, memory, dependencies, state);
            R(state, 11) = R(state, 3);
            R(state, 10) = 12u;
            R(state, 7) = R(state, 6) = R(state, 5) =
                R(state, 4) = R(state, 3) = 0u;
            memory.WriteU32(Address(R(state, 11)), 12u);
            state.lr = 0x82b816f4u;
            Accepted(0x82b7fec0u, memory, dependencies, state);
            R(state, 3) = 0u;
            goto done;
        }
    }
    R(state, 31) = static_cast<std::uint64_t>(
        std::int64_t{Signed(R(state, 3))} * Signed(R(state, 4)));
    Compare(state.cr0, R(state, 31), 0u, state.xer_so, true);
    if (state.cr0.eq) R(state, 31) = 1u;
    R(state, 29) = 0xffffffff832d0000ull;
    for (;;)
    {
        R(state, 3) = 0u;
        Compare(state.cr6, R(state, 31), R(state, 30),
            state.xer_so, false);
        if (!state.cr6.gt)
        {
            state.lr = 0x82b8171cu;
            Accepted(0x823acc98u, memory, dependencies, state);
            R(state, 4) = 8u;
            R(state, 5) = R(state, 31);
            state.lr = 0x82b81728u;
            Accepted(0x823accb0u, memory, dependencies, state);
            Compare(state.cr0, R(state, 3), 0u, state.xer_so, false);
            if (!state.cr0.eq) break;
        }
        R(state, 11) = memory.ReadU32(Address(R(state, 29) + 15084u));
        Compare(state.cr6, R(state, 11), 0u, state.xer_so, true);
        if (!state.cr6.eq)
        {
            R(state, 3) = R(state, 31);
            state.lr = 0x82b81744u;
            InvokeHandler(memory, dependencies.handler, state);
            Compare(state.cr0, R(state, 3), 0u, state.xer_so, true);
            if (!state.cr0.eq) continue;
            Compare(state.cr6, R(state, 28), 0u,
                state.xer_so, false);
            if (state.cr6.eq) {R(state, 3) = 0u;break;}
            R(state, 11) = 12u;
            memory.WriteU32(Address(R(state, 28)), 12u);
            R(state, 3) = 0u;
            break;
        }
        Compare(state.cr6, R(state, 28), 0u, state.xer_so, false);
        if (!state.cr6.eq)
        {
            R(state, 11) = 12u;
            memory.WriteU32(Address(R(state, 28)), 12u);
        }
        break;
    }
done:
    R(state, 1) += 128u;
    Restore28(memory, state);
}

void AllocateRecord(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 24u), R(state, 30));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    Push(memory, state, 112u);
    R(state, 11) = 0u;
    R(state, 5) = R(state, 1) + 80u;
    memory.WriteU32(Address(R(state, 1) + 80u), 0u);
    state.lr = 0x82b8179cu;
    AllocateArray(memory, dependencies, state);
    R(state, 31) = R(state, 3);
    Compare(state.cr0, R(state, 31), 0u, state.xer_so, true);
    if (state.cr0.eq)
    {
        R(state, 30) = memory.ReadU32(Address(R(state, 1) + 80u));
        Compare(state.cr6, R(state, 30), 0u, state.xer_so, true);
        if (!state.cr6.eq)
        {
            state.lr = 0x82b817b4u;
            Accepted(0x82b7fd78u, memory, dependencies, state);
            Compare(state.cr0, R(state, 3), 0u, state.xer_so, false);
            if (!state.cr0.eq)
            {
                state.lr = 0x82b817c0u;
                Accepted(0x82b7fd78u, memory, dependencies, state);
                memory.WriteU32(Address(R(state, 3)), Address(R(state, 30)));
            }
        }
    }
    R(state, 3) = R(state, 31);
    R(state, 1) += 112u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
    R(state, 30) = ReadU64(memory, Address(R(state, 1) - 24u));
    R(state, 31) = ReadU64(memory, Address(R(state, 1) - 16u));
}
} // namespace

bool ApplyAcceptedLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82b7fd78u:
    case 0x82b7fec0u:
    {
        auto lower = crt_context_adapter::ToStream(state);
        if (!crt_stream_operations::ApplyAcceptedCallee(entry, memory,
            dependencies.stream, lower))
            return false;
        crt_context_adapter::FromStream(state, lower);
        return true;
    }
    case 0x823acc98u:
    {
        auto lower = ToHeap(state);
        RawBoundary boundary;
        if (!raw_allocation_context::Apply(entry, memory,
            boundary, lower)) return false;
        FromHeap(state, lower);
        return true;
    }
    case 0x823accb0u:
    case 0x823ad544u:
    {
        auto lower = ToHeap(state);
        if (!heap_allocation_context::Apply(entry, memory,
            dependencies.heap, lower)) return false;
        FromHeap(state, lower);
        return true;
    }
    default: return false;
    }
}

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82b81778u: AllocateRecord(memory, dependencies, state); return true;
    case 0x82b816a0u: AllocateArray(memory, dependencies, state); return true;
    case 0x82b7fe68u: InvokeHandler(memory, dependencies.handler, state);
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_record_allocation_context
