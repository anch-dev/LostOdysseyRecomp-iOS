#include "lo_semantics/object_ring_dispatch.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::object_ring_dispatch
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void CompareWord(Registers& state, std::uint64_t left, std::uint64_t right)
{
    const auto a = Address(left), b = Address(right);
    state.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}

void CompareSignedZero(Registers& state, std::uint64_t value)
{
    const auto word = std::bit_cast<std::int32_t>(Address(value));
    state.cr6 = {std::uint8_t(word < 0), std::uint8_t(word > 0),
        std::uint8_t(word == 0), state.xer_so};
}

void Dispatch(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    dependencies.dynamic.Call(Address(state.ctr) & ~GuestAddress{3}, memory, state);
}

void DisableFlushMode(Dependencies dependencies, Registers& state)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    if (state.cached_fp_control & FlushMask)
    {
        state.cached_fp_control &= ~FlushMask;
        dependencies.fp.SetHostFpControl(state.cached_fp_control);
    }
}

void UpdateObject(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    const auto caller_sp = state.sp;
    R(state, 12) = state.lr;
    memory.WriteU32(Address(caller_sp - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(caller_sp - 24u), R(state, 30));
    WriteU64(memory, Address(caller_sp - 16u), R(state, 31));
    memory.WriteU32(Address(caller_sp - 128u), Address(caller_sp));
    state.sp -= 128u;
    R(state, 31) = R(state, 3);
    R(state, 30) = R(state, 4);

    R(state, 11) = memory.ReadU32(Address(R(state, 31)));
    R(state, 11) = memory.ReadU32(Address(R(state, 11) + 84u));
    state.ctr = R(state, 11);
    state.lr = 0x823efa5cu;
    Dispatch(memory, dependencies, state);

    R(state, 3) = memory.ReadU32(Address(R(state, 31) + 504u));
    CompareWord(state, R(state, 3), R(state, 30));
    if (!state.cr6.eq)
    {
        CompareWord(state, R(state, 3), 0);
        if (!state.cr6.eq)
        {
            R(state, 11) = memory.ReadU32(Address(R(state, 3)));
            R(state, 4) = 1;
            R(state, 11) = memory.ReadU32(Address(R(state, 11)));
            state.ctr = R(state, 11);
            state.lr = 0x823efa84u;
            Dispatch(memory, dependencies, state);
        }
    }

    R(state, 11) = memory.ReadU32(Address(R(state, 31)));
    R(state, 3) = R(state, 31);
    memory.WriteU32(Address(R(state, 31) + 504u), Address(R(state, 30)));
    R(state, 11) = memory.ReadU32(Address(R(state, 11) + 56u));
    state.ctr = R(state, 11);
    state.lr = 0x823efa9cu;
    Dispatch(memory, dependencies, state);

    DisableFlushMode(dependencies, state);
    const auto first = LoadedSingle::FromWord(memory.ReadU32(Address(R(state, 31) + 336u)));
    state.f0_bits = first.FprBits();
    memory.WriteU32(Address(state.sp + 80u), first.StoreWord());
    R(state, 11) = 0xffffffff83370000ull;
    const auto second = LoadedSingle::FromWord(memory.ReadU32(Address(R(state, 31) + 340u)));
    state.f0_bits = second.FprBits();
    R(state, 10) = memory.ReadU32(Address(state.sp + 80u));
    memory.WriteU32(Address(state.sp + 84u), second.StoreWord());
    R(state, 11) -= 22092u;
    const auto third = LoadedSingle::FromWord(memory.ReadU32(Address(R(state, 31) + 344u)));
    state.f0_bits = third.FprBits();
    R(state, 9) = memory.ReadU32(Address(state.sp + 84u));
    memory.WriteU32(Address(state.sp + 88u), third.StoreWord());
    R(state, 8) = memory.ReadU32(Address(state.sp + 88u));
    memory.WriteU32(Address(R(state, 11)), Address(R(state, 10)));
    memory.WriteU32(Address(R(state, 11) + 4u), Address(R(state, 9)));
    memory.WriteU32(Address(R(state, 11) + 8u), Address(R(state, 8)));

    state.sp += 128u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    R(state, 30) = ReadU64(memory, Address(state.sp - 24u));
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

void ReserveRing(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    ring_reservation::Registers ring{R(state, 7), R(state, 8), R(state, 9),
        R(state, 10), R(state, 11)};
    std::uint64_t result = 0;
    (void)ring_reservation::Apply(0x82290ab8u, memory, dependencies.synchronization,
        R(state, 3), R(state, 4), R(state, 5), ring, result);
    R(state, 3) = result;
    R(state, 7) = ring.r7;
    R(state, 8) = ring.r8;
    R(state, 9) = ring.r9;
    R(state, 10) = ring.r10;
    R(state, 11) = ring.r11;
}

void CreateObject(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    const auto caller_sp = state.sp;
    R(state, 12) = state.lr;
    WriteU64(memory, Address(caller_sp - 40u), R(state, 28));
    WriteU64(memory, Address(caller_sp - 32u), R(state, 29));
    WriteU64(memory, Address(caller_sp - 24u), R(state, 30));
    WriteU64(memory, Address(caller_sp - 16u), R(state, 31));
    memory.WriteU32(Address(caller_sp - 8u), Address(R(state, 12)));
    state.lr = 0x82372fb8u;
    R(state, 31) = state.sp - 160u;
    memory.WriteU32(Address(R(state, 31)), Address(state.sp));
    state.sp = R(state, 31);
    R(state, 11) = 0xffffffff83320000ull;
    R(state, 30) = R(state, 3);
    R(state, 29) = R(state, 4);
    R(state, 11) = memory.ReadU32(Address(R(state, 11) - 32704u));
    CompareSignedZero(state, R(state, 11));

    if (!state.cr6.eq)
    {
        R(state, 11) = 0xffffffff83370000ull;
        R(state, 5) = 12;
        R(state, 4) = R(state, 11) - 22620u;
        R(state, 3) = R(state, 31) + 88u;
        state.lr = 0x82372fecu;
        ReserveRing(memory, dependencies, state);
        R(state, 11) = memory.ReadU32(Address(R(state, 3) + 4u));
        CompareWord(state, R(state, 11), 0);
        memory.WriteU32(Address(R(state, 31) + 80u), Address(R(state, 11)));
        if (!state.cr6.eq)
        {
            R(state, 10) = 0xffffffff82080000ull;
            R(state, 10) += 396u;
            memory.WriteU32(Address(R(state, 11)), Address(R(state, 10)));
            R(state, 10) = 0xffffffff82000000ull;
            memory.WriteU32(Address(R(state, 11) + 4u), Address(R(state, 30)));
            R(state, 10) += 13908u;
            memory.WriteU32(Address(R(state, 11) + 8u), Address(R(state, 29)));
            memory.WriteU32(Address(R(state, 11)), Address(R(state, 10)));
        }
        R(state, 11) = memory.ReadU32(Address(R(state, 31) + 92u));
        CompareWord(state, R(state, 11), 0);
        if (!state.cr6.eq)
        {
            dependencies.synchronization.LightweightSync();
            R(state, 11) = memory.ReadU32(Address(R(state, 31) + 88u));
            R(state, 9) = memory.ReadU32(Address(R(state, 31) + 96u));
            R(state, 8) = 0;
            R(state, 10) = memory.ReadU32(Address(R(state, 11) + 8u));
            R(state, 10) += R(state, 9);
            memory.WriteU32(Address(R(state, 11) + 8u), Address(R(state, 10)));
            R(state, 11) = memory.ReadU32(Address(R(state, 31) + 88u));
            memory.WriteU32(Address(R(state, 11) + 16u), Address(R(state, 8)));
            memory.WriteU32(Address(R(state, 31) + 92u), Address(R(state, 8)));
        }
    }
    else
    {
        R(state, 11) = 0xffffffff82080000ull;
        R(state, 28) = R(state, 11) + 396u;
        memory.WriteU32(Address(R(state, 31) + 104u), Address(R(state, 28)));
        R(state, 11) = 0xffffffff82000000ull;
        memory.WriteU32(Address(R(state, 31) + 108u), Address(R(state, 30)));
        R(state, 11) += 13908u;
        memory.WriteU32(Address(R(state, 31) + 112u), Address(R(state, 29)));
        memory.WriteU32(Address(R(state, 31) + 104u), Address(R(state, 11)));
        R(state, 4) = R(state, 29);
        R(state, 3) = R(state, 30);
        state.lr = 0x82373080u;
        UpdateObject(memory, dependencies, state);
        memory.WriteU32(Address(R(state, 31) + 104u), Address(R(state, 28)));
    }

    state.sp = R(state, 31) + 160u;
    R(state, 28) = ReadU64(memory, Address(state.sp - 40u));
    R(state, 29) = ReadU64(memory, Address(state.sp - 32u));
    R(state, 30) = ReadU64(memory, Address(state.sp - 24u));
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}
}

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x823efa30u: UpdateObject(memory, dependencies, state); return true;
    case 0x82372fb0u: CreateObject(memory, dependencies, state); return true;
    default: return false;
    }
}
}
