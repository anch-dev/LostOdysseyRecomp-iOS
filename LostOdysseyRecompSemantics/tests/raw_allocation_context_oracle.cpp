#include "lo_semantics/raw_allocation_context.h"
#include "semantic_oracle_support.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace
{
using namespace lo::semantic::gpu;
namespace family=raw_allocation_context;
constexpr GuestAddress HeapSlot=0x83245708u,RetrySlot=0x832d3aecu,Heap=0x20000u,ReplacementHeap=0x21000u,Error=0x30000u;
constexpr test::Region Regions[]={{0,0x100000u},{0x83245000u,0x1000u},{0x832d3000u,0x1000u}};
struct Case {const char* name;std::uint64_t bytes;unsigned failures;bool retry,missing;};
constexpr Case Cases[]={
 {"success-full-register",0xaabbccdd00000010ull,0,false,false},
 {"zero-low-word",0xaabbccdd00000000ull,0,false,false},
 {"failure-double-errno",16,1,false,false},
 {"handler-then-success",0xaabbccdd00000018ull,1,true,false},
 {"oversized-handler",0xaabbccddfffff001ull,0,true,false},
 {"missing-heap-live-reload",8,0,false,true}};
using Event=std::array<std::uint64_t,8>;using Events=std::vector<Event>;
const Case* active{};test::GuestWindow* original_window{};Events original_events;unsigned original_allocations=0,original_errors=0;
std::array<PPCRegister*,32> Fields(PPCContext& c)
{return {&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,&c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,&c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,&c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,&c.r30,&c.r31};}
family::Registers FromPpc(PPCContext& c)
{
 family::Registers s{};auto f=Fields(c);for(unsigned i=0;i<32;++i)s.r[i]=f[i]->u64;s.lr=c.lr;s.ctr=c.ctr.u64;s.xer_so=c.xer.so;s.xer_ca=c.xer.ca;
 s.cr0={c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};s.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};s.f0_bits=c.f0.u64;s.f1_bits=c.f1.u64;s.f13_bits=c.f13.u64;s.f30_bits=c.f30.u64;s.f31_bits=c.f31.u64;return s;
}
void ToPpc(PPCContext& c,const family::Registers& s)
{
 auto f=Fields(c);for(unsigned i=0;i<32;++i)f[i]->u64=s.r[i];c.lr=s.lr;c.ctr.u64=s.ctr;c.xer.so=s.xer_so;c.xer.ca=s.xer_ca;
 c.cr0.lt=s.cr0.lt;c.cr0.gt=s.cr0.gt;c.cr0.eq=s.cr0.eq;c.cr0.so=s.cr0.un;c.cr6.lt=s.cr6.lt;c.cr6.gt=s.cr6.gt;c.cr6.eq=s.cr6.eq;c.cr6.so=s.cr6.un;c.f0.u64=s.f0_bits;c.f1.u64=s.f1_bits;c.f13.u64=s.f13_bits;c.f30.u64=s.f30_bits;c.f31.u64=s.f31_bits;
}
bool Same(const family::Registers& a,const family::Registers& b)
{return a.r==b.r && a.lr==b.lr && a.ctr==b.ctr && a.cr0==b.cr0 && a.cr6==b.cr6 && a.xer_so==b.xer_so && a.xer_ca==b.xer_ca && a.f0_bits==b.f0_bits && a.f1_bits==b.f1_bits && a.f13_bits==b.f13_bits && a.f30_bits==b.f30_bits && a.f31_bits==b.f31_bits && a.cached_fp_control==b.cached_fp_control;}

void Boundary(GuestAddress target,GuestMemory& m,family::Registers& s,Events& events,unsigned& allocations,unsigned& errors)
{
 events.push_back({target,s.r[3],s.r[4],s.r[5],s.r[1],s.lr,s.r[30],s.r[31]});
 if(target==0x823accb0u)
 {
  const auto bytes=std::uint32_t(active->bytes)==0u?1ull:active->bytes;
  if(s.r[3]!=(active->missing?ReplacementHeap:Heap)||s.r[4]!=0u||s.r[5]!=bytes)throw std::runtime_error("independent heap arguments");
  s.r[3]=allocations++<active->failures?0xabcdef0100000000ull:0x1122334400050000ull;
 }
 else if(target==0x82b7fe68u)
 {if(s.r[3]!=active->bytes)throw std::runtime_error("saved full retry size");s.r[3]=1u;}
 else if(target==0x82b7fd78u)
 {s.r[3]=0x8877665500000000ull|(Error+4u*errors++);}
 else if(target==0x82b7fce0u)m.WriteU32(HeapSlot,ReplacementHeap);
 else if(target==0x82b7fc98u)
 {if(s.r[3]!=30u)throw std::runtime_error("missing heap report code");}
 else if(target==0x82b7bf20u)
 {if(s.r[3]!=255u)throw std::runtime_error("missing heap termination code");}
 else throw std::runtime_error("unexpected raw boundary");
 s.r[10]=0xaabbccdd00000077ull;s.r[13]=0x112233440000ff00ull;s.cr0={1,0,0,1};s.cr6={0,1,0,1};s.xer_ca=1;s.f1_bits=0x7ff8000000000033ull;
}
class Services final:public family::PpcBoundaryServices
{
public:Events events;unsigned allocations=0,errors=0;
 void CallDirect(GuestAddress t,GuestMemory& m,family::Registers& s)override{Boundary(t,m,s,events,allocations,errors);}
};
void Check(const Case& item)
{
 test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xcd);recovered.Fill(0xcd);auto left=original.Memory(),right=recovered.Memory();
 for(auto* memory:{&left,&right}){memory->WriteU32(HeapSlot,item.missing?0u:Heap);memory->WriteU32(RetrySlot,unsigned(item.retry));}
 PPCContext c{};auto f=Fields(c);for(unsigned i=0;i<32;++i)f[i]->u64=0x9988776600000000ull+i;
 c.r1.u64=0x1234567800080000ull;c.r3.u64=item.bytes;c.lr=0x88776655abcdef01ull;c.ctr.u64=0xaabbccdd55556666ull;c.xer.so=1;c.cr0.gt=1;c.cr0.so=1;c.cr6.gt=1;c.cr6.so=1;
 c.f0.u64=0x8000000000000000ull;c.f1.u64=0x3ff0000000000000ull;c.f13.u64=0x4000000000000000ull;c.f30.u64=0x4008000000000000ull;c.f31.u64=0x4010000000000000ull;
 auto state=FromPpc(c);active=&item;original_window=&original;original_events.clear();original_allocations=0;original_errors=0;__imp__sub_823ACBD0(c,original.Bytes());
 const bool oversized=std::uint32_t(item.bytes)>0xfffff000u;const auto expected=oversized?0ull:(item.failures&&!item.retry?0xabcdef0100000000ull:0x1122334400050000ull);
 const auto calls=oversized?2u:(item.missing?4u:(item.failures?3u:1u));
 if(c.r3.u64!=expected||original_events.size()!=calls||c.r1.u64!=0x1234567800080000ull||c.lr!=0xabcdef01u)throw std::runtime_error("original independent raw result");
 if(oversized||item.failures&&!item.retry)
 {if(left.ReadU32(Error)!=12u||(!oversized&&left.ReadU32(Error+4u)!=12u))throw std::runtime_error("original independent errno stores");}
 Services services;if(!family::Apply(0x823acbd0u,right,services,state)||!Same(FromPpc(c),state)||!original.EqualCommitted(recovered)||services.events!=original_events)
 {auto observed=FromPpc(c);std::fprintf(stderr,"FAIL %s RAM=%u events=%zu/%zu CR0=%u%u%u%u/%u%u%u%u\n",item.name,original.EqualCommitted(recovered),original_events.size(),services.events.size(),observed.cr0.lt,observed.cr0.gt,observed.cr0.eq,observed.cr0.un,state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.un);for(unsigned i=0;i<32;++i)if(observed.r[i]!=state.r[i])std::fprintf(stderr,"r%u=%llX/%llX\n",i,static_cast<unsigned long long>(observed.r[i]),static_cast<unsigned long long>(state.r[i]));throw std::runtime_error("raw context actual PPC mismatch");}
}
}
void OriginalRawSave28(PPCContext& c,unsigned char* base)
{auto f=Fields(c);for(unsigned i=28;i<=31;++i)PPC_STORE_U64(c.r1.u32-8u*(33u-i),f[i]->u64);PPC_STORE_U32(c.r1.u32-8u,c.r12.u32);}
void OriginalRawRestore28(PPCContext& c,unsigned char* base)
{auto f=Fields(c);for(unsigned i=28;i<=31;++i)f[i]->u64=PPC_LOAD_U64(c.r1.u32-8u*(33u-i));c.r12.u64=PPC_LOAD_U32(c.r1.u32-8u);c.lr=c.r12.u64;}
void OriginalRawBoundary(unsigned t,PPCContext& c,unsigned char*)
{auto state=FromPpc(c);auto memory=original_window->Memory();Boundary(t,memory,state,original_events,original_allocations,original_errors);ToPpc(c,state);}
int main()
{try{for(const auto& item:Cases)Check(item);std::puts("PASS raw-allocation-context 6 actual PPC cases; 0 new mappings");std::puts("LIMIT remaining heap/CRT/native internals, unexposed PPCContext fields, faults/MMIO/concurrency/runtime; missing-heap callbacks may not return live");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
