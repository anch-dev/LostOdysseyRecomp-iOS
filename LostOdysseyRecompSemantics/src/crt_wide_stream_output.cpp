#include "lo_semantics/crt_wide_stream_output.h"

#include "lo_semantics/crt_code_unit_conversion.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_wide_stream_output
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void CompareWord(Registers& s, Condition& cr, std::uint64_t left,
    std::uint64_t right, bool signed_compare = true)
{
    if (signed_compare)
    {
        const auto a = static_cast<std::int32_t>(left);
        const auto b = static_cast<std::int32_t>(right);
        cr = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), s.xer_so};
    }
    else
    {
        const auto a = static_cast<std::uint32_t>(left);
        const auto b = static_cast<std::uint32_t>(right);
        cr = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), s.xer_so};
    }
}

void CompareDouble(Registers& s, Condition& cr, std::uint64_t left,
    std::int64_t right)
{
    const auto a = static_cast<std::int64_t>(left);
    cr = {std::uint8_t(a < right), std::uint8_t(a > right),
        std::uint8_t(a == right), s.xer_so};
}

void Save27(GuestMemory& m, Registers& s, std::uint64_t return_address)
{
    R(s, 12) = s.lr;
    s.lr = return_address;
    for (unsigned i = 27; i <= 31; ++i)
        WriteU64(m, Address(s.sp - 16u - 8u * (31u - i)), R(s, i));
    m.WriteU32(Address(s.sp - 8u), Address(R(s, 12)));
    m.WriteU32(Address(s.sp - 144u), Address(s.sp));
    s.sp -= 144u;
}

void Restore27(GuestMemory& m, Registers& s)
{
    s.sp += 144u;
    for (unsigned i = 27; i <= 31; ++i)
        R(s, i) = ReadU64(m, Address(s.sp - 16u - 8u * (31u - i)));
    R(s, 12) = m.ReadU32(Address(s.sp - 8u));
    s.lr = R(s, 12);
}

void CallStream(GuestAddress entry, std::uint64_t return_address,
    GuestMemory& m, Dependencies d, Registers& s)
{
    s.lr = return_address;
    if (!crt_stream_operations::Apply(entry, m, d.streams, s) &&
        !crt_stream_operations::ApplyAcceptedCallee(entry, m, d.streams, s))
        throw std::logic_error("missing accepted stream callee");
}

void CallConversion(GuestMemory& m, Dependencies d, Registers& s)
{
    crt_code_unit_conversion::Registers lower{};
    lower.sp=s.sp; lower.lr=s.lr;
    for (unsigned i=3; i<=13; ++i) lower.r[i]=R(s,i);
    lower.xer_so=s.xer_so;
    lower.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,s.cr0.so};
    lower.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,s.cr6.so};
    if (!crt_code_unit_conversion::Apply(0x82b86be0u,m,
        {d.streams.thread,d.streams.invalid},lower))
        throw std::logic_error("missing accepted code-unit converter");
    s.sp=lower.sp;s.lr=lower.lr;
    for (unsigned i=3; i<=13; ++i) R(s,i)=lower.r[i];
    s.xer_so=lower.xer_so;
    s.cr0={lower.cr0.lt,lower.cr0.gt,lower.cr0.eq,lower.cr0.so};
    s.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}

void CallErrorOutput(GuestMemory& m, Dependencies d, Registers& s)
{
    crt_formatting_support::Registers lower{};
    lower.sp=s.sp;lower.lr=s.lr;
    lower.r3=R(s,3);lower.r4=R(s,4);lower.r5=R(s,5);
    lower.r6=R(s,6);lower.r7=R(s,7);lower.r8=R(s,8);
    lower.r9=R(s,9);lower.r10=R(s,10);lower.r11=R(s,11);
    lower.r12=R(s,12);lower.r13=R(s,13);lower.r31=R(s,31);
    lower.xer_so=s.xer_so;
    lower.cr0={s.cr0.lt,s.cr0.gt,s.cr0.eq,s.cr0.so};
    lower.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,s.cr6.so};
    if (!crt_formatting_support::Apply(0x82be4700u,m,d.error_output,lower))
        throw std::logic_error("missing accepted Unicode error output");
    s.sp=lower.sp;s.lr=lower.lr;
    R(s,3)=lower.r3;R(s,4)=lower.r4;R(s,5)=lower.r5;
    R(s,6)=lower.r6;R(s,7)=lower.r7;R(s,8)=lower.r8;
    R(s,9)=lower.r9;R(s,10)=lower.r10;R(s,11)=lower.r11;
    R(s,12)=lower.r12;R(s,13)=lower.r13;R(s,31)=lower.r31;
    s.xer_so=lower.xer_so;
    s.cr0={lower.cr0.lt,lower.cr0.gt,lower.cr0.eq,lower.cr0.so};
    s.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}

void MarkStreamError(GuestMemory& m, Registers& s, bool load_flags)
{
    if (load_flags) R(s,11)=m.ReadU32(Address(R(s,31)+12u));
    R(s,11)|=32u;
    R(s,3)=0xffffu;
    m.WriteU32(Address(R(s,31)+12u),Address(R(s,11)));
}

void SetErrno(GuestMemory& m, Dependencies d, Registers& s,
    std::uint32_t code, std::uint64_t callsite)
{
    CallStream(0x82b7fd78u,callsite,m,d,s);
    R(s,10)=code;
    R(s,11)=R(s,3);
    m.WriteU32(Address(R(s,11)),code);
    MarkStreamError(m,s,true);
}

// A wide buffer flush either commits pending bytes or writes this code unit
// directly. The signed pointer difference and buffer reloads are observable.
void WriteWide(GuestMemory& m, Dependencies d, Registers& s)
{
    Save27(m,s,0x82b87870u);
    R(s,31)=R(s,4);
    R(s,27)=R(s,3);
    R(s,3)=R(s,31);
    CallStream(0x82b81648u,0x82b87884u,m,d,s);
    R(s,11)=m.ReadU32(Address(R(s,31)+12u));
    R(s,30)=R(s,3);
    R(s,10)=R(s,11)&130u;
    CompareWord(s,s.cr0,R(s,10),0);
    if (s.cr0.eq)
        SetErrno(m,d,s,9u,0x82b8789cu);
    else
    {
        R(s,10)=WordRotateMask(R(s,11),0,0x40u);
        CompareWord(s,s.cr0,R(s,10),0);
        if (!s.cr0.eq)
            SetErrno(m,d,s,34u,0x82b878ccu);
        else
        {
            R(s,9)=static_cast<std::uint32_t>(R(s,11))&1u;
            CompareWord(s,s.cr0,R(s,9),0);
            R(s,10)=0;
            bool bad_update=false;
            if (!s.cr0.eq)
            {
                R(s,9)=WordRotateMask(R(s,11),0,0x10u);
                CompareWord(s,s.cr0,R(s,9),0);
                m.WriteU32(Address(R(s,31)+4u),0);
                if (s.cr0.eq) bad_update=true;
                else
                {
                    R(s,9)=m.ReadU32(Address(R(s,31)+8u));
                    R(s,11)=WordRotateMask(R(s,11),0,0xfffffffeu);
                    m.WriteU32(Address(R(s,31)),Address(R(s,9)));
                    m.WriteU32(Address(R(s,31)+12u),Address(R(s,11)));
                }
            }
            if (bad_update) MarkStreamError(m,s,false);
            else
            {
                bool seek_failed=false;
                R(s,11)=m.ReadU32(Address(R(s,31)+12u));
                R(s,28)=R(s,10);
                m.WriteU32(Address(R(s,31)+4u),0);
                R(s,11)=WordRotateMask(R(s,11),0,0xffffffffffffffefull)|2u;
                m.WriteU32(Address(R(s,31)+12u),Address(R(s,11)));
                R(s,10)=R(s,11)&268u;
                CompareWord(s,s.cr0,R(s,10),0);
                if (s.cr0.eq)
                {
                    CallStream(0x822a03c8u,0x82b87924u,m,d,s);
                    R(s,11)=R(s,3)+32u;
                    CompareWord(s,s.cr6,R(s,31),R(s,11),false);
                    bool standard=s.cr6.eq!=0;
                    if (!standard)
                    {
                        CallStream(0x822a03c8u,0x82b87934u,m,d,s);
                        R(s,11)=R(s,3)+64u;
                        CompareWord(s,s.cr6,R(s,31),R(s,11),false);
                        standard=s.cr6.eq!=0;
                    }
                    if (standard)
                    {
                        R(s,3)=R(s,30);
                        CallStream(0x829664e8u,0x82b87948u,m,d,s);
                        CompareWord(s,s.cr0,R(s,3),0);
                    }
                    if (!standard || s.cr0.eq)
                    {
                        R(s,3)=R(s,31);
                        CallStream(0x82b85c40u,0x82b87958u,m,d,s);
                    }
                }
                R(s,11)=m.ReadU32(Address(R(s,31)+12u));
                R(s,11)&=264u;
                CompareWord(s,s.cr0,R(s,11),0);
                if (s.cr0.eq)
                {
                    R(s,5)=2;
                    m.WriteU16(Address(s.sp+80u),static_cast<std::uint16_t>(R(s,27)));
                    R(s,4)=s.sp+80u;R(s,3)=R(s,30);R(s,29)=2;
                    CallStream(0x82b81de0u,0x82b87a20u,m,d,s);
                    R(s,28)=R(s,3);
                }
                else
                {
                    R(s,11)=m.ReadU32(Address(R(s,31)+24u));
                    R(s,4)=m.ReadU32(Address(R(s,31)+8u));
                    R(s,11)-=2u;
                    R(s,10)=m.ReadU32(Address(R(s,31)));
                    R(s,29)=R(s,10)-R(s,4);
                    CompareWord(s,s.cr0,R(s,29),0);
                    m.WriteU32(Address(R(s,31)+4u),Address(R(s,11)));
                    R(s,11)=R(s,4)+2u;
                    m.WriteU32(Address(R(s,31)),Address(R(s,11)));
                    if (s.cr0.gt)
                    {
                        R(s,5)=R(s,29);R(s,3)=R(s,30);
                        CallStream(0x82b81de0u,0x82b87998u,m,d,s);
                        R(s,28)=R(s,3);
                    }
                    else
                    {
                        CompareWord(s,s.cr6,R(s,30),static_cast<std::uint64_t>(-1));
                        bool special=s.cr6.eq!=0;
                        if (!special)
                        {
                            CompareWord(s,s.cr6,R(s,30),static_cast<std::uint64_t>(-2));
                            special=s.cr6.eq!=0;
                        }
                        if (special) R(s,11)=0xffffffff83215318ull;
                        else
                        {
                            const auto h=static_cast<std::int32_t>(R(s,30));
                            s.xer_ca=std::uint8_t(h<0 &&
                                (static_cast<std::uint32_t>(h)&31u)!=0);
                            R(s,10)=static_cast<std::uint64_t>(static_cast<std::int64_t>(h>>5));
                            R(s,11)=0xffffffff83380000ull;
                            R(s,9)=WordRotateMask(R(s,10),2,0xfffffffcu);
                            R(s,11)-=29312u;
                            R(s,10)=WordRotateMask(R(s,30),6,0x7c0u);
                            R(s,11)=m.ReadU32(Address(R(s,9)+R(s,11)));
                            R(s,11)+=R(s,10);
                        }
                        R(s,11)=m.ReadU8(Address(R(s,11)+4u));
                        R(s,11)=WordRotateMask(R(s,11),0,0x20u);
                        CompareWord(s,s.cr0,R(s,11),0);
                        if (!s.cr0.eq)
                        {
                            R(s,5)=2;R(s,4)=0;R(s,3)=R(s,30);
                            CallStream(0x82b85f70u,0x82b879f4u,m,d,s);
                            CompareDouble(s,s.cr6,R(s,3),-1);
                            if (s.cr6.eq)
                            {
                                MarkStreamError(m,s,true);
                                seek_failed=true;
                            }
                        }
                    }
                    if (!seek_failed)
                    {
                        R(s,11)=m.ReadU32(Address(R(s,31)+8u));
                        m.WriteU16(Address(R(s,11)),
                            static_cast<std::uint16_t>(R(s,27)));
                    }
                }
                if (!seek_failed)
                {
                    CompareWord(s,s.cr6,R(s,28),R(s,29));
                    if (!s.cr6.eq) MarkStreamError(m,s,false);
                    else R(s,3)=static_cast<std::uint32_t>(R(s,27))&0xffffu;
                }
            }
        }
    }
    Restore27(m,s);
}

void ReadHandle(GuestMemory& m, Dependencies d, Registers& s,
    std::uint64_t return_address)
{
    R(s,3)=R(s,31);
    CallStream(0x82b81648u,return_address,m,d,s);
}

// The descriptor address is deliberately recomputed on each original branch:
// GetStreamHandle may be observable and each call has a distinct LR.
void SelectDescriptor(GuestMemory& m, Dependencies d, Registers& s,
    std::uint64_t first_return)
{
    ReadHandle(m,d,s,first_return);
    CompareWord(s,s.cr6,R(s,3),static_cast<std::uint64_t>(-1));
    if (!s.cr6.eq)
    {
        ReadHandle(m,d,s,first_return+16u);
        CompareWord(s,s.cr6,R(s,3),static_cast<std::uint64_t>(-2));
    }
    if (s.cr6.eq) R(s,11)=R(s,28);
    else
    {
        ReadHandle(m,d,s,first_return+32u);
        const auto handle=static_cast<std::int32_t>(R(s,3));
        s.xer_ca=std::uint8_t(handle<0 &&
            (static_cast<std::uint32_t>(handle)&31u)!=0);
        R(s,11)=static_cast<std::uint64_t>(static_cast<std::int64_t>(handle>>5));
        R(s,3)=R(s,31);
        R(s,29)=WordRotateMask(R(s,11),2,0xfffffffcu);
        CallStream(0x82b81648u,first_return+48u,m,d,s);
        R(s,10)=m.ReadU32(Address(R(s,29)+R(s,30)));
        R(s,11)=WordRotateMask(R(s,3),6,0x7c0u);
        R(s,11)+=R(s,10);
    }
}

bool BufferedByte(GuestMemory& m, Dependencies d, Registers& s,
    std::uint8_t value, std::uint64_t return_address)
{
    R(s,11)=m.ReadU32(Address(R(s,31)+4u));
    s.xer_ca=std::uint8_t(static_cast<std::uint32_t>(R(s,11))>0u);
    R(s,11)-=1u;
    CompareWord(s,s.cr0,R(s,11),0);
    m.WriteU32(Address(R(s,31)+4u),Address(R(s,11)));
    if (!s.cr0.lt)
    {
        R(s,11)=m.ReadU32(Address(R(s,31)));
        R(s,10)=value;
        m.WriteU8(Address(R(s,11)),value);
        R(s,11)=m.ReadU32(Address(R(s,31)));
        R(s,10)=R(s,11)+1u;
        R(s,3)=m.ReadU8(Address(R(s,11)));
        m.WriteU32(Address(R(s,31)),Address(R(s,10)));
    }
    else
    {
        R(s,11)=value;
        R(s,4)=R(s,31);
        R(s,3)=static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int8_t>(value)));
        CallStream(0x82b82558u,return_address,m,d,s);
    }
    CompareWord(s,s.cr6,R(s,3),static_cast<std::uint64_t>(-1));
    return !s.cr6.eq;
}

// Internal family edge, kept separate from the accepted stream dependencies.
void WriteCodeUnit(GuestMemory& m, Dependencies d, Registers& s)
{
    Save27(m,s,0x822a03e0u);
    R(s,27)=R(s,3);R(s,31)=R(s,4);
    m.WriteU16(Address(s.sp+166u),static_cast<std::uint16_t>(R(s,27)));
    CallStream(0x822a03c8u,0x822a03f4u,m,d,s);
    R(s,11)=R(s,3)+32u;
    CompareWord(s,s.cr6,R(s,31),R(s,11),false);
    bool reserved=s.cr6.eq!=0;
    if (!reserved)
    {
        CallStream(0x822a03c8u,0x822a0404u,m,d,s);
        R(s,11)=R(s,3)+64u;
        CompareWord(s,s.cr6,R(s,31),R(s,11),false);
        reserved=s.cr6.eq!=0;
    }
    bool failure=false, wide=false, fallback_wide=false;
    if (reserved)
    {
        R(s,11)=0;
        m.WriteU16(Address(s.sp+80u),static_cast<std::uint16_t>(R(s,27)));
        R(s,3)=s.sp+80u;
        m.WriteU16(Address(s.sp+82u),0);
        s.lr=0x822a06b0u;
        CallErrorOutput(m,d,s);
    }
    else
    {
        R(s,11)=m.ReadU32(Address(R(s,31)+12u));
        R(s,11)=WordRotateMask(R(s,11),0,0x40u);
        CompareWord(s,s.cr0,R(s,11),0);
        wide=!s.cr0.eq;
        if (!wide)
        {
            ReadHandle(m,d,s,0x822a0424u);
            R(s,11)=0xffffffff83380000ull;
            CompareWord(s,s.cr6,R(s,3),static_cast<std::uint64_t>(-1));
            R(s,30)=R(s,11)-29312u;
            R(s,11)=0xffffffff83210000ull;
            R(s,28)=R(s,11)+21272u;
            if (!s.cr6.eq)
            {
                ReadHandle(m,d,s,0x822a0444u);
                CompareWord(s,s.cr6,R(s,3),static_cast<std::uint64_t>(-2));
            }
            if (s.cr6.eq) R(s,11)=R(s,28);
            else
            {
                ReadHandle(m,d,s,0x822a0454u);
                const auto handle=static_cast<std::int32_t>(R(s,3));
                s.xer_ca=std::uint8_t(handle<0 &&
                    (static_cast<std::uint32_t>(handle)&31u)!=0);
                R(s,11)=static_cast<std::uint64_t>(static_cast<std::int64_t>(handle>>5));
                R(s,3)=R(s,31);
                R(s,29)=WordRotateMask(R(s,11),2,0xfffffffcu);
                CallStream(0x82b81648u,0x822a0464u,m,d,s);
                R(s,10)=m.ReadU32(Address(R(s,29)+R(s,30)));
                R(s,11)=WordRotateMask(R(s,3),6,0x7c0u);
                R(s,11)+=R(s,10);
            }
            R(s,11)=m.ReadU8(Address(R(s,11)+40u));
            R(s,11)=WordRotateMask(R(s,11),0,0xfffffffeu);
            CompareWord(s,s.cr6,R(s,11),4u,false);
            wide=s.cr6.eq!=0;
            if (!wide)
            {
                SelectDescriptor(m,d,s,0x822a04ccu);
                R(s,11)=m.ReadU8(Address(R(s,11)+40u));
                R(s,11)=WordRotateMask(R(s,11),0,0xfffffffeu);
                CompareWord(s,s.cr6,R(s,11),2u,false);
                if (s.cr6.eq)
                {
                    if (!BufferedByte(m,d,s,m.ReadU8(Address(s.sp+166u)),
                            0x822a0560u) ||
                        !BufferedByte(m,d,s,m.ReadU8(Address(s.sp+167u)),
                            0x822a05b0u))
                        failure=true;
                }
                else
                {
                    SelectDescriptor(m,d,s,0x822a05c4u);
                    R(s,11)=m.ReadU8(Address(R(s,11)+4u));
                    R(s,11)=WordRotateMask(R(s,11),0,0xffffff80u);
                    CompareWord(s,s.cr0,R(s,11),0);
                    wide=s.cr0.eq!=0;
                    if (!wide)
                    {
                        R(s,6)=R(s,27);R(s,5)=5;
                        R(s,4)=s.sp+84u;R(s,3)=s.sp+80u;
                        s.lr=0x822a0628u;
                        CallConversion(m,d,s);
                        CompareWord(s,s.cr0,R(s,3),0);
                        if (!s.cr0.eq) failure=true;
                        else
                        {
                            R(s,11)=m.ReadU32(Address(s.sp+80u));
                            R(s,30)=0;
                            CompareWord(s,s.cr6,R(s,11),0);
                            while (s.cr6.gt && !failure)
                            {
                                R(s,11)=m.ReadU32(Address(R(s,31)+4u));
                                s.xer_ca=std::uint8_t(static_cast<std::uint32_t>(R(s,11))>0u);
                                R(s,11)-=1u;
                                CompareWord(s,s.cr0,R(s,11),0);
                                m.WriteU32(Address(R(s,31)+4u),Address(R(s,11)));
                                R(s,11)=s.sp+84u;
                                R(s,11)=m.ReadU8(Address(R(s,30)+R(s,11)));
                                if (!s.cr0.lt)
                                {
                                    R(s,10)=m.ReadU32(Address(R(s,31)));
                                    m.WriteU8(Address(R(s,10)),static_cast<std::uint8_t>(R(s,11)));
                                    R(s,11)=m.ReadU32(Address(R(s,31)));
                                    R(s,10)=R(s,11)+1u;
                                    R(s,3)=m.ReadU8(Address(R(s,11)));
                                    m.WriteU32(Address(R(s,31)),Address(R(s,10)));
                                }
                                else
                                {
                                    R(s,4)=R(s,31);
                                    R(s,3)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
                                        static_cast<std::int8_t>(R(s,11))));
                                    CallStream(0x82b82558u,0x822a0680u,m,d,s);
                                }
                                CompareWord(s,s.cr6,R(s,3),static_cast<std::uint64_t>(-1));
                                if (s.cr6.eq) failure=true;
                                else
                                {
                                    R(s,11)=m.ReadU32(Address(s.sp+80u));
                                    ++R(s,30);
                                    CompareWord(s,s.cr6,R(s,30),R(s,11));
                                }
                            }
                        }
                    }
                }
            }
        }
        if (wide && !failure)
        {
            R(s,11)=m.ReadU32(Address(R(s,31)+4u));
            s.xer_ca=std::uint8_t(static_cast<std::uint32_t>(R(s,11))>1u);
            R(s,11)-=2u;
            CompareWord(s,s.cr0,R(s,11),0);
            m.WriteU32(Address(R(s,31)+4u),Address(R(s,11)));
            if (!s.cr0.lt)
            {
                R(s,11)=m.ReadU32(Address(R(s,31)));
                m.WriteU16(Address(R(s,11)),static_cast<std::uint16_t>(R(s,27)));
                R(s,11)=m.ReadU32(Address(R(s,31)));
                R(s,11)+=2u;
                m.WriteU32(Address(R(s,31)),Address(R(s,11)));
            }
            else
            {
                R(s,4)=R(s,31);
                R(s,3)=static_cast<std::uint32_t>(R(s,27))&0xffffu;
                s.lr=0x822a04bcu;
                WriteWide(m,d,s);
                R(s,3)=static_cast<std::uint32_t>(R(s,3))&0xffffu;
                fallback_wide=true; // Bypasses the common r3=r27 epilogue.
            }
        }
    }
    if (failure) R(s,3)=static_cast<std::uint64_t>(-1);
    else if (!fallback_wide) R(s,3)=R(s,27);
    Restore27(m,s);
}

void CountCodeUnit(GuestMemory& m, Dependencies d, Registers& s)
{
    R(s,12)=s.lr;
    m.WriteU32(Address(s.sp-8u),Address(R(s,12)));
    WriteU64(m,Address(s.sp-16u),R(s,31));
    m.WriteU32(Address(s.sp-96u),Address(s.sp));
    s.sp-=96u;
    R(s,11)=m.ReadU32(Address(R(s,4)+12u));
    R(s,31)=R(s,5);
    R(s,11)=WordRotateMask(R(s,11),0,0x40u);
    CompareWord(s,s.cr0,R(s,11),0);
    bool output=s.cr0.eq!=0;
    if (!output)
    {
        R(s,11)=m.ReadU32(Address(R(s,4)+8u));
        CompareWord(s,s.cr6,R(s,11),0,false);
        output=!s.cr6.eq;
    }
    bool failed=false;
    if (output)
    {
        s.lr=0x82b84a38u;
        WriteCodeUnit(m,d,s);
        R(s,11)=static_cast<std::uint32_t>(R(s,3))&0xffffu;
        CompareWord(s,s.cr6,R(s,11),0xffffu,false);
        failed=s.cr6.eq!=0;
    }
    if (failed) R(s,11)=static_cast<std::uint64_t>(-1);
    else
    {
        R(s,11)=m.ReadU32(Address(R(s,31)));
        ++R(s,11);
    }
    m.WriteU32(Address(R(s,31)),Address(R(s,11)));
    s.sp+=96u;
    R(s,12)=m.ReadU32(Address(s.sp-8u));
    s.lr=R(s,12);
    R(s,31)=ReadU64(m,Address(s.sp-16u));
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (entry)
    {
    case 0x82b87868u: WriteWide(memory,dependencies,registers);return true;
    case 0x822a03d8u: WriteCodeUnit(memory,dependencies,registers);return true;
    case 0x82b84a08u: CountCodeUnit(memory,dependencies,registers);return true;
    default:return false;
    }
}

bool ApplyAcceptedCallee(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (Apply(entry,memory,dependencies,registers)) return true;
    if (crt_stream_operations::Apply(entry,memory,dependencies.streams,registers) ||
        crt_stream_operations::ApplyAcceptedCallee(entry,memory,
            dependencies.streams,registers)) return true;
    if (entry==0x82b86be0u)
    {CallConversion(memory,dependencies,registers);return true;}
    if (entry==0x82be4700u)
    {CallErrorOutput(memory,dependencies,registers);return true;}
    return false;
}
} // namespace lo::semantic::gpu::crt_wide_stream_output
