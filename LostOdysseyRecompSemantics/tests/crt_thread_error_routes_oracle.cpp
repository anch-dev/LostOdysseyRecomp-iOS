#include "lo_semantics/crt_thread_error_routes.h"
#include "semantic_oracle_support.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace
{
using namespace lo::semantic::gpu;
namespace family=crt_thread_error_routes;
constexpr GuestAddress Slot=0x83214d78u,Default=0x832d3adcu,Thread=0x10000u,Record=0x20000u,Stack=0x80000u;
constexpr test::Region Regions[]={{0,0x100000u},{0x83214000u,0x2000u},{0x832d3000u,0x1000u}};
struct Spec {const char* name;GuestAddress entry;std::uint64_t get;std::uint32_t marker;bool alias;};
constexpr Spec Cases[]={
 {"tls-present-full64",0x822ca128u,0x1234567800004000ull,0,false},
 {"tls-late-default",0x822ca128u,0,0,false},
 {"tls-low-word-zero",0x822ca128u,0xaabbccdd00000000ull,0,false},
 {"setter-full64",0x822ca188u,0,0,false},
 {"tail-marker-blocked",0x822ca180u,0,0x80000000u,false},
 {"tail-self-alias",0x822ca180u,0,0,true}};
using Event=std::array<std::uint64_t,5>;
using Events=std::vector<Event>;
const Spec* active{};test::GuestWindow* original_window{};Events original_events;
std::array<PPCRegister*,32> Fields(PPCContext& c)
{return {&c.r0,&c.r1,&c.r2,&c.r3,&c.r4,&c.r5,&c.r6,&c.r7,&c.r8,&c.r9,&c.r10,&c.r11,&c.r12,&c.r13,&c.r14,&c.r15,&c.r16,&c.r17,&c.r18,&c.r19,&c.r20,&c.r21,&c.r22,&c.r23,&c.r24,&c.r25,&c.r26,&c.r27,&c.r28,&c.r29,&c.r30,&c.r31};}
family::Registers FromPpc(PPCContext& c)
{
 family::Registers s{};auto f=Fields(c);for(unsigned i=0;i<32;++i)if(i!=1)s.r[i]=f[i]->u64;
 s.sp=c.r1.u64;s.lr=c.lr;s.ctr=c.ctr.u64;s.xer_so=c.xer.so;s.xer_ca=c.xer.ca;
 s.cr0={c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};s.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};return s;
}
void ToPpc(PPCContext& c,const family::Registers& s)
{
 auto f=Fields(c);for(unsigned i=0;i<32;++i)if(i!=1)f[i]->u64=s.r[i];c.r1.u64=s.sp;c.lr=s.lr;c.ctr.u64=s.ctr;c.xer.so=s.xer_so;c.xer.ca=s.xer_ca;
 c.cr0.lt=s.cr0.lt;c.cr0.gt=s.cr0.gt;c.cr0.eq=s.cr0.eq;c.cr0.so=s.cr0.so;
 c.cr6.lt=s.cr6.lt;c.cr6.gt=s.cr6.gt;c.cr6.eq=s.cr6.eq;c.cr6.so=s.cr6.so;
}
bool Same(const family::Registers& a,const family::Registers& b)
{
 return a.r==b.r && a.sp==b.sp && a.lr==b.lr && a.ctr==b.ctr && a.xer_so==b.xer_so && a.xer_ca==b.xer_ca &&
 a.cr0.lt==b.cr0.lt && a.cr0.gt==b.cr0.gt && a.cr0.eq==b.cr0.eq && a.cr0.so==b.cr0.so &&
 a.cr6.lt==b.cr6.lt && a.cr6.gt==b.cr6.gt && a.cr6.eq==b.cr6.eq && a.cr6.so==b.cr6.so;
}
void GetEffect(GuestMemory& m,family::Registers& s,Events& events)
{
 events.push_back({1,s.r[3],s.r[4],s.sp,s.lr});m.WriteU32(Slot,7);m.WriteU32(Default,0x46000u);
 s.r[3]=active->get;s.r[10]=0xfedcba9876543210ull;s.r[11]=0xaabbccdd00001234ull;s.r[13]=0x9988776600010000ull;
 s.cr6={1,0,0,1};s.xer_ca=1;
}
void SetEffect(GuestMemory& m,family::Registers& s,Events& events)
{
 events.push_back({2,s.r[3],s.r[4],s.sp,s.lr});m.WriteU32(0x50000u,static_cast<GuestAddress>(s.r[4]));m.WriteU32(Default,0x47000u);
 s.r[3]=0xfffffffffffffff0ull;s.r[10]=0x1234000077778888ull;s.cr6={0,1,0,1};
}
class Native final:public family::NativeServices
{
public:Events events;
 void KeTlsGetValue(GuestMemory& m,family::Registers& s)override{GetEffect(m,s,events);}
 void KeTlsSetValue(GuestMemory& m,family::Registers& s)override{SetEffect(m,s,events);}
};
void Seed(GuestMemory& m,const Spec& spec)
{m.WriteU32(Slot,9);m.WriteU32(Default,0x45000u);m.WriteU32(Thread+336u,spec.marker);m.WriteU32(Thread+256u,spec.alias?Thread+256u:Record);}
void Check(const Spec& spec)
{
 test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xad);recovered.Fill(0xad);auto left=original.Memory(),right=recovered.Memory();Seed(left,spec);Seed(right,spec);
 PPCContext c{};auto fields=Fields(c);for(unsigned i=0;i<32;++i)fields[i]->u64=0x1122334400000000ull+i;
 c.r1.u64=0x1234567800080000ull;c.r3.u64=0xaabbccdd11223344ull;c.r13.u64=0x9988776600010000ull;c.lr=0xabcdef0155667788ull;c.ctr.u64=0x8765432180000000ull;
 c.xer.so=1;c.xer.ca=0;c.cr0.lt=1;c.cr0.so=1;c.cr6.gt=1;c.cr6.so=1;
 auto state=FromPpc(c);active=&spec;original_window=&original;original_events.clear();
 switch(spec.entry){case 0x822ca128u:__imp__sub_822CA128(c,original.Bytes());break;case 0x822ca180u:__imp__sub_822CA180(c,original.Bytes());break;default:__imp__sub_822CA188(c,original.Bytes());break;}
 if(spec.entry==0x822ca128u)
 {
   const bool fallback=static_cast<GuestAddress>(spec.get)==0u;const auto expected=fallback?0x46000ull:spec.get;
   if(c.r3.u64!=expected || original_events.size()!=(fallback?2u:1u) || original_events[0][1]!=9u || original_events[0][4]!=0x822ca148u ||
      (fallback && (original_events[1][1]!=7u || original_events[1][2]!=0x46000u || original_events[1][4]!=0x822ca164u)))throw std::runtime_error("TLS original fixture path");
 }
 else
 {
   const auto address=(spec.alias?Thread+256u:Record)+352u;
   const auto expected=spec.marker?0xadadadadu:0x11223344u;
   if(left.ReadU32(address)!=expected || c.r3.u64!=0xaabbccdd11223344ull || !original_events.empty())throw std::runtime_error("setter original fixture path");
 }
 Native native;if(!family::Apply(spec.entry,right,native,state) || !Same(FromPpc(c),state) || !original.EqualCommitted(recovered) || native.events!=original_events)
 {std::fprintf(stderr,"FAIL %s RAM=%u calls=%zu/%zu\n",spec.name,original.EqualCommitted(recovered),original_events.size(),native.events.size());auto observed=FromPpc(c);for(unsigned i=0;i<32;++i)if(observed.r[i]!=state.r[i])std::fprintf(stderr,"r%u=%llX/%llX\n",i,static_cast<unsigned long long>(observed.r[i]),static_cast<unsigned long long>(state.r[i]));throw std::runtime_error("TLS/error actual PPC mismatch");}
}
}
void OriginalTlsGet(PPCContext& c,std::uint8_t*)
{auto s=FromPpc(c);auto m=original_window->Memory();GetEffect(m,s,original_events);ToPpc(c,s);}
void OriginalTlsSet(PPCContext& c,std::uint8_t*)
{auto s=FromPpc(c);auto m=original_window->Memory();SetEffect(m,s,original_events);ToPpc(c,s);}
int main()
{try{for(const auto& spec:Cases)Check(spec);std::printf("PASS crt-thread-error-routes 3 entries %zu actual PPC cases\n",std::size(Cases));std::puts("LIMIT native TLS internals, unselected inputs, faults/MMIO/concurrency/runtime remain open");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
