#include "lo_semantics/crt_stream_operations.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/crt_last_error.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <bit>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_operations
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

constexpr GuestAddress kCount = 0x83378d68u;

std::uint64_t& R(Registers& s, unsigned index) { return s.r[index]; }

void Compare(Condition& cr, std::uint64_t a, std::uint64_t b,
    std::uint8_t so, bool signed_words)
{
    const auto left = static_cast<std::uint32_t>(a);
    const auto right = static_cast<std::uint32_t>(b);
    if (signed_words)
    {
        const auto x = static_cast<std::int32_t>(left);
        const auto y = static_cast<std::int32_t>(right);
        cr = {std::uint8_t(x < y), std::uint8_t(x > y),
            std::uint8_t(x == y), so};
    }
    else
        cr = {std::uint8_t(left < right), std::uint8_t(left > right),
            std::uint8_t(left == right), so};
}

void Save(GuestMemory& m, Registers& s, unsigned first,
    std::uint64_t save_return)
{
    R(s, 12) = s.lr;
    for (unsigned index = first; index <= 31; ++index)
        WriteU64(m, Address(s.sp - 16u - 8u * (31u - index)), R(s, index));
    m.WriteU32(Address(s.sp - 8u), Address(R(s, 12)));
    s.lr = save_return;
}

void Restore(GuestMemory& m, Registers& s, unsigned first)
{
    for (unsigned index = first; index <= 31; ++index)
        R(s, index) = ReadU64(m,
            Address(s.sp - 16u - 8u * (31u - index)));
    R(s, 12) = m.ReadU32(Address(s.sp - 8u));
    s.lr = R(s, 12);
}

void Push(GuestMemory& m, Registers& s, unsigned bytes)
{
    m.WriteU32(Address(s.sp - bytes), Address(s.sp));
    s.sp -= bytes;
}

void EnterSimple(GuestMemory& m, Registers& s, unsigned bytes,
    bool save_nonvolatile)
{
    R(s, 12) = s.lr;
    m.WriteU32(Address(s.sp - 8u), Address(R(s, 12)));
    if (save_nonvolatile)
    {
        WriteU64(m, Address(s.sp - 24u), R(s, 30));
        WriteU64(m, Address(s.sp - 16u), R(s, 31));
    }
    Push(m, s, bytes);
}

void LeaveSimple(GuestMemory& m, Registers& s, unsigned bytes,
    bool restore_nonvolatile)
{
    s.sp += bytes;
    R(s, 12) = m.ReadU32(Address(s.sp - 8u));
    s.lr = R(s, 12);
    if (restore_nonvolatile)
    {
        R(s, 30) = ReadU64(m, Address(s.sp - 24u));
        R(s, 31) = ReadU64(m, Address(s.sp - 16u));
    }
}

class ErrorAddressServices final : public AllocationFailureServices
{
public:
    ErrorAddressServices(GuestMemory& memory, CrtThreadDataServices& thread,
        Registers& registers) : memory_(memory), thread_(thread), s_(registers) {}
    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall call{s_.r[13]};
        const auto value = GetCrtThreadData(memory_, thread_, call);
        s_.r[13] = call.thread_environment;
        s_.r[3] = value;
        return value;
    }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::logic_error("unexpected CRT error output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::logic_error("unexpected CRT bug check"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::logic_error("unexpected CRT new handler"); }
private:
    GuestMemory& memory_;
    CrtThreadDataServices& thread_;
    Registers& s_;
};

void ErrorAddress(GuestMemory& m, Dependencies d, Registers& s)
{
    EnterSimple(m, s, 96u, false);
    ErrorAddressServices services(m, d.thread, s);
    R(s, 3) = GetAllocationErrorAddress(services);
    LeaveSimple(m, s, 96u, false);
}

void Invalid(GuestMemory& m, Dependencies d, Registers& s)
{
    InvalidParameterCall call{{{R(s, 3), R(s, 4), R(s, 5), R(s, 6),
        R(s, 7), R(s, 8), R(s, 9), R(s, 10)}}, R(s, 13)};
    R(s, 3) = ReportInvalidParameter(m, d.invalid, call);
    for (unsigned index = 1; index < 8; ++index)
        R(s, index + 3u) = call.arguments[index];
    R(s, 13) = call.thread_environment;
}

void StreamError(GuestAddress address, GuestMemory& m, Dependencies d,
    Registers& s)
{
    crt_stream_error::Registers lower{};
    lower.sp=s.sp; lower.lr=s.lr;
    lower.r3=R(s,3); lower.r4=R(s,4); lower.r5=R(s,5);
    lower.r6=R(s,6); lower.r7=R(s,7); lower.r8=R(s,8);
    lower.r9=R(s,9); lower.r10=R(s,10); lower.r11=R(s,11);
    lower.r12=R(s,12); lower.r13=R(s,13);
    lower.r30=R(s,30); lower.r31=R(s,31);
    lower.xer_so=s.xer_so; lower.xer_ca=s.xer_ca;
    lower.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,s.cr0.so};
    lower.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,s.cr6.so};
    if (!crt_stream_error::Apply(address,m,d.thread,d.invalid,lower))
        throw std::logic_error("missing accepted stream-error callee");
    s.sp=lower.sp; s.lr=lower.lr;
    R(s,3)=lower.r3; R(s,4)=lower.r4; R(s,5)=lower.r5;
    R(s,6)=lower.r6; R(s,7)=lower.r7; R(s,8)=lower.r8;
    R(s,9)=lower.r9; R(s,10)=lower.r10; R(s,11)=lower.r11;
    R(s,12)=lower.r12; R(s,13)=lower.r13;
    R(s,30)=lower.r30; R(s,31)=lower.r31;
    s.xer_so=lower.xer_so; s.xer_ca=lower.xer_ca;
    s.cr0={lower.cr0.lt,lower.cr0.gt,lower.cr0.eq,lower.cr0.so};
    s.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}

void LastError(GuestMemory& m, Registers& s)
{
    crt_last_error::Registers lower{};
    lower.r3=R(s,3); lower.r11=R(s,11); lower.r13=R(s,13);
    lower.xer_so=s.xer_so;
    lower.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,s.cr6.so};
    if (!crt_last_error::Apply(0x822ca100u,m,lower))
        throw std::logic_error("missing accepted last-error callee");
    R(s,3)=lower.r3; R(s,11)=lower.r11; R(s,13)=lower.r13;
    s.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}

void StreamIo(GuestAddress address, GuestMemory& m, Dependencies d,
    Registers& s)
{
    crt_stream_io::Registers lower{};
    lower.sp=s.sp; lower.lr=s.lr; lower.ctr=s.ctr;
    lower.r3=R(s,3); lower.r4=R(s,4); lower.r5=R(s,5);
    lower.r6=R(s,6); lower.r7=R(s,7); lower.r8=R(s,8);
    lower.r9=R(s,9); lower.r10=R(s,10); lower.r11=R(s,11);
    lower.r12=R(s,12); lower.r13=R(s,13);
    lower.r28=R(s,28); lower.r29=R(s,29);
    lower.r30=R(s,30); lower.r31=R(s,31);
    lower.xer_so=s.xer_so;
    lower.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,s.cr0.so};
    lower.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,s.cr6.so};
    if (!crt_stream_io::Apply(address,m,d.io,lower))
        throw std::logic_error("missing accepted stream-I/O callee");
    s.sp=lower.sp; s.lr=lower.lr; s.ctr=lower.ctr;
    R(s,3)=lower.r3; R(s,4)=lower.r4; R(s,5)=lower.r5;
    R(s,6)=lower.r6; R(s,7)=lower.r7; R(s,8)=lower.r8;
    R(s,9)=lower.r9; R(s,10)=lower.r10; R(s,11)=lower.r11;
    R(s,12)=lower.r12; R(s,13)=lower.r13;
    R(s,28)=lower.r28; R(s,29)=lower.r29;
    R(s,30)=lower.r30; R(s,31)=lower.r31;
    s.xer_so=lower.xer_so;
    s.cr0={lower.cr0.lt,lower.cr0.gt,lower.cr0.eq,lower.cr0.so};
    s.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}

void Lock(GuestMemory& m, Dependencies d, Registers& s)
{
    crt_stream_locks::Registers lower{};
    lower.sp=s.sp; lower.lr=s.lr; lower.ctr=s.ctr;
    lower.r3=R(s,3); lower.r4=R(s,4); lower.r5=R(s,5);
    lower.r6=R(s,6); lower.r7=R(s,7); lower.r8=R(s,8);
    lower.r9=R(s,9); lower.r10=R(s,10); lower.r11=R(s,11);
    lower.r12=R(s,12); lower.r13=R(s,13);
    lower.r28=R(s,28); lower.r29=R(s,29);
    lower.r30=R(s,30); lower.r31=R(s,31);
    lower.xer_so=s.xer_so; lower.xer_ca=s.xer_ca;
    lower.cr0={s.cr0.lt!=0,s.cr0.gt!=0,s.cr0.eq!=0,s.cr0.so!=0};
    lower.cr6={s.cr6.lt!=0,s.cr6.gt!=0,s.cr6.eq!=0,s.cr6.so!=0};
    if (!crt_stream_locks::Apply(0x82b862f8u,m,d.locks,lower))
        throw std::logic_error("missing accepted stream-lock callee");
    s.sp=lower.sp; s.lr=lower.lr; s.ctr=lower.ctr;
    R(s,3)=lower.r3; R(s,4)=lower.r4; R(s,5)=lower.r5;
    R(s,6)=lower.r6; R(s,7)=lower.r7; R(s,8)=lower.r8;
    R(s,9)=lower.r9; R(s,10)=lower.r10; R(s,11)=lower.r11;
    R(s,12)=lower.r12; R(s,13)=lower.r13;
    R(s,28)=lower.r28; R(s,29)=lower.r29;
    R(s,30)=lower.r30; R(s,31)=lower.r31;
    s.xer_so=lower.xer_so; s.xer_ca=lower.xer_ca;
    s.cr0={std::uint8_t(lower.cr0.lt),std::uint8_t(lower.cr0.gt),
        std::uint8_t(lower.cr0.eq),std::uint8_t(lower.cr0.so)};
    s.cr6={std::uint8_t(lower.cr6.lt),std::uint8_t(lower.cr6.gt),
        std::uint8_t(lower.cr6.eq),std::uint8_t(lower.cr6.so)};
}

void Unlock(GuestAddress address, GuestMemory& m, Dependencies d,
    Registers& s)
{
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp=s.sp; lower.lr=s.lr; lower.r3=R(s,3);
    lower.r9=R(s,9); lower.r10=R(s,10); lower.r11=R(s,11);
    lower.r12=R(s,12); lower.r30=R(s,30); lower.r31=R(s,31);
    lower.xer_ca=s.xer_ca;
    if (!crt_stream_pointer_unlock::Apply(address,m,d.unlock,lower))
        throw std::logic_error("missing accepted pointer-unlock callee");
    s.sp=lower.sp; s.lr=lower.lr; R(s,3)=lower.r3;
    R(s,9)=lower.r9; R(s,10)=lower.r10; R(s,11)=lower.r11;
    R(s,12)=lower.r12; R(s,30)=lower.r30; R(s,31)=lower.r31;
    s.xer_ca=lower.xer_ca;
}

void StreamState(GuestAddress address, GuestMemory& m, Dependencies d,
    Registers& s)
{
    InvalidParameterCall call{{{R(s,3),R(s,4),R(s,5),R(s,6),
        R(s,7),R(s,8),R(s,9),R(s,10)}},R(s,13)};
    crt_stream_state::FrameRegisters frame{s.lr,R(s,31),s.sp};
    std::uint64_t result=R(s,3);
    if (!crt_stream_state::Apply(address,m,d.thread,d.invalid,d.raw,
        d.state,call,s.sp,frame,result))
        throw std::logic_error("missing accepted stream-state callee");
    s.sp=frame.sp; s.lr=frame.lr; R(s,31)=frame.r31;
    R(s,3)=result;
    for (unsigned index=1; index<8; ++index)
        R(s,index+3u)=call.arguments[index];
    R(s,13)=call.thread_environment;
}

void Integer(GuestAddress address, Registers& s)
{
    integer_leaf::Registers lower{};
    lower.r3=R(s,3); lower.r4=R(s,4); lower.r5=R(s,5);
    lower.r6=R(s,6); lower.r7=R(s,7);
    lower.r10=R(s,10); lower.r11=R(s,11);
    if (!integer_leaf::Apply(address,lower))
        throw std::logic_error("missing accepted integer callee");
    R(s,3)=lower.r3; R(s,4)=lower.r4; R(s,5)=lower.r5;
    R(s,6)=lower.r6; R(s,7)=lower.r7;
    R(s,10)=lower.r10; R(s,11)=lower.r11;
}

std::uint64_t StreamRecord(GuestMemory& m, Registers& s, std::uint64_t handle)
{
    const auto index=static_cast<std::int32_t>(handle);
    s.xer_ca=std::uint8_t(index<0 && (Address(handle)&31u)!=0);
    R(s,10)=static_cast<std::uint64_t>(index>>5);
    R(s,9)=WordRotateMask(R(s,10),2,0xfffffffcu);
    R(s,11)=0xffffffff83378d80ull;
    R(s,10)=WordRotateMask(handle,6,0x7c0u);
    R(s,11)=m.ReadU32(Address(R(s,9)+R(s,11)));
    R(s,11)+=R(s,10);
    return R(s,11);
}

void Seek(GuestMemory& m, Dependencies d, Registers& s)
{
    EnterSimple(m,s,112,true);
    R(s,31)=R(s,3);
    WriteU64(m,Address(s.sp+80u),R(s,4));
    R(s,30)=R(s,5);
    s.lr=0x82b85eccu;
    StreamError(0x82b86228u,m,d,s);
    Compare(s.cr6,R(s,3),UINT32_MAX,s.xer_so,true);
    if (s.cr6.eq)
    {
        s.lr=0x82b85ed8u;
        ErrorAddress(m,d,s);
        R(s,11)=R(s,3); R(s,10)=9; R(s,3)=UINT64_MAX;
        m.WriteU32(Address(R(s,11)),9);
    }
    else
    {
        R(s,6)=R(s,30);
        R(s,4)=m.ReadU32(Address(s.sp+84u));
        R(s,5)=s.sp+80u;
        s.lr=0x82b85efcu;
        StreamIo(0x82be2938u,m,d,s);
        m.WriteU32(Address(s.sp+84u),Address(R(s,3)));
        Compare(s.cr6,R(s,3),UINT32_MAX,s.xer_so,true);
        bool failed=false;
        if (s.cr6.eq)
        {
            s.lr=0x82b85f0cu;
            LastError(m,s);
            Compare(s.cr0,R(s,3),0,s.xer_so,false);
            if (!s.cr0.eq)
            {
                s.lr=0x82b85f18u;
                StreamError(0x82b7fde8u,m,d,s);
                R(s,3)=UINT64_MAX;
                failed=true;
            }
        }
        if (!failed)
        {
            const auto index=static_cast<std::int32_t>(R(s,31));
            s.xer_ca=std::uint8_t(index<0 && (Address(R(s,31))&31u)!=0);
            R(s,10)=static_cast<std::uint64_t>(index>>5);
            R(s,9)=WordRotateMask(R(s,10),2,0xfffffffcu);
            R(s,11)=0xffffffff83380000ull;
            R(s,11)-=29312u;
            R(s,10)=WordRotateMask(R(s,31),6,0x7c0u);
            R(s,11)=m.ReadU32(Address(R(s,9)+R(s,11)));
            R(s,11)+=R(s,10);
            R(s,10)=static_cast<std::int64_t>(
                static_cast<std::int8_t>(m.ReadU8(Address(R(s,11)+4u))));
            R(s,10)=WordRotateMask(R(s,10),0,0xfffffffffffffffdull);
            m.WriteU8(Address(R(s,11)+4u),static_cast<std::uint8_t>(R(s,10)));
            R(s,3)=ReadU64(m,Address(s.sp+80u));
        }
    }
    LeaveSimple(m,s,112,true);
}

void InvalidWrite(GuestMemory& m, Dependencies d, Registers& s)
{
    s.lr=0x82b81bc4u;
    StreamError(0x82b7fdb0u,m,d,s);
    m.WriteU32(Address(R(s,3)),0);
    s.lr=0x82b81bccu;
    ErrorAddress(m,d,s);
    R(s,11)=R(s,3); R(s,10)=22;
    R(s,7)=R(s,6)=R(s,5)=R(s,4)=R(s,3)=0;
    m.WriteU32(Address(R(s,11)),22);
    s.lr=0x82b81bf0u;
    Invalid(m,d,s);
    R(s,3)=UINT64_MAX;
}

void Write(GuestMemory& m, Dependencies d, Registers& s)
{
    Save(m,s,21,0x82b81b90u);
    Push(m,s,1232);
    R(s,21)=0; R(s,26)=R(s,5); R(s,24)=R(s,4);
    R(s,23)=0; R(s,22)=0;
    Compare(s.cr6,R(s,26),0,s.xer_so,false);
    if (s.cr6.eq)
    {
        R(s,3)=0;
    }
    else
    {
        Compare(s.cr6,R(s,24),0,s.xer_so,false);
        if (s.cr6.eq)
        {
            InvalidWrite(m,d,s);
            goto done;
        }
        R(s,11)=0xffffffff83380000ull;
        R(s,28)=WordRotateMask(R(s,3),6,0x7c0u);
        R(s,29)=R(s,11)-29312u;
        const auto index=static_cast<std::int32_t>(R(s,3));
        s.xer_ca=std::uint8_t(index<0 && (Address(R(s,3))&31u)!=0);
        R(s,11)=static_cast<std::uint64_t>(index>>5);
        R(s,27)=WordRotateMask(R(s,11),2,0xfffffffcu);
        R(s,11)=m.ReadU32(Address(R(s,27)+R(s,29)));
        R(s,10)=R(s,11)+R(s,28);
        R(s,11)=m.ReadU8(Address(R(s,10)+40u));
        R(s,11)=std::rotl(static_cast<std::uint32_t>(R(s,11)),24);
        const auto mode=static_cast<std::int32_t>(R(s,11))>>25;
        s.xer_ca=std::uint8_t(static_cast<std::int32_t>(R(s,11))<0 &&
            (Address(R(s,11))&0x1ffffffu)!=0);
        R(s,11)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
            static_cast<std::int8_t>(mode)));
        Compare(s.cr6,R(s,11),2,s.xer_so,true);
        if (s.cr6.eq || (Compare(s.cr6,R(s,11),1,s.xer_so,true),s.cr6.eq))
        {
            R(s,11)=~R(s,26);
            R(s,11)=WordRotateMask(R(s,11),0,1u);
            Compare(s.cr0,R(s,11),0,s.xer_so,true);
            if (s.cr0.eq)
            {
                InvalidWrite(m,d,s);
                goto done;
            }
        }
        R(s,11)=m.ReadU8(Address(R(s,10)+4u));
        R(s,11)=WordRotateMask(R(s,11),0,0x20u);
        Compare(s.cr0,R(s,11),0,s.xer_so,true);
        if (!s.cr0.eq)
        {
            R(s,5)=2; R(s,4)=0;
            s.lr=0x82b81c58u;
            Seek(m,d,s);
        }
        R(s,11)=m.ReadU32(Address(R(s,27)+R(s,29)))+R(s,28);
        R(s,10)=m.ReadU8(Address(R(s,11)+4u));
        R(s,10)=WordRotateMask(R(s,10),0,0xffffff80u);
        Compare(s.cr0,R(s,10),0,s.xer_so,true);
        R(s,25)=0;
        if (!s.cr0.eq)
        {
            R(s,30)=R(s,24);
            Compare(s.cr6,R(s,26),0,s.xer_so,false);
            do
            {
                R(s,11)=s.sp+96u;
                R(s,10)=0;
                R(s,9)=R(s,30)-R(s,24);
                while (true)
                {
                    Compare(s.cr6,R(s,9),R(s,26),s.xer_so,false);
                    if (!s.cr6.lt) break;
                    R(s,8)=m.ReadU8(Address(R(s,30)));
                    ++R(s,9); ++R(s,30);
                    Compare(s.cr6,R(s,8),10,s.xer_so,false);
                    if (s.cr6.eq)
                    {
                        R(s,7)=13;
                        ++R(s,22); ++R(s,10);
                        m.WriteU8(Address(R(s,11)++),13);
                    }
                    ++R(s,10);
                    m.WriteU8(Address(R(s,11)++),static_cast<std::uint8_t>(R(s,8)));
                    Compare(s.cr6,R(s,10),1024,s.xer_so,false);
                    if (!s.cr6.lt) break;
                }
                R(s,10)=s.sp+96u;
                R(s,9)=m.ReadU32(Address(R(s,27)+R(s,29)));
                R(s,7)=0;
                R(s,31)=R(s,11)-R(s,10);
                R(s,6)=s.sp+80u; R(s,4)=s.sp+96u;
                R(s,5)=R(s,31);
                R(s,3)=m.ReadU32(Address(R(s,9)+R(s,28)));
                s.lr=0x82b81cf0u;
                StreamIo(0x82be2810u,m,d,s);
                Compare(s.cr0,R(s,3),0,s.xer_so,true);
                if (s.cr0.eq)
                {
                    s.lr=0x82b81d48u;
                    LastError(m,s);
                    R(s,25)=R(s,3);
                    break;
                }
                R(s,11)=m.ReadU32(Address(s.sp+80u));
                R(s,23)+=R(s,11);
                Compare(s.cr6,R(s,11),R(s,31),s.xer_so,true);
                if (s.cr6.lt) break;
                R(s,11)=R(s,30)-R(s,24);
                Compare(s.cr6,R(s,11),R(s,26),s.xer_so,false);
            } while (s.cr6.lt);
        }
        else
        {
            R(s,7)=0;
            R(s,3)=m.ReadU32(Address(R(s,11)));
            R(s,6)=s.sp+80u; R(s,5)=R(s,26); R(s,4)=R(s,24);
            s.lr=0x82b81d30u;
            StreamIo(0x82be2810u,m,d,s);
            Compare(s.cr0,R(s,3),0,s.xer_so,true);
            if (s.cr0.eq)
            {
                s.lr=0x82b81d48u;
                LastError(m,s);
                R(s,25)=R(s,3);
            }
            else
            {
                R(s,23)=m.ReadU32(Address(s.sp+80u));
                R(s,25)=0;
            }
        }
        Compare(s.cr6,R(s,23),0,s.xer_so,true);
        if (s.cr6.eq)
        {
            Compare(s.cr6,R(s,25),0,s.xer_so,false);
            if (!s.cr6.eq)
            {
                Compare(s.cr6,R(s,25),5,s.xer_so,false);
                if (s.cr6.eq)
                {
                    s.lr=0x82b81d68u; ErrorAddress(m,d,s);
                    R(s,11)=9; m.WriteU32(Address(R(s,3)),9);
                    s.lr=0x82b81d74u;
                    StreamError(0x82b7fdb0u,m,d,s);
                    R(s,11)=5; m.WriteU32(Address(R(s,3)),5);
                }
                else
                {
                    R(s,3)=R(s,25); s.lr=0x82b81d88u;
                    StreamError(0x82b7fde8u,m,d,s);
                }
                R(s,3)=UINT64_MAX;
                goto done;
            }
            R(s,11)=m.ReadU32(Address(R(s,27)+R(s,29)))+R(s,28);
            R(s,11)=m.ReadU8(Address(R(s,11)+4u));
            R(s,11)=WordRotateMask(R(s,11),0,0x40u);
            Compare(s.cr0,R(s,11),0,s.xer_so,true);
            if (!s.cr0.eq)
            {
                R(s,11)=m.ReadU8(Address(R(s,24)));
                Compare(s.cr6,R(s,11),26,s.xer_so,false);
                if (s.cr6.eq) { R(s,3)=0; goto done; }
            }
            s.lr=0x82b81db0u; ErrorAddress(m,d,s);
            R(s,11)=28; m.WriteU32(Address(R(s,3)),28);
            s.lr=0x82b81dbcu;
            StreamError(0x82b7fdb0u,m,d,s);
            R(s,11)=0; m.WriteU32(Address(R(s,3)),0);
            R(s,3)=UINT64_MAX;
        }
        else R(s,3)=R(s,23)-R(s,22);
    }
done:
    s.sp+=1232u;
    Restore(m,s,21);
}

void Locked(GuestMemory& m, Dependencies d, Registers& s, bool seek)
{
    Save(m,s,seek?24u:25u,seek?0x82b85f78u:0x82b81de8u);
    R(s,31)=s.sp-160u;
    Push(m,s,160u);
    R(s,30)=R(s,3);
    m.WriteU32(Address(R(s,31)+180u),Address(R(s,30)));
    R(s,26)=R(s,4); R(s,25)=R(s,5);
    if (seek)
    {
        R(s,24)=UINT64_MAX;
        WriteU64(m,Address(R(s,31)+80u),R(s,24));
    }
    const auto handle=static_cast<std::int32_t>(R(s,30));
    Compare(s.cr6,R(s,30),0xfffffffeu,s.xer_so,true);
    bool bad=s.cr6.eq;
    if (!bad)
    {
        Compare(s.cr6,R(s,30),0,s.xer_so,true);
        bad=s.cr6.lt;
        if (!bad)
        {
            R(s,11)=0xffffffff83380000ull;
            R(s,11)=m.ReadU32(kCount);
            Compare(s.cr6,R(s,30),R(s,11),s.xer_so,false);
            bad=!s.cr6.lt;
        }
    }
    if (bad)
    {
        s.lr=seek?0x82b85fa4u:(handle==-2?0x82b81e0cu:0x82b81e48u);
        StreamError(0x82b7fdb0u,m,d,s);
        R(s,11)=0; m.WriteU32(Address(R(s,3)),0);
        s.lr=seek?0x82b85fb0u:(handle==-2?0x82b81e18u:0x82b81e54u);
        ErrorAddress(m,d,s);
        R(s,11)=R(s,3); R(s,10)=9;
        if (seek || handle==-2)
            R(s,3)=UINT64_MAX;
        else
        {
            R(s,7)=R(s,6)=R(s,5)=R(s,4)=R(s,3)=0;
        }
        m.WriteU32(Address(R(s,11)),9);
        if (!seek && handle!=-2)
        {
            s.lr=0x82b81e78u;
            Invalid(m,d,s);
            R(s,3)=UINT64_MAX;
        }
    }
    else
    {
        R(s,11)=0xffffffff83380000ull;
        R(s,29)=R(s,11)-29312u;
        s.xer_ca=std::uint8_t(handle<0 && (Address(R(s,30))&31u)!=0);
        R(s,11)=static_cast<std::uint64_t>(handle>>5);
        R(s,27)=WordRotateMask(R(s,11),2,0xfffffffcu);
        R(s,28)=WordRotateMask(R(s,30),6,0x7c0u);
        R(s,11)=m.ReadU32(Address(R(s,27)+R(s,29)))+R(s,28);
        R(s,11)=m.ReadU8(Address(R(s,11)+4u))&1u;
        Compare(s.cr0,R(s,11),0,s.xer_so,true);
        if (s.cr0.eq)
        {
            s.lr=seek?0x82b85fe0u:0x82b81e48u;
            StreamError(0x82b7fdb0u,m,d,s);
            R(s,11)=0; m.WriteU32(Address(R(s,3)),0);
            s.lr=seek?0x82b85fecu:0x82b81e54u;
            ErrorAddress(m,d,s);
            R(s,11)=R(s,3); R(s,10)=9;
            R(s,7)=R(s,6)=R(s,5)=R(s,4)=R(s,3)=0;
            m.WriteU32(Address(R(s,11)),9);
            s.lr=seek?0x82b86010u:0x82b81e78u;
            Invalid(m,d,s);
            R(s,3)=UINT64_MAX;
        }
        else
        {
            R(s,3)=R(s,30);
            s.lr=seek?0x82b86048u:0x82b81eb0u;
            Lock(m,d,s);
            R(s,11)=m.ReadU32(Address(R(s,27)+R(s,29)))+R(s,28);
            R(s,11)=m.ReadU8(Address(R(s,11)+4u))&1u;
            Compare(s.cr0,R(s,11),0,s.xer_so,true);
            if (!s.cr0.eq)
            {
                R(s,5)=R(s,25); R(s,4)=R(s,26); R(s,3)=R(s,30);
                s.lr=seek?0x82b86070u:0x82b81ed8u;
                if (seek)
                {
                    Seek(m,d,s);
                    WriteU64(m,Address(R(s,31)+80u),R(s,3));
                }
                else
                {
                    Write(m,d,s);
                    m.WriteU32(Address(R(s,31)+80u),Address(R(s,3)));
                }
            }
            else
            {
                s.lr=seek?0x82b8607cu:0x82b81ee4u;
                ErrorAddress(m,d,s);
                R(s,11)=9; m.WriteU32(Address(R(s,3)),9);
                s.lr=seek?0x82b86088u:0x82b81ef0u;
                StreamError(0x82b7fdb0u,m,d,s);
                R(s,11)=0; m.WriteU32(Address(R(s,3)),0);
                if (seek) WriteU64(m,Address(R(s,31)+80u),R(s,24));
                else { R(s,11)=UINT64_MAX; m.WriteU32(Address(R(s,31)+80u),Address(R(s,11))); }
            }
            R(s,12)=R(s,31)+160u;
            s.lr=seek?0x82b860a0u:0x82b81f0cu;
            Unlock(seek?0x82b860ccu:0x82b81f38u,m,d,s);
            R(s,3)=seek?ReadU64(m,Address(R(s,31)+80u)):
                m.ReadU32(Address(R(s,31)+80u));
        }
    }
    s.sp=R(s,31)+160u;
    Restore(m,s,seek?24u:25u);
}

void PutCharacter(GuestMemory& m, Dependencies d, Registers& s)
{
    Save(m,s,28,0x82b82560u);
    Push(m,s,128u);
    R(s,31)=R(s,4);
    m.WriteU32(Address(s.sp+148u),Address(R(s,3)));
    R(s,3)=R(s,31);
    s.lr=0x82b82574u;
    StreamState(0x82b81648u,m,d,s);
    R(s,11)=m.ReadU32(Address(R(s,31)+12u));
    R(s,29)=R(s,3);
    R(s,10)=R(s,11)&130u;
    Compare(s.cr0,R(s,10),0,s.xer_so,true);
    if (s.cr0.eq)
    {
        s.lr=0x82b8258cu; ErrorAddress(m,d,s);
        R(s,10)=9;
        goto set_error;
    }
    R(s,10)=WordRotateMask(R(s,11),0,0x40u);
    Compare(s.cr0,R(s,10),0,s.xer_so,true);
    if (!s.cr0.eq)
    {
        s.lr=0x82b825b8u; ErrorAddress(m,d,s);
        R(s,10)=34;
        goto set_error;
    }
    R(s,9)=R(s,11)&1u;
    R(s,10)=0;
    Compare(s.cr0,R(s,9),0,s.xer_so,true);
    if (!s.cr0.eq)
    {
        R(s,9)=WordRotateMask(R(s,11),0,0x10u);
        Compare(s.cr0,R(s,9),0,s.xer_so,true);
        m.WriteU32(Address(R(s,31)+4u),0);
        if (s.cr0.eq) goto mark_error;
        R(s,9)=m.ReadU32(Address(R(s,31)+8u));
        R(s,11)=WordRotateMask(R(s,11),0,0xfffffffeu);
        m.WriteU32(Address(R(s,31)),Address(R(s,9)));
        m.WriteU32(Address(R(s,31)+12u),Address(R(s,11)));
    }
    R(s,11)=m.ReadU32(Address(R(s,31)+12u));
    R(s,28)=0;
    m.WriteU32(Address(R(s,31)+4u),0);
    R(s,11)=WordRotateMask(R(s,11),0,0xffffffffffffffefull)|2u;
    m.WriteU32(Address(R(s,31)+12u),Address(R(s,11)));
    R(s,10)=R(s,11)&268u;
    Compare(s.cr0,R(s,10),0,s.xer_so,true);
    if (s.cr0.eq)
    {
        s.lr=0x82b82610u; Integer(0x822a03c8u,s);
        R(s,11)=R(s,3)+32u;
        Compare(s.cr6,R(s,31),R(s,11),s.xer_so,false);
        if (!s.cr6.eq)
        {
            s.lr=0x82b82620u; Integer(0x822a03c8u,s);
            R(s,11)=R(s,3)+64u;
            Compare(s.cr6,R(s,31),R(s,11),s.xer_so,false);
        }
        if (s.cr6.eq)
        {
            R(s,3)=R(s,29);
            s.lr=0x82b82634u; Integer(0x829664e8u,s);
            Compare(s.cr0,R(s,3),0,s.xer_so,true);
        }
        if (!s.cr6.eq || s.cr0.eq)
        {
            R(s,3)=R(s,31);
            s.lr=0x82b82644u;
            StreamState(0x82b85c40u,m,d,s);
        }
    }
    R(s,11)=m.ReadU32(Address(R(s,31)+12u));
    R(s,11)&=264u;
    Compare(s.cr0,R(s,11),0,s.xer_so,true);
    if (!s.cr0.eq)
    {
        R(s,11)=m.ReadU32(Address(R(s,31)+24u));
        R(s,4)=m.ReadU32(Address(R(s,31)+8u));
        --R(s,11);
        R(s,10)=m.ReadU32(Address(R(s,31)));
        R(s,30)=R(s,10)-R(s,4);
        Compare(s.cr0,R(s,30),0,s.xer_so,true);
        m.WriteU32(Address(R(s,31)+4u),Address(R(s,11)));
        R(s,11)=R(s,4)+1u;
        m.WriteU32(Address(R(s,31)),Address(R(s,11)));
        if (s.cr0.gt)
        {
            R(s,5)=R(s,30); R(s,3)=R(s,29);
            s.lr=0x82b82684u;
            Locked(m,d,s,false);
            R(s,28)=R(s,3);
        }
        else
        {
            Compare(s.cr6,R(s,29),UINT32_MAX,s.xer_so,true);
            bool special=s.cr6.eq;
            if (!special)
            {
                Compare(s.cr6,R(s,29),0xfffffffeu,s.xer_so,true);
                special=s.cr6.eq;
            }
            if (!special) R(s,11)=StreamRecord(m,s,R(s,29));
            else R(s,11)=0xffffffff83215318ull;
            R(s,11)=m.ReadU8(Address(R(s,11)+4u));
            R(s,11)=WordRotateMask(R(s,11),0,0x20u);
            Compare(s.cr0,R(s,11),0,s.xer_so,true);
            if (!s.cr0.eq)
            {
                R(s,5)=2; R(s,4)=0; R(s,3)=R(s,29);
                s.lr=0x82b826e0u;
                Locked(m,d,s,true);
                s.cr6={std::uint8_t(static_cast<std::int64_t>(R(s,3)) < -1),
                    std::uint8_t(static_cast<std::int64_t>(R(s,3)) > -1),
                    std::uint8_t(R(s,3)==UINT64_MAX),s.xer_so};
                if (s.cr6.eq) goto mark_error;
            }
        }
        R(s,11)=m.ReadU32(Address(R(s,31)+8u));
        R(s,10)=m.ReadU32(Address(s.sp+148u));
        m.WriteU8(Address(R(s,11)),static_cast<std::uint8_t>(R(s,10)));
    }
    else
    {
        R(s,5)=1; R(s,4)=s.sp+148u; R(s,3)=R(s,29);
        R(s,30)=1;
        s.lr=0x82b8270cu;
        Locked(m,d,s,false);
        R(s,28)=R(s,3);
    }
    Compare(s.cr6,R(s,28),R(s,30),s.xer_so,true);
    if (s.cr6.eq)
    {
        R(s,11)=m.ReadU32(Address(s.sp+148u));
        R(s,3)=R(s,11)&0xffu;
        goto done;
    }
mark_error:
    R(s,11)=m.ReadU32(Address(R(s,31)+12u));
    R(s,11)|=32u;
    m.WriteU32(Address(R(s,31)+12u),Address(R(s,11)));
    R(s,3)=UINT64_MAX;
    goto done;
set_error:
    R(s,11)=R(s,3);
    m.WriteU32(Address(R(s,11)),Address(R(s,10)));
    goto mark_error;
done:
    s.sp+=128u;
    Restore(m,s,28);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (address)
    {
    case 0x82b85ea8u: Seek(memory,dependencies,registers); return true;
    case 0x82b81b88u: Write(memory,dependencies,registers); return true;
    case 0x82b81de0u: Locked(memory,dependencies,registers,false); return true;
    case 0x82b85f70u: Locked(memory,dependencies,registers,true); return true;
    case 0x82b82558u: PutCharacter(memory,dependencies,registers); return true;
    default: return false;
    }
}

bool ApplyAcceptedCallee(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (address)
    {
    case 0x82b86228u:
    case 0x82b7fdb0u:
    case 0x82b7fde8u:
        StreamError(address,memory,dependencies,registers); return true;
    case 0x82b7fd78u:
        ErrorAddress(memory,dependencies,registers); return true;
    case 0x82b7fec0u:
        Invalid(memory,dependencies,registers); return true;
    case 0x82be2810u:
    case 0x82be2938u:
        StreamIo(address,memory,dependencies,registers); return true;
    case 0x822ca100u:
        LastError(memory,registers); return true;
    case 0x82b862f8u:
        Lock(memory,dependencies,registers); return true;
    case 0x82b81f38u:
    case 0x82b860ccu:
        Unlock(address,memory,dependencies,registers); return true;
    case 0x82b81648u:
    case 0x82b85c40u:
        StreamState(address,memory,dependencies,registers); return true;
    case 0x822a03c8u:
    case 0x829664e8u:
        Integer(address,registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_operations
