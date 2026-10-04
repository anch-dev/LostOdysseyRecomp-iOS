#include "lo_semantics/manager_init_context.h"
#include "semantic_oracle_support.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace
{
using namespace lo::semantic::gpu;
namespace family=manager_init_context;
constexpr GuestAddress Primary=0x20000u,Fallback=0x22000u,Replacement=0x24000u,Vtable=0x30000u,Slot=0x8330b608u;
constexpr test::Region Regions[]={{0,0x100000u},{0x8330b000u,0x1000u}};
struct Case {const char* name;bool primary_null,fallback_null,replace;std::uint64_t first;};
constexpr Case Cases[]={
 {"primary-success",false,false,false,1},
 {"fallback-full-low-zero",false,false,false,0xabcdef0000000000ull},
 {"primary-mapped-zero",true,false,false,1},
 {"fallback-mapped-zero",false,true,false,0},
 {"live-global-reload",false,false,true,0xffffffffu},
 {"fallback-live-global",false,false,true,0}};
using Event=std::array<std::uint64_t,8>;using Events=std::vector<Event>;
const Case* active{};test::GuestWindow* original_window{};Events original_events;
std::array<PPCRegister*,32> Fields(PPCContext& c)
{return {&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,&c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,&c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,&c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,&c.r30,&c.r31};}
family::Registers FromPpc(PPCContext& c)
{
 family::Registers s{};auto f=Fields(c);for(unsigned i=0;i<32;++i)s.r[i]=f[i]->u64;s.lr=c.lr;s.ctr=c.ctr.u64;s.xer_so=c.xer.so;s.xer_ca=c.xer.ca;
 s.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};s.f0_bits=c.f0.u64;s.f1_bits=c.f1.u64;s.f13_bits=c.f13.u64;s.f30_bits=c.f30.u64;s.f31_bits=c.f31.u64;return s;
}
void ToPpc(PPCContext& c,const family::Registers& s)
{
 auto f=Fields(c);for(unsigned i=0;i<32;++i)f[i]->u64=s.r[i];c.lr=s.lr;c.ctr.u64=s.ctr;c.xer.so=s.xer_so;c.xer.ca=s.xer_ca;
 c.cr6.lt=s.cr6.lt;c.cr6.gt=s.cr6.gt;c.cr6.eq=s.cr6.eq;c.cr6.so=s.cr6.un;c.f0.u64=s.f0_bits;c.f1.u64=s.f1_bits;c.f13.u64=s.f13_bits;c.f30.u64=s.f30_bits;c.f31.u64=s.f31_bits;
}
bool Same(const family::Registers& a,const family::Registers& b)
{return a.r==b.r && a.lr==b.lr && a.ctr==b.ctr && a.cr6==b.cr6 && a.xer_so==b.xer_so && a.xer_ca==b.xer_ca && a.f0_bits==b.f0_bits && a.f1_bits==b.f1_bits && a.f13_bits==b.f13_bits && a.f30_bits==b.f30_bits && a.f31_bits==b.f31_bits && a.cached_fp_control==b.cached_fp_control;}

void Boundary(bool direct,GuestAddress target,GuestMemory& m,family::Registers& s,Events& e)
{
 const auto input=s.r[3];e.push_back({std::uint64_t(direct),target,input,s.r[4],s.r[31],s.r[1],s.lr,m.ReadU32(Slot)});
 if(direct)
 {
  if(target==0x823acbd0u)
  {
   if(input==0x48decu)s.r[3]=active->primary_null?0x1234567800000000ull:0xaabbccdd00020000ull;
   else if(input==36u)s.r[3]=active->fallback_null?0x1234567800000000ull:0xaabbccdd00022000ull;
   else throw std::runtime_error("independent allocation size");
  }
  else if(target==0x827c5970u)
  {if(std::uint32_t(input)!=Primary)throw std::runtime_error("primary constructor receiver");s.r[3]=0x5566778800020000ull;}
  else if(target==0x827c4ed0u)
  {if(std::uint32_t(input)!=Fallback || s.r[4]!=(active->replace?Replacement:Primary))throw std::runtime_error("fallback live global argument");s.r[3]=0x778899aa00022000ull;}
  else throw std::runtime_error("unknown direct boundary");
 }
 else if(target==0x70000u)
 {
  if(std::uint32_t(input)!=(active->primary_null?0u:Primary))throw std::runtime_error("first virtual receiver");
  if(active->replace)m.WriteU32(Slot,Replacement);
  s.r[3]=active->first;
 }
 else if(target==0x70004u)
 {
  const auto expected=std::uint32_t(active->first)==0u?(active->fallback_null?0u:Fallback):(active->replace?Replacement:(active->primary_null?0u:Primary));
  if(std::uint32_t(input)!=expected)throw std::runtime_error("last virtual receiver");
  s.r[3]=0xabcdef0123456789ull;
 }
 else throw std::runtime_error("unknown virtual boundary");
 s.r[10]=0xaabbccdd01234567ull;s.r[4]=0x8877665500000042ull;s.cr6={1,0,0,1};s.xer_ca=1;s.f1_bits=0x7ff8000000000042ull;
}
class Services final:public family::PpcBoundaryServices
{
public:Events events;
 void CallDirect(GuestAddress t,GuestMemory& m,family::Registers& s)override{Boundary(true,t,m,s,events);}
 void CallVirtual(GuestAddress t,GuestMemory& m,family::Registers& s)override{Boundary(false,t,m,s,events);}
};
void Seed(GuestMemory& m)
{
 for(auto address:{0u,Primary,Fallback,Replacement})m.WriteU32(address,Vtable);
 m.WriteU32(Vtable+60u,0x70003u);m.WriteU32(Vtable+56u,0x70007u);m.WriteU32(Slot,0xdeadbeefu);
}
void Check(const Case& item)
{
 test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xcd);recovered.Fill(0xcd);auto left=original.Memory(),right=recovered.Memory();Seed(left);Seed(right);
 PPCContext c{};auto f=Fields(c);for(unsigned i=0;i<32;++i)f[i]->u64=0x9988776600000000ull+i;
 c.r1.u64=0x1234567800080000ull;c.lr=0x88776655abcdef01ull;c.ctr.u64=0xaabbccdd55556666ull;c.xer.so=1;c.xer.ca=0;c.cr6.gt=1;c.cr6.so=1;
 c.f0.u64=0x8000000000000000ull;c.f1.u64=0x3ff0000000000000ull;c.f13.u64=0x4000000000000000ull;c.f30.u64=0x4008000000000000ull;c.f31.u64=0x4010000000000000ull;
 auto state=FromPpc(c);active=&item;original_window=&original;original_events.clear();__imp__sub_827C5F38(c,original.Bytes());
 const bool fallback=std::uint32_t(item.first)==0u;const auto expected_calls=3u+unsigned(!item.primary_null)+(fallback?1u+unsigned(!item.fallback_null):0u);
 const auto expected_slot=fallback?(item.fallback_null?0u:Fallback):(item.replace?Replacement:(item.primary_null?0u:Primary));
 if(original_events.size()!=expected_calls || c.r3.u64!=0xabcdef0123456789ull || left.ReadU32(Slot)!=expected_slot || c.r1.u64!=0x1234567800080000ull || c.lr!=0xabcdef01u)throw std::runtime_error("original independent path result");
 Services services;family::Apply(right,services,state);
 if(!Same(FromPpc(c),state)||!original.EqualCommitted(recovered)||services.events!=original_events)
 {const auto observed=FromPpc(c);std::fprintf(stderr,"FAIL %s RAM=%u events=%zu/%zu\n",item.name,original.EqualCommitted(recovered),original_events.size(),services.events.size());for(unsigned i=0;i<32;++i)if(observed.r[i]!=state.r[i])std::fprintf(stderr,"r%u=%llX/%llX\n",i,static_cast<unsigned long long>(observed.r[i]),static_cast<unsigned long long>(state.r[i]));throw std::runtime_error("manager context actual PPC mismatch");}
}
}
void OriginalManagerDirect(unsigned target,PPCContext& c,unsigned char*)
{auto s=FromPpc(c);auto m=original_window->Memory();Boundary(true,target,m,s,original_events);ToPpc(c,s);}
void OriginalManagerVirtual(unsigned target,PPCContext& c,unsigned char*)
{auto s=FromPpc(c);auto m=original_window->Memory();Boundary(false,target,m,s,original_events);ToPpc(c,s);}
int main()
{try{for(const auto& item:Cases)Check(item);std::puts("PASS manager-init-context 6 actual PPC cases; 0 new mappings");std::puts("LIMIT allocation/constructor/virtual internals, unexposed PPCContext fields, faults/MMIO/concurrency/runtime remain open; null paths use mapped page zero");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
