#include "lo_semantics/crt_utf8_conversion_routes.h"

#include "lo_semantics/crt_thread_error_routes.h"
#include "lo_semantics/recovery_abi.h"

#include <stdexcept>

namespace lo::semantic::gpu::crt_utf8_conversion_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

constexpr GuestAddress kDispatch = 0x8229c560u;
constexpr GuestAddress kUtf8 = 0x827ca660u;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.r[index]; }

void Compare(crt_stream_operations::Condition& condition,
    std::uint64_t left, std::uint64_t right, std::uint8_t so,
    bool signed_words)
{
    const auto a = Address(left);
    const auto b = Address(right);
    if (signed_words)
    {
        const auto x = static_cast<std::int32_t>(a);
        const auto y = static_cast<std::int32_t>(b);
        condition = {std::uint8_t(x < y), std::uint8_t(x > y),
            std::uint8_t(x == y), so};
    }
    else
        condition = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), so};
}

void Push(GuestMemory& memory, Registers& state, unsigned size)
{
    memory.WriteU32(Address(state.sp - size), Address(state.sp));
    state.sp -= size;
}

class UnusedTlsServices final : public crt_thread_error_routes::NativeServices
{
public:
    void KeTlsGetValue(GuestMemory&, Registers&) override
    { throw std::logic_error("unexpected TLS getter in error-store tail"); }
    void KeTlsSetValue(GuestMemory&, Registers&) override
    { throw std::logic_error("unexpected TLS setter in error-store tail"); }
};

void StoreError(GuestMemory& memory, Registers& state,
    GuestAddress return_address)
{
    state.lr = return_address;
    UnusedTlsServices native;
    if (!crt_thread_error_routes::Apply(0x822ca180u, memory, native, state))
        throw std::logic_error("missing accepted CRT thread error tail");
}

void EnterUtf8(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    for (unsigned index = 20; index <= 31; ++index)
        WriteU64(memory, Address(state.sp - 16u - 8u * (31u - index)),
            R(state, index));
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    state.lr = 0x827ca668u;
    Push(memory, state, 192u);
}

void LeaveUtf8(GuestMemory& memory, Registers& state)
{
    state.sp += 192u;
    for (unsigned index = 20; index <= 31; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 16u - 8u * (31u - index)));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void DecodeUtf8(GuestMemory& memory, Registers& state)
{
    EnterUtf8(memory, state);
    R(state, 22) = R(state, 3);
    R(state, 3) = 0;
    R(state, 10) = R(state, 22);
    Compare(state.cr6, R(state, 4), UINT32_MAX, state.xer_so, true);
    if (state.cr6.eq)
    {
        R(state, 11) = R(state, 22);
        R(state, 9) = R(state, 11);
        do
        {
            R(state, 8) = memory.ReadU8(Address(R(state, 11)));
            ++R(state, 11);
            Compare(state.cr6, R(state, 8), 0, state.xer_so, false);
        } while (!state.cr6.eq);
        R(state, 11) -= R(state, 9);
        R(state, 11) -= 1u;
        R(state, 11) = Address(R(state, 11));
        R(state, 4) = R(state, 11) + 1u;
    }
    Compare(state.cr6, R(state, 4), 0, state.xer_so, true);
    if (state.cr6.gt)
    {
        R(state, 11) = 0xffffffff831e0000ull;
        R(state, 31) = 0;
        R(state, 24) = R(state, 11) + 31920u;
        R(state, 11) = 0xffffffff831e0000ull;
        R(state, 27) = R(state, 11);
        R(state, 26) = R(state, 11);
        R(state, 28) = R(state, 11);
        R(state, 29) = R(state, 11);
        R(state, 25) = R(state, 11);
        R(state, 21) = R(state, 11);
        R(state, 20) = R(state, 11);
        R(state, 23) = R(state, 11);
        R(state, 30) = R(state, 11) + 31944u;
        do
        {
            R(state, 8) = memory.ReadU8(Address(R(state, 10)));
            R(state, 11) = 0;
            R(state, 9) = memory.ReadU8(Address(R(state, 8) + R(state, 30)));
            R(state, 9) = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(static_cast<std::int8_t>(R(state, 9))));
            R(state, 7) = Address(R(state, 9)) & 0xffffu;
            R(state, 9) = R(state, 7) - R(state, 22);
            R(state, 9) += R(state, 10);
            Compare(state.cr6, R(state, 9), R(state, 4), state.xer_so, true);
            if (state.cr6.gt) break;

            unsigned bytes = 0;
            Compare(state.cr6, R(state, 7), 1u, state.xer_so, false);
            if (state.cr6.lt) bytes = 1;
            else if (state.cr6.eq) bytes = 2;
            else
            {
                Compare(state.cr6, R(state, 7), 3u, state.xer_so, false);
                if (state.cr6.lt) bytes = 3;
                else if (state.cr6.eq) bytes = 4;
                else
                {
                    Compare(state.cr6, R(state, 7), 5u, state.xer_so, false);
                    if (state.cr6.lt) bytes = 5;
                    else if (state.cr6.eq)
                    {
                        ++R(state, 10);
                        ++R(state, 31);
                        R(state, 11) = WordRotateMask(R(state, 8), 6,
                            0xffffffc0u);
                        bytes = 5;
                    }
                }
            }
            for (unsigned remaining = bytes; remaining > 1; --remaining)
            {
                R(state, 9) = memory.ReadU8(Address(R(state, 10)));
                ++R(state, 31);
                ++R(state, 10);
                R(state, 11) += R(state, 9);
                R(state, 11) = WordRotateMask(R(state, 11), 6,
                    0xffffffc0u);
            }
            if (bytes)
            {
                R(state, 9) = memory.ReadU8(Address(R(state, 10)));
                ++R(state, 31);
                ++R(state, 10);
                R(state, 11) += R(state, 9);
            }
            R(state, 9) = WordRotateMask(R(state, 7), 2, 0xfffffffcu);
            R(state, 9) = memory.ReadU32(Address(R(state, 9) + R(state, 24)));
            R(state, 11) -= R(state, 9);
            R(state, 9) = memory.ReadU32(Address(R(state, 23) + 31892u));
            Compare(state.cr6, R(state, 11), R(state, 9), state.xer_so, false);
            if (!state.cr6.gt)
            {
                ++R(state, 3);
                Compare(state.cr6, R(state, 3), R(state, 6),
                    state.xer_so, true);
                if (!state.cr6.gt)
                {
                    memory.WriteU16(Address(R(state, 5)),
                        static_cast<std::uint16_t>(R(state, 11)));
                    R(state, 5) += 2u;
                }
            }
            else
            {
                R(state, 9) = memory.ReadU32(Address(R(state, 20) + 31896u));
                Compare(state.cr6, R(state, 11), R(state, 9),
                    state.xer_so, false);
                if (state.cr6.gt)
                {
                    ++R(state, 3);
                    Compare(state.cr6, R(state, 3), R(state, 6),
                        state.xer_so, true);
                    if (!state.cr6.gt)
                    {
                        R(state, 11) = memory.ReadU32(
                            Address(R(state, 21) + 31888u));
                        memory.WriteU16(Address(R(state, 5)),
                            static_cast<std::uint16_t>(R(state, 11)));
                        R(state, 5) += 2u;
                    }
                }
                else
                {
                    R(state, 9) = memory.ReadU32(Address(R(state, 25) + 31904u));
                    R(state, 7) = R(state, 3) + 1u;
                    R(state, 11) -= R(state, 9);
                    Compare(state.cr6, R(state, 7), R(state, 6),
                        state.xer_so, true);
                    if (!state.cr6.gt)
                    {
                        R(state, 9) = memory.ReadU32(
                            Address(R(state, 29) + 31900u));
                        const auto shift = static_cast<unsigned>(R(state, 9) & 63u);
                        R(state, 8) = (shift & 32u) ? 0 :
                            (Address(R(state, 11)) >> shift);
                        R(state, 9) = memory.ReadU32(
                            Address(R(state, 28) + 31912u));
                        R(state, 9) += R(state, 8);
                        memory.WriteU16(Address(R(state, 5)),
                            static_cast<std::uint16_t>(R(state, 9)));
                        R(state, 5) += 2u;
                    }
                    R(state, 3) = R(state, 7) + 1u;
                    Compare(state.cr6, R(state, 3), R(state, 6),
                        state.xer_so, true);
                    if (!state.cr6.gt)
                    {
                        R(state, 9) = memory.ReadU32(
                            Address(R(state, 26) + 31908u));
                        R(state, 9) &= R(state, 11);
                        R(state, 11) = memory.ReadU32(
                            Address(R(state, 27) + 31916u));
                        R(state, 11) += R(state, 9);
                        memory.WriteU16(Address(R(state, 5)),
                            static_cast<std::uint16_t>(R(state, 11)));
                        R(state, 5) += 2u;
                    }
                }
            }
            Compare(state.cr6, R(state, 31), R(state, 4), state.xer_so, true);
        } while (state.cr6.lt);
    }
    Compare(state.cr6, R(state, 6), 0, state.xer_so, true);
    if (!state.cr6.eq)
    {
        Compare(state.cr6, R(state, 6), R(state, 3), state.xer_so, true);
        if (state.cr6.lt)
        {
            R(state, 3) = 122;
            StoreError(memory, state, 0x827ca850u);
            R(state, 3) = 0;
        }
    }
    LeaveUtf8(memory, state);
}

void EnterDispatch(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(state.sp - 16u), R(state, 31));
    Push(memory, state, 96u);
}

void LeaveDispatch(GuestMemory& memory, Registers& state)
{
    state.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

void Dispatch(GuestMemory& memory, NativeServices& native, Registers& state)
{
    EnterDispatch(memory, state);
    R(state, 11) = R(state, 3);
    R(state, 9) = R(state, 5);
    R(state, 4) = R(state, 6);
    R(state, 3) = R(state, 7);
    Compare(state.cr6, R(state, 11), 65001u, state.xer_so, false);
    if (state.cr6.eq)
    {
        R(state, 5) = R(state, 3);
        R(state, 6) = R(state, 8);
        R(state, 3) = R(state, 9);
        state.lr = 0x8229c598u;
        DecodeUtf8(memory, state);
    }
    else
    {
        Compare(state.cr6, R(state, 4), UINT32_MAX, state.xer_so, true);
        if (state.cr6.eq)
        {
            R(state, 11) = R(state, 9);
            R(state, 10) = R(state, 11);
            do
            {
                R(state, 7) = memory.ReadU8(Address(R(state, 11)));
                ++R(state, 11);
                Compare(state.cr6, R(state, 7), 0, state.xer_so, false);
            } while (!state.cr6.eq);
            R(state, 11) -= R(state, 10);
            R(state, 11) -= 1u;
            R(state, 11) = Address(R(state, 11));
            R(state, 31) = R(state, 11) + 1u;
        }
        else
            R(state, 31) = R(state, 4);
        Compare(state.cr6, R(state, 8), 0, state.xer_so, true);
        if (state.cr6.eq)
            R(state, 3) = R(state, 31);
        else
        {
            Compare(state.cr6, R(state, 31), 0, state.xer_so, true);
            bool insufficient = state.cr6.lt;
            if (!insufficient)
            {
                Compare(state.cr6, R(state, 8), R(state, 31),
                    state.xer_so, true);
                insufficient = state.cr6.lt;
            }
            if (insufficient)
                R(state, 3) = 122;
            else
            {
                R(state, 7) = R(state, 31);
                R(state, 6) = R(state, 9);
                R(state, 5) = 0;
                R(state, 4) = WordRotateMask(R(state, 8), 1, 0xfffffffeu);
                state.lr = 0x8229c608u;
                native.RtlMultiByteToUnicodeN(memory, state);
                Compare(state.cr0, R(state, 3), 0, state.xer_so, true);
                if (!state.cr0.lt)
                {
                    R(state, 3) = R(state, 31);
                    LeaveDispatch(memory, state);
                    return;
                }
                state.lr = 0x8229c614u;
                native.RtlNtStatusToDosError(memory, state);
            }
            StoreError(memory, state, 0x8229c620u);
            R(state, 3) = 0;
        }
    }
    LeaveDispatch(memory, state);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    switch (address)
    {
    case kDispatch: Dispatch(memory, native, state); return true;
    case kUtf8: DecodeUtf8(memory, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_utf8_conversion_routes
