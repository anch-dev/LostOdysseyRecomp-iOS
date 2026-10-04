#include "lo_semantics/legacy_descriptor_record_rebind.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_record_rebind;
constexpr GuestAddress A = 0x10000u, B = 0x20000u, Tags = 0x30000u;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x90000u}}};
enum class Mode { TaggedEmpty, TaggedMiss, TaggedMatches, TaggedZeroMask, RangeEmpty, RangeThree };
struct Case { const char* name; Mode mode; };
constexpr std::array Cases{
 Case{"tagged-empty",Mode::TaggedEmpty},Case{"tagged-miss",Mode::TaggedMiss},
 Case{"tagged-two-matches",Mode::TaggedMatches},Case{"tagged-zero-mask",Mode::TaggedZeroMask},
 Case{"range-empty",Mode::RangeEmpty},Case{"range-three",Mode::RangeThree}};
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
        s.r[i] = i == 1u ? 0u : fields[i]->u64;
    s.sp = c.r1.u64; s.lr = c.lr; s.ctr = c.ctr.u64;
    s.xer_so = c.xer.so; s.xer_ca = c.xer.ca;
    s.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    return s;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }
bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr && a.ctr == b.ctr &&
        a.xer_so == b.xer_so && a.xer_ca == b.xer_ca &&
        SameCondition(a.cr0, b.cr0) && SameCondition(a.cr6, b.cr6);
}
PPCContext Initial(const Case& item)
{
 PPCContext c{};const auto fields=Fields(c);
 for(unsigned i=0;i<32u;++i)fields[i]->u64=0x1122334400000000ull+i;
 c.r1.u64=0x1234567800080000ull;c.r4.u64=0x5566778800000000ull|A;
 c.r5.u64=7u;c.r6.u64=1u;c.r7.u64=item.mode==Mode::RangeThree?4u:1u;
 c.r8.u64=0x8877665555443322ull;c.lr=0xabcdef0123456789ull;
 c.ctr.u64=0x5555666677778888ull;c.cr0.gt=1;c.cr6.lt=1;c.xer.so=1;c.xer.ca=1;
 return c;
}
void Seed(GuestMemory& m,const Case& item)
{
 const bool tagged=item.mode!=Mode::RangeEmpty&&item.mode!=Mode::RangeThree;
 const auto count=item.mode==Mode::TaggedEmpty?0u:3u;
 m.WriteU32(A,tagged?0x40000000u|(count<<3u):0u);
 m.WriteU32(A+24u,B);m.WriteU32(A+28u,Tags);
 for(unsigned i=0;i<5u;++i){m.WriteU32(B+i*8u,0xcccc0000u+i);m.WriteU32(B+i*8u+4u,0xabcdfffeu);}
 for(unsigned i=0;i<3u;++i){
  auto key=item.mode==Mode::TaggedMiss?8u:(i==1u?8u:7u);
  const auto mask=item.mode==Mode::TaggedZeroMask?0u:(i==0u?1u:8u);
  m.WriteU32(Tags+i*8u,(key<<4u)|mask);
 }
}
void Check(const Case& item)
{
 test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xa5);recovered.Fill(0xa5);
 auto left=original.Memory(),right=recovered.Memory();Seed(left,item);Seed(right,item);
 auto c=Initial(item);auto state=FromPpc(c);__imp__sub_83053AE8(c,original.Bytes());
 if(!family::Apply(0x83053ae8u,right,state))throw std::runtime_error("missing entry");
 const auto observed=FromPpc(c);
 if(!Same(observed,state)||!original.EqualCommitted(recovered)){
  for(unsigned i=0;i<32u;++i)if(observed.r[i]!=state.r[i])std::fprintf(stderr,"r%u %llX/%llX\n",i,(unsigned long long)observed.r[i],(unsigned long long)state.r[i]);
  throw std::runtime_error(item.name);
 }
 const bool matches=item.mode==Mode::TaggedMatches||item.mode==Mode::TaggedZeroMask;
 if(matches&&(left.ReadU32(B)!=0x55443322u||left.ReadU32(B+16u)!=0x55443322u||left.ReadU32(B+8u)!=0xcccc0001u))throw std::runtime_error("tagged path assertion");
 if(item.mode==Mode::RangeThree){
  if(left.ReadU32(B)!=0xcccc0000u)throw std::runtime_error("range start assertion");
  for(unsigned i=1u;i<4u;++i)if(left.ReadU32(B+i*8u)!=0x55443322u||((left.ReadU32(B+i*8u+4u)&0xfffcu)!=(i-1u)*4u))throw std::runtime_error("range write assertion");
 }
 if(item.mode==Mode::TaggedZeroMask&&(left.ReadU32(B+4u)&0xfffcu)!=0xfffcu)throw std::runtime_error("zero mask wrapping assertion");
}
}
int main(){try{for(const auto& item:Cases)Check(item);
 std::printf("PASS legacy-descriptor-record-rebind %zu actual PPC cases\n",Cases.size());
 std::puts("LIMIT ordinary descriptor RAM; faults/MMIO/concurrency/runtime open");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
