#include "lo_semantics/manager_init_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::manager_init_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s, bool signed_word)
{
    const auto word = Address(s.r[3]);
    const auto signed_value = std::bit_cast<std::int32_t>(word);
    s.cr6 = {std::uint8_t(signed_word && signed_value < 0),
        std::uint8_t(signed_word ? signed_value > 0 : word > 0),
        std::uint8_t(word == 0), s.xer_so};
}
void Direct(GuestAddress target, GuestAddress continuation,
    GuestMemory& m, PpcBoundaryServices& b, Registers& s)
{
    s.lr = continuation;
    b.CallDirect(target, m, s);
}
void Virtual(unsigned offset, GuestAddress continuation,
    GuestMemory& m, PpcBoundaryServices& b, Registers& s)
{
    s.r[11] = m.ReadU32(Address(s.r[3]));
    s.r[11] = m.ReadU32(Address(s.r[11] + offset));
    s.ctr = s.r[11];
    s.lr = continuation;
    b.CallVirtual(Address(s.ctr) & ~GuestAddress{3}, m, s);
}
}
void Apply(GuestMemory& m, PpcBoundaryServices& b, Registers& s)
{
    s.r[12] = s.lr;
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
    WriteU64(m, Address(s.r[1] - 24u), s.r[30]);
    WriteU64(m, Address(s.r[1] - 16u), s.r[31]);
    s.r[31] = s.r[1] - 112u;
    const auto previous_sp = Address(s.r[1]);
    s.r[1] -= 112u;
    m.WriteU32(Address(s.r[1]), previous_sp);
    s.r[3] = 0x48decu;
    Direct(0x823acbd0u, 0x827c5f5cu, m, b, s);
    m.WriteU32(Address(s.r[31] + 80u), Address(s.r[3]));
    Compare(s, false);
    if (!s.cr6.eq) Direct(0x827c5970u, 0x827c5f6cu, m, b, s);
    else s.r[3] = 0;
    s.r[30] = 0xffffffff83310000ull;
    m.WriteU32(Address(s.r[30] - 18936u), Address(s.r[3]));
    Virtual(60u, 0x827c5f8cu, m, b, s);
    Compare(s, true);
    if (s.cr6.eq)
    {
        s.r[3] = 36;
        Direct(0x823acbd0u, 0x827c5f9cu, m, b, s);
        m.WriteU32(Address(s.r[31] + 80u), Address(s.r[3]));
        Compare(s, false);
        if (!s.cr6.eq)
        {
            s.r[4] = m.ReadU32(Address(s.r[30] - 18936u));
            Direct(0x827c4ed0u, 0x827c5fb0u, m, b, s);
        }
        else s.r[3] = 0;
        m.WriteU32(Address(s.r[30] - 18936u), Address(s.r[3]));
    }
    else s.r[3] = m.ReadU32(Address(s.r[30] - 18936u));
    Virtual(56u, 0x827c5fd4u, m, b, s);
    s.r[1] = s.r[31] + 112u;
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    s.r[30] = ReadU64(m, Address(s.r[1] - 24u));
    s.r[31] = ReadU64(m, Address(s.r[1] - 16u));
}
}
