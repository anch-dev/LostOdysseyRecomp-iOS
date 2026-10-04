#include "lo_semantics/crt_close_recursive_buffer_context.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::crt_close_recursive_buffer_context
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

void Compare(Condition& cr, std::uint64_t lhs, std::uint64_t rhs,
    std::uint8_t so)
{
    const auto a = Word(lhs), b = Word(rhs);
    cr = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), so};
}

void Save(GuestMemory& memory, Registers& state, unsigned first,
    GuestAddress return_address)
{
    R(state, 12) = state.lr;
    for (unsigned index = first; index <= 31u; ++index)
        WriteU64(memory, Address(R(state, 1) - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), Word(R(state, 12)));
    state.lr = return_address;
}

void Restore(GuestMemory& memory, Registers& state, unsigned first)
{
    for (unsigned index = first; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(R(state, 1) - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

void Push(GuestMemory& memory, Registers& state, unsigned bytes)
{
    const auto old_sp = R(state, 1);
    R(state, 1) -= bytes;
    memory.WriteU32(Address(R(state, 1)), Word(old_sp));
}

void Table(GuestMemory& memory, Registers& state)
{
    R(state, 11) = std::uint64_t(std::int64_t(-2094137344));
    R(state, 3) = memory.ReadU32(Address(R(state, 11) - 2732u));
    Compare(state.cr6, R(state, 3), 0u, state.xer_so);
    if (!state.cr6.eq) return;
    R(state, 10) = std::uint64_t(std::int64_t(-2094989312));
    R(state, 3) = R(state, 10) + 26148u;
    memory.WriteU32(Address(R(state, 11) - 2732u), Word(R(state, 3)));
}

void AllocateNode(GuestMemory& memory, GuestServices& guest,
    Registers& state)
{
    Save(memory, state, 27u, 0x82bd07e0u);
    Push(memory, state, 128u);
    R(state, 27) = R(state, 3);
    R(state, 30) = R(state, 4);
    R(state, 29) = R(state, 5);
    state.lr = 0x82bd07f4u;
    Table(memory, state);
    R(state, 11) = memory.ReadU32(Address(R(state, 3)));
    R(state, 5) = 36u;
    R(state, 4) = 16u;
    R(state, 11) = memory.ReadU32(Address(R(state, 11)));
    state.ctr = R(state, 11);
    state.lr = 0x82bd080cu;
    guest.CallIndirect(Word(state.ctr) & ~3u, memory, state);
    R(state, 31) = R(state, 3);
    Compare(state.cr6, R(state, 31), 0u, state.xer_so);
    if (state.cr6.eq) goto failed;
    R(state, 28) = 0u;
    Compare(state.cr6, R(state, 30), 0u, state.xer_so);
    memory.WriteU32(Address(R(state, 31)), Word(R(state, 28)));
    memory.WriteU32(Address(R(state, 31) + 12u), Word(R(state, 28)));
    if (!state.cr6.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 30) + 8u));
        R(state, 11) = recovery_abi::WordRotateMask(R(state, 11), 1,
            0xfffffffeull);
    }
    else R(state, 11) = R(state, 29);
    memory.WriteU32(Address(R(state, 31) + 8u), Word(R(state, 11)));
    state.lr = 0x82bd0850u;
    Table(memory, state);
    R(state, 11) = memory.ReadU32(Address(R(state, 3)));
    R(state, 5) = 65u;
    R(state, 4) = memory.ReadU32(Address(R(state, 31) + 8u));
    R(state, 11) = memory.ReadU32(Address(R(state, 11)));
    state.ctr = R(state, 11);
    state.lr = 0x82bd0868u;
    guest.CallIndirect(Word(state.ctr) & ~3u, memory, state);
    Compare(state.cr6, R(state, 3), 0u, state.xer_so);
    memory.WriteU32(Address(R(state, 31)), Word(R(state, 3)));
    if (state.cr6.eq) goto failed;
    memory.WriteU32(Address(R(state, 31) + 4u), Word(R(state, 28)));
    Compare(state.cr6, R(state, 30), 0u, state.xer_so);
    memory.WriteU32(Address(R(state, 27)), Word(R(state, 31)));
    if (!state.cr6.eq)
        memory.WriteU32(Address(R(state, 30) + 12u), Word(R(state, 31)));
    R(state, 3) = 1u;
    goto done;
failed:
    R(state, 3) = 0u;
done:
    R(state, 1) += 128u;
    Restore(memory, state, 27u);
}

void Process(GuestMemory& memory, GuestServices& guest, Registers& state)
{
    Save(memory, state, 29u, 0x82bd0c20u);
    Push(memory, state, 112u);
    R(state, 31) = R(state, 3);
    R(state, 11) = memory.ReadU8(Address(R(state, 31) + 24u));
    Compare(state.cr6, R(state, 11), 0u, state.xer_so);
    if (state.cr6.eq) goto done;
    R(state, 29) = 0u;
    do
    {
        R(state, 10) = memory.ReadU8(Address(R(state, 31) + 25u));
        R(state, 11) = memory.ReadU8(Address(R(state, 31) + 24u));
        R(state, 10) = std::rotl(Word(R(state, 10)), 1);
        R(state, 11) += 1u;
        R(state, 30) = Word(R(state, 10)) & 0xffu;
        R(state, 11) = Word(R(state, 11)) & 0xffu;
        Compare(state.cr6, R(state, 11), 8u, state.xer_so);
        memory.WriteU8(Address(R(state, 31) + 25u),
            static_cast<std::uint8_t>(R(state, 30)));
        memory.WriteU8(Address(R(state, 31) + 24u),
            static_cast<std::uint8_t>(R(state, 11)));
        if (state.cr6.eq)
        {
            R(state, 3) = R(state, 31);
            memory.WriteU8(Address(R(state, 31) + 24u),
                static_cast<std::uint8_t>(R(state, 29)));
            state.lr = 0x82bd0c6cu;
            Process(memory, guest, state);
            R(state, 4) = memory.ReadU32(Address(R(state, 3)));
            R(state, 11) = memory.ReadU32(Address(R(state, 4) + 4u));
            R(state, 10) = memory.ReadU32(Address(R(state, 4) + 8u));
            R(state, 11) += 1u;
            Compare(state.cr6, R(state, 11), R(state, 10), state.xer_so);
            if (state.cr6.gt)
            {
                R(state, 5) = 0u;
                state.lr = 0x82bd0c8cu;
                AllocateNode(memory, guest, state);
            }
            R(state, 11) = memory.ReadU32(Address(R(state, 31)));
            R(state, 9) = memory.ReadU32(Address(R(state, 11)));
            R(state, 10) = memory.ReadU32(Address(R(state, 11) + 4u));
            R(state, 10) += R(state, 9);
            memory.WriteU32(Address(R(state, 31) + 16u),
                Word(R(state, 10)));
            R(state, 10) = memory.ReadU32(Address(R(state, 11) + 4u));
            R(state, 10) += 1u;
            memory.WriteU32(Address(R(state, 11) + 4u),
                Word(R(state, 10)));
            R(state, 11) = memory.ReadU32(Address(R(state, 31) + 16u));
            memory.WriteU8(Address(R(state, 11)),
                static_cast<std::uint8_t>(R(state, 30)));
        }
        R(state, 11) = memory.ReadU8(Address(R(state, 31) + 24u));
        Compare(state.cr6, R(state, 11), 0u, state.xer_so);
    } while (!state.cr6.eq);
done:
    R(state, 3) = R(state, 31);
    R(state, 1) += 112u;
    Restore(memory, state, 29u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    GuestServices& guest, Registers& state)
{
    switch (entry)
    {
    case 0x82bd0798u: Table(memory, state); return true;
    case 0x82bd07d8u: AllocateNode(memory, guest, state); return true;
    case 0x82bd0c18u: Process(memory, guest, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_close_recursive_buffer_context
