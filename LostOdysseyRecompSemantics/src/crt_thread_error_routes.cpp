#include "lo_semantics/crt_thread_error_routes.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::crt_thread_error_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void InitializeTls(GuestMemory& memory, NativeServices& services, Registers& s)
{
    s.r[12]=s.lr;
    memory.WriteU32(Address(s.sp-8u),Address(s.r[12]));
    WriteU64(memory,Address(s.sp-24u),s.r[30]);
    WriteU64(memory,Address(s.sp-16u),s.r[31]);
    memory.WriteU32(Address(s.sp-112u),Address(s.sp));
    s.sp-=112u;
    s.r[30]=0xffffffff83210000ull;
    s.r[3]=memory.ReadU32(Address(s.r[30]+19832u));
    s.lr=0x822ca148u;
    services.KeTlsGetValue(memory,s);
    s.r[31]=s.r[3];
    const auto value=std::bit_cast<std::int32_t>(Address(s.r[31]));
    s.cr0={std::uint8_t(value<0),std::uint8_t(value>0),std::uint8_t(value==0),s.xer_so};
    if(s.cr0.eq)
    {
        s.r[11]=0xffffffff832d0000ull;
        s.r[3]=memory.ReadU32(Address(s.r[30]+19832u));
        s.r[31]=memory.ReadU32(Address(s.r[11]+15068u));
        s.r[4]=s.r[31];
        s.lr=0x822ca164u;
        services.KeTlsSetValue(memory,s);
    }
    s.r[3]=s.r[31];
    s.sp+=112u;
    s.r[12]=memory.ReadU32(Address(s.sp-8u));
    s.lr=s.r[12];
    s.r[30]=ReadU64(memory,Address(s.sp-24u));
    s.r[31]=ReadU64(memory,Address(s.sp-16u));
}
void StoreError(GuestMemory& memory, Registers& s)
{
    s.r[11]=memory.ReadU32(Address(s.r[13]+336u));
    s.cr6={0,std::uint8_t(s.r[11]!=0),std::uint8_t(s.r[11]==0),s.xer_so};
    if(!s.cr6.eq)return;
    s.r[11]=memory.ReadU32(Address(s.r[13]+256u));
    memory.WriteU32(Address(s.r[11]+352u),Address(s.r[3]));
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,NativeServices& services,Registers& s)
{
    switch(entry)
    {
    case 0x822ca128u:InitializeTls(memory,services,s);return true;
    case 0x822ca180u:case 0x822ca188u:StoreError(memory,s);return true;
    default:return false;
    }
}
}
