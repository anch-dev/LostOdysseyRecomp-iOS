#include "lo_semantics/memory_copy_context.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = memory_copy_context;
constexpr GuestAddress Destination = 0x10000u, Source = 0x20000u;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x90000u}}};
struct Case {const char* name;unsigned destination_offset,source_offset,count;};
constexpr std::array Cases{
 Case{"zero",0,0,0},Case{"short-byte-tail",1,3,5},Case{"caller-record24",0,0,24},
 Case{"leading-word",4,0,28},Case{"word-bulk-tail4",0,4,132},
 Case{"byte-assembled-bulk",0,1,260},Case{"aligned-two-lines",0,0,264},
 Case{"partial-line-word-route",8,4,257}};
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
PPCContext Initial(const Case& item)
{
 PPCContext c{};auto fields=Fields(c);for(unsigned i=0;i<32u;++i)fields[i]->u64=0x1122334400000000ull+i;
 c.r1.u64=0x1234567800080000ull;c.r3.u64=0x5566778800000000ull|(Destination+item.destination_offset);
 c.r4.u64=0x9988776600000000ull|(Source+item.source_offset);c.r5.u64=item.count;
 c.lr=0xabcdef0123456789ull;c.ctr.u64=0x123456789abcdef0ull;
 c.cr0.gt=1;c.cr1.eq=1;c.cr6.lt=1;c.cr7.gt=1;c.xer.so=1;c.xer.ca=0;return c;
}
void Seed(GuestMemory& m)
{ for(unsigned i=0;i<600u;++i)m.WriteU8(Source+i,static_cast<std::uint8_t>(i*17u+3u)); }
void Check(const Case& item)
{
 test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xa5);recovered.Fill(0xa5);
 auto left=original.Memory(),right=recovered.Memory();Seed(left);Seed(right);
 auto c=Initial(item);auto state=FromPpc(c);__imp__sub_82B7A0B0(c,original.Bytes());
 if(!family::Apply(0x82b7a0b0u,right,state))throw std::runtime_error("missing entry");
 const auto observed=FromPpc(c);
 if(!Same(observed,state)||!original.EqualCommitted(recovered)){
  for(unsigned i=0;i<32u;++i)if(observed.integer.r[i]!=state.integer.r[i])std::fprintf(stderr,"r%u %llX/%llX\n",i,(unsigned long long)observed.integer.r[i],(unsigned long long)state.integer.r[i]);
  std::fprintf(stderr,"CR1 %u%u%u%u/%u%u%u%u CR7 %u%u%u%u/%u%u%u%u\n",observed.cr1.lt,observed.cr1.gt,observed.cr1.eq,observed.cr1.so,state.cr1.lt,state.cr1.gt,state.cr1.eq,state.cr1.so,observed.cr7.lt,observed.cr7.gt,observed.cr7.eq,observed.cr7.so,state.cr7.lt,state.cr7.gt,state.cr7.eq,state.cr7.so);
  throw std::runtime_error(item.name);
 }
 for(unsigned i=0;i<item.count;++i)if(left.ReadU8(Destination+item.destination_offset+i)!=static_cast<std::uint8_t>((item.source_offset+i)*17u+3u))throw std::runtime_error("independent copied bytes");
 if(left.ReadU8(Destination+item.destination_offset+item.count)!=0xa5u||c.r3.u64!=(0x5566778800000000ull|(Destination+item.destination_offset)))throw std::runtime_error("copy extent/full return");
}
}
int main(){try{for(const auto& item:Cases)Check(item);
 std::printf("PASS memory-copy-context %zu actual PPC cases\n",Cases.size());
 std::puts("LIMIT selected GPR/CR0/1/6/7 and RAM; cache prefetch, access-width/fault/MMIO/concurrency/runtime open");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
