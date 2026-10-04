#include "lo_semantics/crt_reallocation_context.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_reallocation_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }
std::uint32_t W(std::uint64_t value) { return Address(value); }
std::int32_t S(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(W(value)); }
template<class T>
void Compare(crt_async_status_transfer::Condition& cr, T a, T b,
    std::uint8_t so)
{ cr = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b), so}; }
void Save27(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 27u; index <= 31u; ++index)
        WriteU64(memory, Address(R(state, 1) - 8u * (33u - index)), R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), W(R(state, 12)));
}
void Restore27(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 27u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory, Address(R(state, 1) - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}
void TranslateError(GuestMemory& memory, Registers& state)
{
    R(state, 11) = std::uint64_t(std::int64_t(-2094989312));
    R(state, 9) = R(state, 11) + 20648u;
    R(state, 11) = 0;
    R(state, 10) = R(state, 9);
    for (;;)
    {
        R(state, 8) = memory.ReadU32(W(R(state, 10)));
        Compare<std::uint32_t>(state.cr6, W(R(state, 3)), W(R(state, 8)), state.xer_so);
        if (state.cr6.eq)
        {
            R(state, 11) = std::rotl(W(R(state, 11)) | (R(state, 11) << 32u), 3) & 0xfffffff8u;
            R(state, 10) = R(state, 9) + 4u;
            R(state, 3) = memory.ReadU32(W(R(state, 11)) + W(R(state, 10)));
            return;
        }
        R(state, 11) += 1u;
        R(state, 10) += 8u;
        Compare<std::uint32_t>(state.cr6, W(R(state, 11)), 45u, state.xer_so);
        if (!state.cr6.lt) break;
    }
    R(state, 11) = R(state, 3) - 19u;
    Compare<std::uint32_t>(state.cr6, W(R(state, 11)), 17u, state.xer_so);
    if (!state.cr6.gt) { R(state, 3) = 13u; return; }
    R(state, 11) = R(state, 3) - 188u;
    state.xer_ca = W(R(state, 11)) <= 14u;
    R(state, 11) = 14u - R(state, 11);
    const auto word = W(R(state, 11));
    const auto carry = state.xer_ca;
    const auto sum = std::uint32_t(~word + word);
    const auto next_carry = std::uint8_t((sum < std::uint32_t(~word)) ||
        (std::uint32_t(sum + carry) < carry));
    R(state, 11) = ~R(state, 11) + R(state, 11) + carry;
    state.xer_ca = next_carry;
    R(state, 11) = std::rotl(W(R(state, 11)) | (R(state, 11) << 32u), 0) & 0xeu;
    R(state, 3) = R(state, 11) + 8u;
}
void Direct(GuestAddress entry, GuestMemory& memory, Dependencies deps,
    Registers& state)
{
    if (!ApplyLower(entry, memory, deps, state))
        throw std::logic_error("unselected CRT reallocation callee");
}
void Reallocate(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    R(state, 12) = state.lr;
    state.lr = 0x823acae0u;
    Save27(memory, state);
    const auto old_sp = R(state, 1);
    R(state, 1) -= 128u;
    memory.WriteU32(W(R(state, 1)), W(old_sp));
    R(state, 28) = R(state, 3);
    R(state, 31) = R(state, 4);
    Compare<std::uint32_t>(state.cr6, W(R(state, 28)), 0u, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 3) = R(state, 31);
        state.lr = 0x823acafcu;
        Direct(0x823acbd0u, memory, dependencies, state);
        goto finish;
    }
    Compare<std::uint32_t>(state.cr6, W(R(state, 31)), 0u, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 3) = R(state, 28);
        state.lr = 0x823acb10u;
        Direct(0x823addc0u, memory, dependencies, state);
        goto zero;
    }
    R(state, 29) = std::uint64_t(std::int64_t(-4096));
    Compare<std::uint32_t>(state.cr6, W(R(state, 31)), W(R(state, 29)), state.xer_so);
    if (state.cr6.gt) goto oversized;
    R(state, 27) = std::uint64_t(std::int64_t(-2094202880));
retry:
    Compare<std::uint32_t>(state.cr6, W(R(state, 31)), 0u, state.xer_so);
    if (state.cr6.eq) R(state, 31) = 1u;
    state.lr = 0x823acb34u;
    Direct(0x823acc98u, memory, dependencies, state);
    R(state, 4) = 0;
    R(state, 5) = R(state, 28);
    R(state, 6) = R(state, 31);
    state.lr = 0x823acb44u;
    Direct(0x827ccf80u, memory, dependencies, state);
    R(state, 30) = R(state, 3);
    Compare<std::int32_t>(state.cr0, S(R(state, 30)), 0, state.xer_so);
    if (!state.cr0.eq) goto success;
    R(state, 11) = memory.ReadU32(W(R(state, 27)) + 15084u);
    Compare<std::int32_t>(state.cr6, S(R(state, 11)), 0, state.xer_so);
    if (state.cr6.eq) goto translate;
    R(state, 3) = R(state, 31);
    state.lr = 0x823acb60u;
    Direct(0x82b7fe68u, memory, dependencies, state);
    Compare<std::int32_t>(state.cr0, S(R(state, 3)), 0, state.xer_so);
    if (state.cr0.eq) goto declined;
    Compare<std::uint32_t>(state.cr6, W(R(state, 31)), W(R(state, 29)), state.xer_so);
    if (!state.cr6.gt) goto retry;
oversized:
    R(state, 3) = R(state, 31);
    state.lr = 0x823acb78u;
    Direct(0x82b7fe68u, memory, dependencies, state);
    state.lr = 0x823acb7cu;
    Direct(0x82b7fd78u, memory, dependencies, state);
    R(state, 11) = R(state, 3);
    R(state, 10) = 12u;
    memory.WriteU32(W(R(state, 11)), W(R(state, 10)));
zero:
    R(state, 3) = 0;
finish:
    R(state, 1) += 128u;
    Restore27(memory, state);
    return;
declined:
    state.lr = 0x823acb98u;
    Direct(0x82b7fd78u, memory, dependencies, state);
    R(state, 31) = R(state, 3);
    state.lr = 0x823acba0u;
    Direct(0x822ca100u, memory, dependencies, state);
    state.lr = 0x823acba4u;
    Direct(0x82b7fd10u, memory, dependencies, state);
    R(state, 11) = R(state, 3);
    memory.WriteU32(W(R(state, 31)), W(R(state, 11)));
    goto zero;
translate:
    state.lr = 0x823acbb4u;
    Direct(0x82b7fd78u, memory, dependencies, state);
    R(state, 31) = R(state, 3);
    state.lr = 0x823acbbcu;
    Direct(0x822ca100u, memory, dependencies, state);
    state.lr = 0x823acbc0u;
    Direct(0x82b7fd10u, memory, dependencies, state);
    memory.WriteU32(W(R(state, 31)), W(R(state, 3)));
success:
    R(state, 3) = R(state, 30);
    goto finish;
}
} // namespace

bool ApplyLower(GuestAddress entry, GuestMemory& memory, Dependencies deps,
    Registers& state)
{
    switch (entry)
    {
    case 0x823acbd0u: case 0x823addc0u: case 0x827ccf80u:
        deps.guest.CallLower(entry, memory, state); return true;
    case 0x82b7fe68u:
        return crt_record_allocation_context::Apply(entry, memory, deps.allocation, state);
    case 0x823acc98u: case 0x82b7fd78u:
        return crt_record_allocation_context::ApplyAcceptedLower(entry, memory, deps.allocation, state);
    case 0x82b7fd10u:
        TranslateError(memory, state); return true;
    default: break;
    }
    auto lower = crt_context_adapter::ToStream(state);
    if (!crt_stream_operations::ApplyAcceptedCallee(entry, memory,
            deps.allocation.stream, lower)) return false;
    crt_context_adapter::FromStream(state, lower);
    return true;
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps,
    Registers& state)
{
    if (entry == 0x823acad8u) { Reallocate(memory, deps, state); return true; }
    if (entry == 0x82b7fd10u) { TranslateError(memory, state); return true; }
    return false;
}
} // namespace lo::semantic::gpu::crt_reallocation_context
