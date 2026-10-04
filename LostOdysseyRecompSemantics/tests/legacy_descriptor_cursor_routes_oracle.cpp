#include "lo_semantics/legacy_descriptor_cursor_routes.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"
#include <array>
#include <cstdio>
#include <stdexcept>
namespace
{
using namespace lo::semantic::gpu;
namespace family=legacy_descriptor_cursor_routes;
constexpr GuestAddress Cursor=0x11080u,Node=0x20000u,Child=0x21000u,Record=0x22000u,Stack=0x30000u,Owner=0x40000u,Sink=0x50000u;
constexpr std::array<test::Region,1> Regions{{{0u,0x90000u}}};
enum class Mode {Product,Branch,Repeat,Grow,Consume,Skip,Empty,Refill,Diagnostic};
struct Case {const char* name;GuestAddress entry;Mode mode;};
constexpr std::array Cases{
 Case{"product-full64",0x8302b2f8u,Mode::Product},
 Case{"branch-stack-copy24",0x8302b2f8u,Mode::Branch},
 Case{"repeat-stack-copy24",0x8302b2f8u,Mode::Repeat},
 Case{"mutable-stack-grow",0x8302b2f8u,Mode::Grow},
 Case{"consume-existing",0x8302b620u,Mode::Consume},
 Case{"skip-real-refill",0x83053770u,Mode::Skip},
 Case{"empty-frame",0x8302b5c8u,Mode::Empty},
 Case{"next-frame-refill",0x8302b5c8u,Mode::Refill},
 Case{"diagnostic-fallthrough",0x8302b2f8u,Mode::Diagnostic}};
std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers s{};
    const auto fields = Fields(c);
    for (unsigned i = 0u; i < 32u; ++i)
        s.integer.r[i] = i == 1u ? 0u : fields[i]->u64;
    s.integer.sp = c.r1.u64; s.integer.lr = c.lr; s.integer.ctr = c.ctr.u64;
    s.integer.xer_so = c.xer.so; s.integer.xer_ca = c.xer.ca;
    s.integer.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    s.integer.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    s.cr1={c.cr1.lt,c.cr1.gt,c.cr1.eq,c.cr1.so};
    s.cr7={c.cr7.lt,c.cr7.gt,c.cr7.eq,c.cr7.so};
    return s;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }
bool Same(const family::Registers& a,const family::Registers& b)
{
 const auto& x=a.integer;const auto& y=b.integer;
 return x.r==y.r&&x.sp==y.sp&&x.lr==y.lr&&x.ctr==y.ctr&&x.xer_so==y.xer_so&&x.xer_ca==y.xer_ca&&SameCondition(x.cr0,y.cr0)&&SameCondition(x.cr6,y.cr6)&&SameCondition(a.cr1,b.cr1)&&SameCondition(a.cr7,b.cr7);
}

struct Event {GuestAddress entry;std::uint64_t r3,r4,r5,sp,lr;bool operator==(const Event&)const=default;};
struct Services final:family::GuestBoundaryServices
{
 std::array<Event,8> events{};unsigned count=0;
 void Call(GuestAddress entry,GuestMemory& m,family::Registers& s) override
 {
  auto& g=s.integer;
  if(count==events.size())throw std::runtime_error("too many boundary events");
  events[count++]={entry,g.r[3],g.r[4],g.r[5],g.sp,g.lr};
  if(entry==0x83028fe0u)g.r[3]=Child;
  else if(entry==0x83026518u){g.r[3]=Stack;m.WriteU32(Cursor+24u,Stack+4u);m.WriteU32(Cursor+28u,0u);}
  else if(entry==0x82f99d98u)
  {
   // Model the selected diagnostic target returning through its caller frame.
   // The actual pinned 14-instruction prologue has no local epilogue.
   g.sp+=96u;g.lr=m.ReadU32(static_cast<GuestAddress>(g.sp-8u));
  }
  else throw std::runtime_error("unselected guest callback");
  g.r[9]=0x77665544000000e9ull;g.cr6={0,1,0,g.xer_so};
  m.WriteU32(Sink,entry);
 }
};
Services* active=nullptr;GuestMemory* original_memory=nullptr;
void ToPpc(const family::Registers& s,PPCContext& c)
{
 const auto fields=Fields(c);for(unsigned i=0;i<32u;++i)fields[i]->u64=i==1u?s.integer.sp:s.integer.r[i];
 c.lr=s.integer.lr;c.ctr.u64=s.integer.ctr;c.xer.so=s.integer.xer_so;c.xer.ca=s.integer.xer_ca;
 c.cr0.lt=s.integer.cr0.lt;c.cr0.gt=s.integer.cr0.gt;c.cr0.eq=s.integer.cr0.eq;c.cr0.so=s.integer.cr0.so;
 c.cr6.lt=s.integer.cr6.lt;c.cr6.gt=s.integer.cr6.gt;c.cr6.eq=s.integer.cr6.eq;c.cr6.so=s.integer.cr6.so;
 c.cr1.lt=s.cr1.lt;c.cr1.gt=s.cr1.gt;c.cr1.eq=s.cr1.eq;c.cr1.so=s.cr1.so;
 c.cr7.lt=s.cr7.lt;c.cr7.gt=s.cr7.gt;c.cr7.eq=s.cr7.eq;c.cr7.so=s.cr7.so;
}
PPCContext Initial(const Case& item)
{
 PPCContext c{};auto fields=Fields(c);for(unsigned i=0;i<32u;++i)fields[i]->u64=0x1122334400000000ull+i;
 c.r1.u64=0x1234567800080000ull;c.r3.u64=0x5566778800000000ull|Cursor;c.r4.u64=Node;
 if(item.mode==Mode::Skip)c.r4.u64=3u;
 c.lr=0xabcdef0123456789ull;c.ctr.u64=0x123456789abcdef0ull;
 c.cr0.gt=1;c.cr1.eq=1;c.cr6.lt=1;c.cr7.gt=1;c.xer.so=1;c.xer.ca=0;return c;
}
void Seed(GuestMemory& m,const Case& item)
{
 for(unsigned i=0;i<40u;i+=4u)m.WriteU32(Cursor+i,0u);
 m.WriteU32(Cursor,Child);m.WriteU32(Cursor+8,0x8000u);m.WriteU32(Cursor+12,0x9000u);m.WriteU32(Cursor+16,0xa000u);
 m.WriteU32(Cursor+24,Stack+4u);m.WriteU32(Cursor+28,Stack+8u);m.WriteU32(Cursor+4,2u);
 m.WriteU32(Stack+8,0u);m.WriteU32(Stack+12,4u);
 m.WriteU32(Child+4,9u);m.WriteU32(Child+20,0x42u);m.WriteU32(Child+28,2u);m.WriteU32(Child+32,3u);
 m.WriteU32(Node+4,9u);m.WriteU32(Node+28,0x80000001u);m.WriteU32(Node+32,3u);
 m.WriteU32(Cursor&0xfffff000u,Owner);m.WriteU32(Owner+148,0x1234u);
 if(item.mode==Mode::Branch||item.mode==Mode::Grow){m.WriteU32(Node+4,1u);m.WriteU32(Node+8,Child);m.WriteU32(Node+12,Record);}
 if(item.mode==Mode::Repeat){m.WriteU32(Node+4,8u);m.WriteU32(Node+16,Child);m.WriteU32(Node+20,5u);}
 if(item.mode==Mode::Grow)m.WriteU32(Cursor+28,1u);
 if(item.mode==Mode::Skip||item.mode==Mode::Refill)m.WriteU32(Cursor+4,0u);
 if(item.mode==Mode::Empty)m.WriteU32(Cursor+28,1u);
 if(item.mode==Mode::Diagnostic){m.WriteU32(Node+28,0u);m.WriteU32(Node+32,0u);}
}
void Check(const Case& item)
{
 test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xa5);recovered.Fill(0xa5);
 auto left=original.Memory(),right=recovered.Memory();Seed(left,item);Seed(right,item);
 auto c=Initial(item);auto state=FromPpc(c);Services a,b;active=&a;original_memory=&left;
 switch(item.entry){case 0x8302b2f8u:__imp__sub_8302B2F8(c,original.Bytes());break;case 0x8302b5c8u:__imp__sub_8302B5C8(c,original.Bytes());break;case 0x8302b620u:__imp__sub_8302B620(c,original.Bytes());break;case 0x83053770u:__imp__sub_83053770(c,original.Bytes());break;default:throw std::logic_error("case entry");}
 if(!family::Apply(item.entry,right,b,state))throw std::runtime_error("missing selected entry");
 const auto observed=FromPpc(c);
 if(!Same(observed,state)||!original.EqualCommitted(recovered)||a.count!=b.count||a.events!=b.events)
 {
  for(unsigned i=0;i<32u;++i)if(observed.integer.r[i]!=state.integer.r[i])std::fprintf(stderr,"r%u %llX/%llX\n",i,(unsigned long long)observed.integer.r[i],(unsigned long long)state.integer.r[i]);
  std::fprintf(stderr,"sp %llX/%llX lr %llX/%llX events %u/%u\n",(unsigned long long)observed.integer.sp,(unsigned long long)state.integer.sp,(unsigned long long)observed.integer.lr,(unsigned long long)state.integer.lr,a.count,b.count);
  throw std::runtime_error(item.name);
 }
 if(item.mode==Mode::Product&&left.ReadU32(Cursor+4)!=0x80000003u)throw std::runtime_error("independent product low word");
 if(item.mode==Mode::Branch||item.mode==Mode::Repeat||item.mode==Mode::Grow)
 {
  const bool repeat=item.mode==Mode::Repeat;
  if(left.ReadU32(Stack+8)!=1u||left.ReadU32(Stack+16)!=(repeat?Child:Record)||left.ReadU8(Stack+20)!=(repeat?1u:0u)||left.ReadU32(Stack+24)!=(repeat?3u:0u)||left.ReadU32(Stack+28)!=0x9000u||left.ReadU32(Stack+32)!=0x8000u||left.ReadU32(Stack+36)!=0xa000u||left.ReadU32(Cursor+4)!=6u)throw std::runtime_error("independent stack record24");
 }
 if(item.mode==Mode::Consume&&(left.ReadU32(Cursor+4)!=1u||c.r3.u64!=0x42u))throw std::runtime_error("independent consume");
 if(item.mode==Mode::Skip&&(left.ReadU32(Cursor+4)!=3u||a.count!=1u))throw std::runtime_error("independent skip/refill");
 if(item.mode==Mode::Diagnostic&&(a.count!=5u||c.r1.u64!=Initial(item).r1.u64-144u))throw std::runtime_error("diagnostic continuation frame");
}
}
void OriginalSave28(PPCContext& c,std::uint8_t*)
{for(unsigned i=28u;i<32u;++i)recovery_abi::WriteU64(*original_memory,static_cast<GuestAddress>(c.r1.u64-8u*(33u-i)),Fields(c)[i]->u64);original_memory->WriteU32(c.r1.u32-8u,c.r12.u32);}
void OriginalRestore28(PPCContext& c,std::uint8_t*)
{for(unsigned i=28u;i<32u;++i)Fields(c)[i]->u64=recovery_abi::ReadU64(*original_memory,c.r1.u32-8u*(33u-i));c.r12.u64=original_memory->ReadU32(c.r1.u32-8u);c.lr=c.r12.u64;}
void OriginalBoundary(std::uint32_t entry,PPCContext& c,std::uint8_t*)
{auto state=FromPpc(c);active->Call(entry,*original_memory,state);ToPpc(state,c);}
int main(){try{for(const auto& item:Cases)Check(item);std::printf("PASS legacy-descriptor-cursor-routes %zu actual PPC cases\n",Cases.size());std::puts("LIMIT next-frame/stack-grow/diagnostic-target guest boundaries modeled; faults/MMIO/concurrency/runtime open");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
