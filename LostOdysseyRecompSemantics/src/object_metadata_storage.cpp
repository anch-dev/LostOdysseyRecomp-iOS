#include "lo_semantics/object_metadata_storage.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::object_metadata_storage
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
std::int32_t S(std::uint64_t value){return std::bit_cast<std::int32_t>(Address(value));}
void Compare(Registers& s,std::uint64_t a,std::uint64_t b,bool sign=false)
{
 if(sign)s.cr6={std::uint8_t(S(a)<S(b)),std::uint8_t(S(a)>S(b)),std::uint8_t(S(a)==S(b)),s.xer_so};
 else s.cr6={std::uint8_t(Address(a)<Address(b)),std::uint8_t(Address(a)>Address(b)),std::uint8_t(Address(a)==Address(b)),s.xer_so};
}
void Save(GuestMemory& m,Registers& s,unsigned first,std::uint64_t size)
{
 s.r[12]=s.lr;
 if(first!=27)m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
 for(unsigned i=first;i<=31;++i)WriteU64(m,Address(s.r[1]-8u*(33u-i)),s.r[i]);
 if(first==27)m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
 m.WriteU32(Address(s.r[1]-size),Address(s.r[1]));s.r[1]-=size;
}
void Restore(GuestMemory& m,Registers& s,unsigned first,std::uint64_t size)
{
 s.r[1]+=size;
 if(first==27)for(unsigned i=first;i<=31;++i)s.r[i]=ReadU64(m,Address(s.r[1]-8u*(33u-i)));
 s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];
 if(first!=27)for(unsigned i=first;i<=31;++i)s.r[i]=ReadU64(m,Address(s.r[1]-8u*(33u-i)));
}
void Reallocate(GuestMemory& m,PpcBoundaryServices& services,Registers& s)
{
 Save(m,s,27,128);
 s.r[31]=s.r[3];s.r[27]=s.r[5];s.r[28]=m.ReadU32(Address(s.r[31]));Compare(s,s.r[28],0);
 bool needed=!s.cr6.eq;
 if(!needed){s.r[11]=m.ReadU32(Address(s.r[31]+8u));Compare(s,s.r[11],0,true);needed=!s.cr6.eq;}
 if(needed)
 {
  s.r[30]=0xffffffff83310000ull;s.r[11]=m.ReadU32(Address(s.r[31]+8u));
  s.r[29]=static_cast<std::uint64_t>(std::int64_t(S(s.r[11]))*std::int64_t(S(s.r[4])));
  s.r[3]=m.ReadU32(Address(s.r[30]-18936u));Compare(s,s.r[3],0);
  if(s.cr6.eq){s.lr=0x8229f6c0u;services.InitializeManager827C5F38(m,s);s.r[3]=m.ReadU32(Address(s.r[30]-18936u));}
  s.r[11]=m.ReadU32(Address(s.r[3]));s.r[6]=s.r[27];s.r[5]=s.r[29];s.r[4]=s.r[28];
  s.r[11]=m.ReadU32(Address(s.r[11]+8u));s.ctr=s.r[11];s.lr=0x8229f6e0u;
  services.CallAllocator(Address(s.ctr)&~3u,m,s);m.WriteU32(Address(s.r[31]),Address(s.r[3]));
 }
 Restore(m,s,27,128);
}
void Grow(GuestMemory& m,PpcBoundaryServices& services,Registers& s)
{
 Save(m,s,31,96);s.r[31]=m.ReadU32(Address(s.r[3]+4u));s.r[9]=s.r[5];s.r[10]=m.ReadU32(Address(s.r[3]+8u));
 s.r[11]=s.r[31]+s.r[4];Compare(s,s.r[11],s.r[10],true);m.WriteU32(Address(s.r[3]+4u),Address(s.r[11]));
 if(s.cr6.gt)
 {
  s.r[10]=WordRotateMask(s.r[11],1,0xfffffffeu);s.r[5]=s.r[6];s.r[10]=s.r[11]+s.r[10];s.r[4]=s.r[9];
  const auto before=S(s.r[10]);s.xer_ca=std::uint8_t(before<0 && (Address(s.r[10])&7u)!=0u);
  s.r[10]=static_cast<std::uint64_t>(std::int64_t(before>>3));
  const auto old=s.r[10],result=old+s.xer_ca;s.xer_ca=std::uint8_t(Address(result)<Address(old));s.r[10]=result;
  s.r[11]=s.r[10]+s.r[11];s.r[11]+=32u;m.WriteU32(Address(s.r[3]+8u),Address(s.r[11]));
  s.lr=0x822c432cu;Reallocate(m,services,s);
 }
 s.r[3]=s.r[31];Restore(m,s,31,96);
}
void Append(GuestMemory& m,PpcBoundaryServices& services,Registers& s)
{
 Save(m,s,30,112);s.r[30]=s.r[4];s.r[6]=8;s.r[5]=4;s.r[4]=1;s.r[31]=s.r[3];s.lr=0x825f4214u;Grow(m,services,s);
 s.r[10]=m.ReadU32(Address(s.r[31]));s.r[11]=WordRotateMask(s.r[3],2,0xfffffffcu);s.r[11]+=s.r[10];Compare(s,s.r[11],0);
 if(!s.cr6.eq){s.r[10]=m.ReadU32(Address(s.r[30]));m.WriteU32(Address(s.r[11]),Address(s.r[10]));}
 s.r[11]=m.ReadU32(Address(s.r[31]+4u));s.r[3]=s.r[11]+std::uint64_t(-1);Restore(m,s,30,112);
}
}
bool Apply(GuestAddress entry,GuestMemory& m,PpcBoundaryServices& services,Registers& s)
{
 switch(entry){case 0x825f41e8u:Append(m,services,s);return true;case 0x822c42d8u:Grow(m,services,s);return true;case 0x8229f678u:Reallocate(m,services,s);return true;default:return false;}
}
}
