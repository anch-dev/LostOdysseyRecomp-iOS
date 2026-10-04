#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::crt_async_status_transfer
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
std::uint64_t& R(Registers& s, unsigned i) { return s.r[i]; }
std::uint32_t W(std::uint64_t value) { return Address(value); }
std::int32_t S(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(W(value)); }
void CmpS(Condition& c, Registers& s, std::uint64_t a, std::uint64_t b)
{ c = {std::uint8_t(S(a) < S(b)), std::uint8_t(S(a) > S(b)),
      std::uint8_t(S(a) == S(b)), s.xer_so}; }
void CmpU(Condition& c, Registers& s, std::uint64_t a, std::uint64_t b)
{ c = {std::uint8_t(W(a) < W(b)), std::uint8_t(W(a) > W(b)),
      std::uint8_t(W(a) == W(b)), s.xer_so}; }
void Save(GuestMemory& m, Registers& s)
{
    const auto old_sp = W(R(s, 1));
    R(s, 12) = s.lr;
    s.lr = 0x82be2de0u;
    for (unsigned i = 28u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    R(s, 1) -= 144u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore(GuestMemory& m, Registers& s)
{
    R(s, 1) += 144u;
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 28u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, old_sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(old_sp - 8u);
    s.lr = R(s, 12);
}
void CopyToLower(const Registers& s, crt_status_error::Registers& lower)
{
    lower.sp = s.r[1]; lower.lr = s.lr;
    lower.r3 = s.r[3]; lower.r11 = s.r[11];
    lower.r12 = s.r[12]; lower.r13 = s.r[13];
    lower.xer_so = s.xer_so; lower.cr6 = s.cr6;
}
void CopyFromLower(Registers& s, const crt_status_error::Registers& lower)
{
    R(s, 1) = lower.sp; s.lr = lower.lr;
    R(s, 3) = lower.r3; R(s, 11) = lower.r11;
    R(s, 12) = lower.r12; R(s, 13) = lower.r13;
    s.xer_so = lower.xer_so; s.cr6 = lower.cr6;
}
class LowerNative final : public crt_status_error::NativeServices
{
public:
    LowerNative(crt_async_status_transfer::NativeServices& native, Registers& full)
        : native_(native), full_(full) {}
    void NtStatusToDosError(GuestMemory& memory,
        crt_status_error::Registers& lower) override
    {
        CopyFromLower(full_, lower);
        native_.NtStatusToDosError(memory, full_);
        CopyToLower(full_, lower);
    }
private:
    crt_async_status_transfer::NativeServices& native_;
    Registers& full_;
};
void ConvertStatus(GuestMemory& memory, NativeServices& native,
    Registers& s)
{
    s.lr = 0x82be2f50u;
    crt_status_error::Registers lower{};
    CopyToLower(s, lower);
    LowerNative adapter(native, s);
    (void)crt_status_error::Apply(0x827ca628u, memory, adapter, lower);
    CopyFromLower(s, lower);
    R(s, 3) = 0u;
}
void Indirect(GuestMemory& m, NativeServices& native, Registers& s,
    std::uint32_t return_address)
{
    R(s, 11) = 0xffffffff831e0000ull;
    R(s, 11) = m.ReadU32(W(R(s, 11) + 32244u));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 16u));
    s.ctr = R(s, 11);
    s.lr = return_address;
    native.CallIndirect(W(s.ctr) & ~3u, m, s);
}
void External(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 4) = m.ReadU32(W(R(s, 31) + 16u));
    R(s, 11) = 259u;
    R(s, 10) = m.ReadU32(W(R(s, 31) + 8u));
    R(s, 6) = R(s, 28);
    m.WriteU32(W(R(s, 31)), W(R(s, 11)));
    m.WriteU32(W(R(s, 1) + 92u), W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(R(s, 31) + 12u));
    m.WriteU32(W(R(s, 1) + 88u), W(R(s, 10)));
    R(s, 11) = W(R(s, 4)) & 1u;
    CmpS(s.cr0, s, R(s, 11), 0u);
    if (s.cr0.eq) R(s, 6) = R(s, 31);
    R(s, 10) = R(s, 1) + 88u;
    R(s, 7) = R(s, 31);
    R(s, 5) = 0u;
    R(s, 3) = R(s, 30);
    Indirect(m, native, s, 0x82be2e60u);
    CmpS(s.cr0, s, R(s, 3), 0u);
    if (!s.cr0.lt)
    {
        CmpS(s.cr6, s, R(s, 3), 259u);
        if (!s.cr6.eq)
        {
            CmpU(s.cr6, s, R(s, 29), 0u);
            if (!s.cr6.eq)
            {
                R(s, 11) = m.ReadU32(W(R(s, 31) + 4u));
                m.WriteU32(W(R(s, 29)), W(R(s, 11)));
            }
            R(s, 3) = 1u;
            return;
        }
    }
    R(s, 11) = 0xffffffffc0000000ull;
    R(s, 11) |= 17u;
    CmpS(s.cr6, s, R(s, 3), R(s, 11));
    if (s.cr6.eq)
    {
        CmpU(s.cr6, s, R(s, 29), 0u);
        if (!s.cr6.eq) m.WriteU32(W(R(s, 29)), W(R(s, 28)));
        R(s, 3) = R(s, 11);
    }
    ConvertStatus(m, native, s);
}
void LocalFailure(GuestMemory& m, NativeServices& native, Registers& s);
void Local(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 10) = 0u;
    R(s, 7) = R(s, 1) + 80u;
    R(s, 6) = 0u;
    R(s, 5) = 0u;
    R(s, 4) = 0u;
    R(s, 3) = R(s, 30);
    Indirect(m, native, s, 0x82be2ed8u);
    CmpS(s.cr6, s, R(s, 3), 259u);
    if (s.cr6.eq)
    {
        R(s, 6) = 0u;
        R(s, 5) = 0u;
        R(s, 4) = 1u;
        R(s, 3) = R(s, 30);
        s.lr = 0x82be2ef4u;
        native.NtWaitForSingleObjectEx(m, s);
        CmpS(s.cr0, s, R(s, 3), 0u);
        if (!s.cr0.lt) R(s, 3) = m.ReadU32(W(R(s, 1) + 80u));
        else { LocalFailure(m, native, s); return; }
    }
    CmpS(s.cr6, s, R(s, 3), 0u);
    if (s.cr6.lt) { LocalFailure(m, native, s); return; }
    R(s, 11) = m.ReadU32(W(R(s, 1) + 84u));
    R(s, 3) = 1u;
    m.WriteU32(W(R(s, 29)), W(R(s, 11)));
}
void LocalFailure(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 11) = 0xffffffffc0000000ull;
    R(s, 11) |= 17u;
    CmpS(s.cr6, s, R(s, 3), R(s, 11));
    if (s.cr6.eq)
    {
        R(s, 3) = 1u;
        m.WriteU32(W(R(s, 29)), W(R(s, 28)));
        return;
    }
    R(s, 11) = recovery_abi::WordRotateMask(R(s, 3), 0, 0xc0000000u);
    R(s, 10) = 0xffffffff80000000ull;
    CmpU(s.cr6, s, R(s, 11), R(s, 10));
    if (s.cr6.eq)
    {
        R(s, 11) = m.ReadU32(W(R(s, 1) + 84u));
        m.WriteU32(W(R(s, 29)), W(R(s, 11)));
    }
    ConvertStatus(m, native, s);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry != 0x82be2dd8u) return false;
    Save(memory, state);
    R(state, 29) = R(state, 6);
    R(state, 30) = R(state, 3);
    R(state, 8) = R(state, 4);
    R(state, 9) = R(state, 5);
    R(state, 31) = R(state, 7);
    R(state, 28) = 0u;
    CmpU(state.cr6, state, R(state, 29), 0u);
    if (!state.cr6.eq) memory.WriteU32(W(R(state, 29)), 0u);
    CmpU(state.cr6, state, R(state, 31), 0u);
    if (state.cr6.eq) Local(memory, native, state);
    else External(memory, native, state);
    Restore(memory, state);
    return true;
}
} // namespace lo::semantic::gpu::crt_async_status_transfer
