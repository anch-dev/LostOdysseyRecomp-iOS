#include "lo_semantics/legacy_descriptor_scalar_intern_callers.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>

namespace lo::semantic::gpu::legacy_descriptor_scalar_intern_callers
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
std::uint64_t& R(Registers& s,unsigned i)
{return i==1u?s.integer.sp:s.integer.r[i];}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t a)
{R(s,i)=m.ReadU32(Address(a));}
void Store(GuestMemory& m,std::uint64_t a,std::uint64_t x)
{m.WriteU32(Address(a),Address(x));}
double F(std::uint64_t x){return std::bit_cast<double>(x);}
std::uint64_t Bits(double x){return std::bit_cast<std::uint64_t>(x);}
void DisableFlush(Dependencies d,Registers& s)
{
    if(s.cached_fp_control&0x8040u)
    {s.cached_fp_control&=~0x8040u;d.lookup.allocation.SetHostFpControl(s.cached_fp_control);}
}
std::uint64_t Widen(std::uint32_t word)
{volatile float x=std::bit_cast<float>(word);return Bits(double(x));}
void StoreFloat(GuestMemory& m,std::uint64_t a,std::uint64_t x)
{m.WriteU32(Address(a),std::bit_cast<std::uint32_t>(float(F(x))));}
void Compare(Registers& s,std::uint64_t a,std::uint32_t b)
{
    const auto x=Address(a);
    s.integer.cr6={std::uint8_t(x<b),std::uint8_t(x>b),std::uint8_t(x==b),s.integer.xer_so};
}
void Save(GuestMemory& m,Registers& s,unsigned frame,bool save31=false)
{
    R(s,12)=s.integer.lr;Store(m,R(s,1)-8u,R(s,12));
    if(save31)WriteU64(m,Address(R(s,1)-16u),R(s,31));
    const auto incoming=R(s,1);R(s,1)-=frame;Store(m,R(s,1),incoming);
}
void Restore(GuestMemory& m,Registers& s,unsigned frame,bool save31=false)
{
    R(s,1)+=frame;Load(m,s,12u,R(s,1)-8u);s.integer.lr=R(s,12);
    if(save31)R(s,31)=ReadU64(m,Address(R(s,1)-16u));
}
void Lookup(GuestMemory& m,Dependencies d,Registers& s)
{(void)legacy_descriptor_array_lookup::Apply(0x83058ad8u,m,d.lookup,s);}
void ScalarWord(GuestMemory& m,Dependencies d,Registers& s)
{
    Save(m,s,128u);Store(m,R(s,1)+156u,R(s,4));
    R(s,10)=std::uint64_t(std::int64_t{-2113929216});
    R(s,9)=Address(R(s,5))&0xffu;R(s,11)=R(s,1)+80u;R(s,8)=0u;
    R(s,5)=R(s,1)+96u;R(s,4)=1u;Store(m,R(s,11),R(s,8));
    Load(m,s,11u,R(s,1)+80u);DisableFlush(d,s);
    s.f0_bits=Widen(m.ReadU32(Address(R(s,1)+156u)));
    StoreFloat(m,R(s,1)+96u,s.f0_bits);
    s.f0_bits=Widen(m.ReadU32(Address(R(s,10)+3664u)));
    R(s,10)=std::countl_zero(Address(R(s,9)));
    StoreFloat(m,R(s,1)+100u,s.f0_bits);
    R(s,10)=WordRotateMask(R(s,10),27,1u);
    StoreFloat(m,R(s,1)+104u,s.f0_bits);StoreFloat(m,R(s,1)+108u,s.f0_bits);
    R(s,10)+=1u;R(s,6)=R(s,10)|R(s,11);s.integer.lr=0x8305a70cu;
    Lookup(m,d,s);Restore(m,s,128u);
}
void OutputRecord(GuestMemory& m,Dependencies d,Registers& s)
{
    Save(m,s,96u,true);R(s,31)=R(s,3);R(s,3)=R(s,4);R(s,4)=R(s,5);R(s,5)=R(s,6);
    s.integer.lr=0x8305a744u;ScalarWord(m,d,s);Load(m,s,11u,R(s,31)+4u);
    R(s,10)=1u;R(s,9)=R(s,3);
    R(s,11)=WordRotateMask(R(s,10),0,0xfffffffffffeffffull)|(R(s,11)&0x10000u);
    R(s,3)=R(s,31);Store(m,R(s,31),R(s,9));Store(m,R(s,31)+4u,R(s,11));
    Restore(m,s,96u,true);
}
void Vector(GuestMemory& m,Dependencies d,Registers& s)
{
    Save(m,s,128u);R(s,11)=R(s,1)+80u;DisableFlush(d,s);
    StoreFloat(m,R(s,1)+96u,s.f1_bits);R(s,10)=0u;
    StoreFloat(m,R(s,1)+100u,s.f2_bits);R(s,5)=R(s,1)+96u;
    StoreFloat(m,R(s,1)+104u,s.f3_bits);StoreFloat(m,R(s,1)+108u,s.f4_bits);
    Store(m,R(s,11),R(s,10));Load(m,s,6u,R(s,1)+80u);s.integer.lr=0x8305a7acu;
    Lookup(m,d,s);Restore(m,s,128u);
}
template<class T>std::uint64_t Convert(double value)
{
    constexpr T Maximum=std::numeric_limits<T>::max(),Minimum=std::numeric_limits<T>::min();
    if(value>double(Maximum))return std::uint64_t(std::int64_t(Maximum));
    const auto x=std::trunc(value),upper=std::ldexp(1.0,sizeof(T)*8u-1u);
    if(!std::isfinite(x)||x<double(Minimum)||x>=upper)
    {std::feraiseexcept(FE_INVALID);return std::uint64_t(std::int64_t(Minimum));}
    return std::uint64_t(std::int64_t(static_cast<T>(x)));
}
void Typed(GuestMemory& m,Dependencies d,Registers& s)
{
    Save(m,s,128u);Compare(s,R(s,5),1u);
    if(!s.integer.cr6.lt)
    {
        const bool word=s.integer.cr6.eq!=0;
        if(!word)
        {
            Compare(s,R(s,5),3u);
            if(!s.integer.cr6.lt){R(s,4)=4800u;s.integer.lr=0x8305a7e8u;d.diagnostic.Call(m,s);}
        }
        R(s,11)=R(s,1)+80u;DisableFlush(d,s);
        s.f0_bits=word?Convert<std::int32_t>(F(s.f1_bits)):Convert<std::int64_t>(F(s.f1_bits));
        R(s,5)=word?1u:0u;Store(m,R(s,11),s.f0_bits);Load(m,s,4u,R(s,1)+80u);
        s.integer.lr=0x8305a818u;ScalarWord(m,d,s);
    }
    else
    {
        R(s,11)=R(s,1)+80u;DisableFlush(d,s);s.f0_bits=Bits(double(float(F(s.f1_bits))));
        R(s,9)=0u;StoreFloat(m,R(s,1)+96u,s.f0_bits);
        R(s,10)=std::uint64_t(std::int64_t{-2113929216});R(s,5)=R(s,1)+96u;R(s,4)=1u;
        Store(m,R(s,11),R(s,9));s.f0_bits=Widen(m.ReadU32(Address(R(s,10)+3664u)));
        Load(m,s,6u,R(s,1)+80u);StoreFloat(m,R(s,1)+100u,s.f0_bits);
        StoreFloat(m,R(s,1)+104u,s.f0_bits);StoreFloat(m,R(s,1)+108u,s.f0_bits);
        s.integer.lr=0x8305a854u;Lookup(m,d,s);
    }
    Restore(m,s,128u);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies d,Registers& s)
{
    switch(entry)
    {
    case 0x8305a6b0u:ScalarWord(memory,d,s);return true;
    case 0x8305a720u:OutputRecord(memory,d,s);return true;
    case 0x8305a778u:Vector(memory,d,s);return true;
    case 0x8305a7c0u:Typed(memory,d,s);return true;
    default:return false;
    }
}
}
