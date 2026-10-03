#include "lo_semantics/crt_stream_error.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/crt_allocation.h"

#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_error
{
namespace
{
constexpr GuestAddress kGetStreamError = 0x82B7FDB0u;
constexpr GuestAddress kGetStream = 0x82B86228u;
constexpr GuestAddress kSetStreamError = 0x82B7FDE8u;
constexpr GuestAddress kStreamCount = 0x83378D68u;

GuestAddress Address(std::uint64_t value)
{ return static_cast<GuestAddress>(value); }

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, Address(value >> 32));
    memory.WriteU32(address + 4u, Address(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}

void Compare(Condition& condition, std::uint32_t left, std::uint32_t right,
    std::uint8_t so, bool is_signed)
{
    if (is_signed)
    {
        const auto a = static_cast<std::int32_t>(left);
        const auto b = static_cast<std::int32_t>(right);
        condition = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), so};
    }
    else
        condition = {std::uint8_t(left < right), std::uint8_t(left > right),
            std::uint8_t(left == right), so};
}

void Enter(GuestMemory& memory, Registers& state, unsigned size,
    bool save_nonvolatile)
{
    state.r12 = state.lr;
    const GuestAddress caller = Address(state.sp);
    memory.WriteU32(caller - 8u, Address(state.r12));
    if (save_nonvolatile)
    {
        WriteU64(memory, caller - 24u, state.r30);
        WriteU64(memory, caller - 16u, state.r31);
    }
    memory.WriteU32(caller - size, caller);
    state.sp -= size;
}

void Leave(GuestMemory& memory, Registers& state, unsigned size,
    bool restore_nonvolatile)
{
    state.sp += size;
    const GuestAddress caller = Address(state.sp);
    state.r12 = memory.ReadU32(caller - 8u);
    state.lr = state.r12;
    if (restore_nonvolatile)
    {
        state.r30 = ReadU64(memory, caller - 24u);
        state.r31 = ReadU64(memory, caller - 16u);
    }
}

std::uint64_t ThreadData(GuestMemory& memory,
    CrtThreadDataServices& services, Registers& state)
{
    CrtThreadDataCall call{state.r13};
    const auto data = GetCrtThreadData(memory, services, call);
    state.r13 = call.thread_environment;
    state.r3 = data;
    return data;
}

class ErrorAddressAdapter final : public AllocationFailureServices
{
public:
    ErrorAddressAdapter(GuestMemory& memory, CrtThreadDataServices& services,
        Registers& state) : memory_(memory), services_(services), state_(state) {}
    std::uint64_t GetThreadData() override
    { return ThreadData(memory_, services_, state_); }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::logic_error("unexpected CRT error output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::logic_error("unexpected CRT bug-check"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::logic_error("unexpected CRT new handler"); }
private:
    GuestMemory& memory_;
    CrtThreadDataServices& services_;
    Registers& state_;
};

void ErrorAddress(GuestMemory& memory, CrtThreadDataServices& services,
    Registers& state)
{
    // 82B7FD78 owns a 96-byte frame around its recovered thread-data call.
    Enter(memory, state, 96, false);
    ErrorAddressAdapter adapter(memory, services, state);
    state.r3 = GetAllocationErrorAddress(adapter);
    Leave(memory, state, 96, false);
}

void InvalidParameter(GuestMemory& memory,
    InvalidParameterServices& services, Registers& state)
{
    InvalidParameterCall call{{{state.r3, state.r4, state.r5, state.r6,
        state.r7, state.r8, state.r9, state.r10}}, state.r13};
    state.r3 = ReportInvalidParameter(memory, services, call);
    state.r4 = call.arguments[1]; state.r5 = call.arguments[2];
    state.r6 = call.arguments[3]; state.r7 = call.arguments[4];
    state.r8 = call.arguments[5]; state.r9 = call.arguments[6];
    state.r10 = call.arguments[7]; state.r13 = call.thread_environment;
}

void GetErrorSlot(GuestMemory& memory, CrtThreadDataServices& services,
    Registers& state)
{
    Enter(memory, state, 96, false);
    state.lr = 0x82B7FDC0u;
    const auto data = ThreadData(memory, services, state);
    Compare(state.cr0, Address(data), 0, state.xer_so, false);
    state.r3 = Address(data) == 0 ?
        0xFFFFFFFF83215214ull : data + 12u;
    if (Address(data) == 0) state.r11 = 0xFFFFFFFF83210000ull;
    Leave(memory, state, 96, false);
}

void InvalidHandle(GuestMemory& memory, CrtThreadDataServices& thread,
    InvalidParameterServices& invalid, Registers& state,
    bool report_invalid)
{
    state.lr = report_invalid ? 0x82B8627Cu : 0x82B86240u;
    GetErrorSlot(memory, thread, state);
    state.r11 = 0;
    memory.WriteU32(Address(state.r3), 0);
    state.lr = report_invalid ? 0x82B86288u : 0x82B8624Cu;
    ErrorAddress(memory, thread, state);
    state.r11 = state.r3;
    state.r10 = 9;
    if (report_invalid)
    {
        state.r7 = state.r6 = state.r5 = state.r4 = state.r3 = 0;
    }
    else
        state.r3 = UINT64_MAX;
    memory.WriteU32(Address(state.r11), 9);
    if (report_invalid)
    {
        state.lr = 0x82B862ACu;
        InvalidParameter(memory, invalid, state);
        state.r3 = UINT64_MAX;
    }
}

void GetStream(GuestMemory& memory, CrtThreadDataServices& thread,
    InvalidParameterServices& invalid, Registers& state)
{
    Enter(memory, state, 96, false);
    Compare(state.cr6, Address(state.r3), 0xFFFFFFFEu, state.xer_so, true);
    if (state.cr6.eq)
    {
        InvalidHandle(memory, thread, invalid, state, false);
    }
    else
    {
        Compare(state.cr6, Address(state.r3), 0, state.xer_so, true);
        bool bad = state.cr6.lt;
        if (!bad)
        {
            state.r11 = 0xFFFFFFFF83380000ull;
            state.r11 = memory.ReadU32(kStreamCount);
            Compare(state.cr6, Address(state.r3), Address(state.r11),
                state.xer_so, false);
            bad = !state.cr6.lt;
        }
        if (bad)
            InvalidHandle(memory, thread, invalid, state, true);
        else
        {
            const auto index = Address(state.r3);
            state.xer_ca = 0;
            state.r10 = index >> 5;
            state.r9 = (state.r10 << 2) & 0xFFFFFFFCu;
            state.r11 = 0xFFFFFFFF83380000ull;
            state.r11 -= 29312u;
            state.r10 = (index << 6) & 0x7C0u;
            state.r11 = memory.ReadU32(Address(state.r11 + state.r9));
            state.r11 += state.r10;
            state.r10 = memory.ReadU8(Address(state.r11 + 4u)) & 1u;
            Compare(state.cr0, Address(state.r10), 0, state.xer_so, true);
            if (state.cr0.eq)
                InvalidHandle(memory, thread, invalid, state, true);
            else
                state.r3 = memory.ReadU32(Address(state.r11));
        }
    }
    Leave(memory, state, 96, false);
}

void SetError(GuestMemory& memory, CrtThreadDataServices& services,
    Registers& state)
{
    Enter(memory, state, 112, true);
    state.r30 = state.r3;
    state.lr = 0x82B7FE04u;
    const auto first = ThreadData(memory, services, state);
    state.r11 = 0xFFFFFFFF83210000ull;
    Compare(state.cr0, Address(first), 0, state.xer_so, false);
    state.r31 = 0xFFFFFFFF83215210ull;
    state.r11 = state.r31 + 4u;
    if (Address(first) != 0) state.r11 = first + 12u;
    memory.WriteU32(Address(state.r11), Address(state.r30));
    state.lr = 0x82B7FE24u;
    const auto second = ThreadData(memory, services, state);
    Compare(state.cr0, Address(second), 0, state.xer_so, false);
    state.r7 = state.r31;
    if (Address(second) != 0) state.r7 = second + 8u;
    state.r3 = state.r30;
    state.lr = 0x82B7FE3Cu;
    state.r3 = TranslateCrtError(memory, Address(state.r3));
    memory.WriteU32(Address(state.r7), Address(state.r3));
    Leave(memory, state, 112, true);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& state)
{
    switch (address)
    {
    case kGetStreamError:
        GetErrorSlot(memory, thread_services, state); return true;
    case kGetStream:
        GetStream(memory, thread_services, invalid_services, state); return true;
    case kSetStreamError:
        SetError(memory, thread_services, state); return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_error
