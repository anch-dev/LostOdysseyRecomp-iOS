#include "lo_semantics/crt_reallocate.h"

#include "lo_semantics/crt_last_error.h"
#include "lo_semantics/memory_services.h"

#include <array>
#include <stdexcept>

namespace lo::semantic::gpu::crt_reallocate
{
namespace
{
GuestAddress Address(std::uint64_t value)
{
    return static_cast<GuestAddress>(value);
}

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

std::array<std::uint64_t*, 5> SavedRegisters(Registers& state)
{
    return {&state.r27, &state.r28, &state.r29, &state.r30, &state.r31};
}

void Enter(GuestMemory& memory, Registers& state, unsigned bytes,
    unsigned first_saved)
{
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    const auto saved = SavedRegisters(state);
    for (unsigned reg = first_saved; reg != 32; ++reg)
        WriteU64(memory, Address(state.sp - (33u - reg) * 8u), *saved[reg - 27u]);
    memory.WriteU32(Address(state.sp - bytes), Address(state.sp));
    state.sp -= bytes;
}

void Leave(GuestMemory& memory, Registers& state, unsigned bytes,
    unsigned first_saved)
{
    state.sp += bytes;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    const auto saved = SavedRegisters(state);
    for (unsigned reg = first_saved; reg != 32; ++reg)
        *saved[reg - 27u] = ReadU64(memory, Address(state.sp - (33u - reg) * 8u));
}

template<class Function>
void CallLower(GuestMemory& memory, Registers& state, unsigned bytes,
    unsigned first_saved, Function&& function)
{
    Enter(memory, state, bytes, first_saved);
    state.r3 = function();
    Leave(memory, state, bytes, first_saved);
}

// This adapter calls the accepted allocator implementation. Its kernel/heap
// growth and deeper ABI effects retain the existing allocator's boundaries.
class RawAdapter final : public RecoveredRawAllocationServices
{
public:
    RawAdapter(GuestMemory& memory, CrtAllocationServices& services,
        Registers& state)
        : RecoveredRawAllocationServices(memory, services), memory_(memory),
          services_(services), state_(state) {}

    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags,
        std::uint64_t bytes) override
    {
        return AllocateHeapBlock(memory_, services_, heap, flags,
            Address(bytes), Address(state_.sp - 320u));
    }
private:
    GuestMemory& memory_;
    CrtAllocationServices& services_;
    Registers& state_;
};

void CallNewHandler(GuestMemory& memory, CrtAllocationServices& crt,
    Registers& state, GuestAddress return_address)
{
    state.r3 = state.r31;
    state.lr = return_address;
    CallLower(memory, state, 96, 32, [&] {
        return InvokeNewHandler(memory, crt, state.r3);
    });
}

void CallErrorAddress(GuestMemory& memory, CrtAllocationServices& crt,
    Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    CallLower(memory, state, 96, 32, [&] {
        return GetAllocationErrorAddress(crt);
    });
}

void StoreTranslatedError(GuestMemory& memory, CrtAllocationServices& crt,
    Registers& state, bool handler_rejected)
{
    CallErrorAddress(memory, crt, state,
        handler_rejected ? 0x823ACB98u : 0x823ACBB4u);
    state.r31 = state.r3;
    state.lr = handler_rejected ? 0x823ACBA0u : 0x823ACBBCu;
    crt_last_error::Registers error{state.r3, state.r11, state.r13,
        state.xer_so, {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so}};
    (void)crt_last_error::Apply(0x822CA100u, memory, error);
    state.r3 = error.r3;
    state.r11 = error.r11;
    state.cr6 = {error.cr6.lt, error.cr6.gt, error.cr6.eq, error.cr6.so};
    state.lr = handler_rejected ? 0x823ACBA4u : 0x823ACBC0u;
    state.r3 = TranslateCrtError(memory, Address(state.r3));
    if (handler_rejected) state.r11 = state.r3;
    memory.WriteU32(Address(state.r31), Address(state.r3));
}

void Compare(heap_reallocate::Condition& condition, std::uint64_t a,
    std::uint64_t b, std::uint8_t so, bool signed_words = false)
{
    if (signed_words)
    {
        const auto left = static_cast<std::int32_t>(Address(a));
        const auto right = static_cast<std::int32_t>(Address(b));
        condition = {static_cast<std::uint8_t>(left < right),
            static_cast<std::uint8_t>(left > right),
            static_cast<std::uint8_t>(left == right), so};
    }
    else
    {
        condition = {static_cast<std::uint8_t>(Address(a) < Address(b)),
            static_cast<std::uint8_t>(Address(a) > Address(b)),
            static_cast<std::uint8_t>(Address(a) == Address(b)), so};
    }
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    heap_reallocate::Services& heap, CrtAllocationServices& crt,
    Registers& state)
{
    if (address != 0x823ACAD8u) return false;
    Enter(memory, state, 128, 27);
    state.r28 = state.r3;
    state.r31 = state.r4;
    Compare(state.cr6, state.r28, 0, state.xer_so);
    if (Address(state.r28) == 0)
    {
        state.r3 = state.r31;
        state.lr = 0x823ACAFCu;
        CallLower(memory, state, 128, 28, [&] {
            RawAdapter raw(memory, crt, state);
            return AllocateRawMemory(memory, raw, state.r3);
        });
    }
    else
    {
        Compare(state.cr6, state.r31, 0, state.xer_so);
        if (Address(state.r31) == 0)
        {
            state.r3 = state.r28;
            state.lr = 0x823ACB10u;
            CallLower(memory, state, 96, 31, [&] {
                // FreeCrtRecord takes its incoming stack, before the own
                // frame represented by CallLower has been subtracted.
                return FreeCrtRecord(memory, crt, state.r3,
                    Address(state.r13), Address(state.sp + 96u));
            });
            state.r3 = 0;
        }
        else
        {
            state.r29 = UINT64_MAX - 4095u;
            Compare(state.cr6, state.r31, state.r29, state.xer_so);
            if (Address(state.r31) <= Address(state.r29))
            {
                state.r27 = 0xFFFFFFFF832D0000ull;
                bool oversized_after_retry = false;
                for (;;)
                {
                    Compare(state.cr6, state.r31, 0, state.xer_so);
                    if (Address(state.r31) == 0) state.r31 = 1;
                    state.lr = 0x823ACB34u;
                    state.r3 = GetProcessHeap(memory);
                    state.r4 = 0;
                    state.r5 = state.r28;
                    state.r6 = state.r31;
                    state.lr = 0x823ACB44u;
                    if (!heap_reallocate::Apply(0x827CCF80u, memory, heap, state))
                        throw std::logic_error("heap realloc model missing");
                    state.r30 = state.r3;
                    Compare(state.cr0, state.r30, 0, state.xer_so, true);
                    if (Address(state.r30) != 0) break;
                    state.r11 = memory.ReadU32(Address(state.r27 + 15084u));
                    Compare(state.cr6, state.r11, 0, state.xer_so, true);
                    if (Address(state.r11) == 0)
                    {
                        StoreTranslatedError(memory, crt, state, false);
                        break;
                    }
                    CallNewHandler(memory, crt, state, 0x823ACB60u);
                    Compare(state.cr0, state.r3, 0, state.xer_so, true);
                    if (Address(state.r3) == 0)
                    {
                        StoreTranslatedError(memory, crt, state, true);
                        state.r30 = 0;
                        break;
                    }
                    Compare(state.cr6, state.r31, state.r29, state.xer_so);
                    if (Address(state.r31) <= Address(state.r29)) continue;
                    oversized_after_retry = true;
                    break;
                }
                // The original can reach the oversized path after a retry
                // if a live child restore changed the saved size register.
                if (!oversized_after_retry)
                    state.r3 = state.r30;
                else
                {
                    CallNewHandler(memory, crt, state, 0x823ACB78u);
                    CallErrorAddress(memory, crt, state, 0x823ACB7Cu);
                    state.r11 = state.r3;
                    state.r10 = 12;
                    memory.WriteU32(Address(state.r11), 12);
                    state.r3 = 0;
                }
            }
            else
            {
                CallNewHandler(memory, crt, state, 0x823ACB78u);
                CallErrorAddress(memory, crt, state, 0x823ACB7Cu);
                state.r11 = state.r3;
                state.r10 = 12;
                memory.WriteU32(Address(state.r11), 12);
                state.r3 = 0;
            }
        }
    }
    Leave(memory, state, 128, 27);
    return true;
}
} // namespace lo::semantic::gpu::crt_reallocate
